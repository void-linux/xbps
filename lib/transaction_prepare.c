/*-
 * Copyright (c) 2009-2020 Juan Romero Pardines.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/statvfs.h>

#include "xbps_api_impl.h"

/**
 * @file lib/transaction_prepare.c
 * @brief Transaction handling routines
 * @defgroup transaction Transaction handling functions
 *
 * The following image shows off the full transaction dictionary returned
 * by xbps_transaction_prepare().
 *
 * @image html images/xbps_transaction_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: The transaction dictionary.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *  - <b>Green bg box</b>: possible value set in the object, only one of them
 *    will be set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static int
compute_transaction_stats(struct xbps_handle *xhp)
{
	xbps_dictionary_t pkg_metad;
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	struct statvfs svfs;
	uint64_t rootdir_free_size, tsize, dlsize, instsize, rmsize;
	uint32_t inst_pkgcnt, up_pkgcnt, cf_pkgcnt, rm_pkgcnt, dl_pkgcnt;
	uint32_t hold_pkgcnt;

	inst_pkgcnt = up_pkgcnt = cf_pkgcnt = rm_pkgcnt = 0;
	hold_pkgcnt = dl_pkgcnt = 0;
	tsize = dlsize = instsize = rmsize = 0;

	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		const char *pkgver = NULL, *repo = NULL, *pkgname = NULL;
		bool preserve = false;
		xbps_trans_type_t ttype;
		/*
		 * Count number of pkgs to be removed, configured,
		 * installed and updated.
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgname);
		xbps_dictionary_get_cstring_nocopy(obj, "repository", &repo);
		xbps_dictionary_get_bool(obj, "preserve", &preserve);
		ttype = xbps_transaction_pkg_type(obj);

		if (ttype == XBPS_TRANS_REMOVE) {
			rm_pkgcnt++;
		} else if (ttype == XBPS_TRANS_CONFIGURE) {
			cf_pkgcnt++;
		} else if (ttype == XBPS_TRANS_INSTALL || ttype == XBPS_TRANS_REINSTALL) {
			inst_pkgcnt++;
		} else if (ttype == XBPS_TRANS_UPDATE) {
			up_pkgcnt++;
		} else if (ttype == XBPS_TRANS_HOLD) {
			hold_pkgcnt++;
		}

		if ((ttype != XBPS_TRANS_CONFIGURE) && (ttype != XBPS_TRANS_REMOVE) &&
		    (ttype != XBPS_TRANS_HOLD) &&
		    xbps_repository_is_remote(repo) && !xbps_binpkg_exists(xhp, obj)) {
			xbps_dictionary_get_uint64(obj, "filename-size", &tsize);
			tsize += 512;
			dlsize += tsize;
			dl_pkgcnt++;
			xbps_dictionary_set_bool(obj, "download", true);
		}
		if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
			continue;
		}
		/* installed_size from repo */
		if (ttype != XBPS_TRANS_REMOVE && ttype != XBPS_TRANS_HOLD &&
		    ttype != XBPS_TRANS_CONFIGURE) {
			xbps_dictionary_get_uint64(obj, "installed_size", &tsize);
			instsize += tsize;
		}
		/*
		 * If removing or updating a package without preserve,
		 * get installed_size from pkgdb instead.
		 */
		if (ttype == XBPS_TRANS_REMOVE ||
		   ((ttype == XBPS_TRANS_UPDATE) && !preserve)) {
			pkg_metad = xbps_pkgdb_get_pkg(xhp, pkgname);
			if (pkg_metad == NULL)
				continue;
			xbps_dictionary_get_uint64(pkg_metad,
			    "installed_size", &tsize);
			rmsize += tsize;
		}
	}
	xbps_object_iterator_release(iter);

	if (instsize > rmsize) {
		instsize -= rmsize;
		rmsize = 0;
	} else if (rmsize > instsize) {
		rmsize -= instsize;
		instsize = 0;
	} else {
		instsize = rmsize = 0;
	}

	if (!xbps_dictionary_set_uint32(xhp->transd,
				"total-install-pkgs", inst_pkgcnt))
		return EINVAL;
	if (!xbps_dictionary_set_uint32(xhp->transd,
				"total-update-pkgs", up_pkgcnt))
		return EINVAL;
	if (!xbps_dictionary_set_uint32(xhp->transd,
				"total-configure-pkgs", cf_pkgcnt))
		return EINVAL;
	if (!xbps_dictionary_set_uint32(xhp->transd,
				"total-remove-pkgs", rm_pkgcnt))
		return EINVAL;
	if (!xbps_dictionary_set_uint32(xhp->transd,
				"total-download-pkgs", dl_pkgcnt))
		return EINVAL;
	if (!xbps_dictionary_set_uint32(xhp->transd,
				"total-hold-pkgs", hold_pkgcnt))
		return EINVAL;
	if (!xbps_dictionary_set_uint64(xhp->transd,
				"total-installed-size", instsize))
		return EINVAL;
	if (!xbps_dictionary_set_uint64(xhp->transd,
				"total-download-size", dlsize))
		return EINVAL;
	if (!xbps_dictionary_set_uint64(xhp->transd,
				"total-removed-size", rmsize))
		return EINVAL;

	/* Get free space from target rootdir: return ENOSPC if there's not enough space */
	if (statvfs(xhp->rootdir, &svfs) == -1) {
		xbps_dbg_printf("%s: statvfs failed: %s\n", __func__, strerror(errno));
		return 0;
	}
	/* compute free space on disk */
	rootdir_free_size = svfs.f_bfree * svfs.f_bsize;

	if (!xbps_dictionary_set_uint64(xhp->transd,
				"disk-free-size", rootdir_free_size))
		return EINVAL;

	if (instsize > rootdir_free_size)
		return ENOSPC;

	return 0;
}

int HIDDEN
xbps_transaction_init(struct xbps_handle *xhp)
{
	xbps_array_t array;
	xbps_dictionary_t dict;

	if (xhp->transd != NULL)
		return 0;

	if ((xhp->transd = xbps_dictionary_create()) == NULL)
		return xbps_error_oom();

	if ((array = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	if (!xbps_dictionary_set(xhp->transd, "packages", array)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	xbps_object_release(array);

	if ((array = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	if (!xbps_dictionary_set(xhp->transd, "missing_deps", array)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	xbps_object_release(array);

	if ((array = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	if (!xbps_dictionary_set(xhp->transd, "missing_shlibs", array)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	xbps_object_release(array);

	if ((array = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	if (!xbps_dictionary_set(xhp->transd, "conflicts", array)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	xbps_object_release(array);

	if ((dict = xbps_dictionary_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	if (!xbps_dictionary_set(xhp->transd, "obsolete_files", dict)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	xbps_object_release(dict);

	if ((dict = xbps_dictionary_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	if (!xbps_dictionary_set(xhp->transd, "remove_files", dict)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return xbps_error_oom();
	}
	xbps_object_release(dict);

	return 0;
}

int
xbps_transaction_prepare(struct xbps_handle *xhp)
{
	xbps_array_t pkgs, edges;
	xbps_dictionary_t tpkgd;
	xbps_trans_type_t ttype;
	unsigned int i, cnt;
	int rv = 0;
	int r;
	bool all_on_hold = true;

	if ((rv = xbps_transaction_init(xhp)) != 0)
		return rv;

	if (xhp->transd == NULL)
		return ENXIO;

	/*
	 * Collect dependencies for pkgs in transaction.
	 */
	if ((edges = xbps_array_create()) == NULL)
		return ENOMEM;

	xbps_dbg_printf("%s: processing deps\n", __func__);
	/*
	 * The edges are also appended after its dependencies have been
	 * collected; the edges at the original array are removed later.
	 */
	pkgs = xbps_dictionary_get(xhp->transd, "packages");
	assert(xbps_object_type(pkgs) == XBPS_TYPE_ARRAY);
	cnt = xbps_array_count(pkgs);
	for (i = 0; i < cnt; i++) {
		xbps_dictionary_t pkgd;
		xbps_string_t str;

		pkgd = xbps_array_get(pkgs, i);
		str = xbps_dictionary_get(pkgd, "pkgver");
		ttype = xbps_transaction_pkg_type(pkgd);

		if (ttype == XBPS_TRANS_REMOVE || ttype == XBPS_TRANS_HOLD)
			continue;

		assert(xbps_object_type(str) == XBPS_TYPE_STRING);

		if (!xbps_array_add(edges, str)) {
			xbps_object_release(edges);
			return ENOMEM;
		}
		if ((rv = xbps_transaction_pkg_deps(xhp, pkgs, pkgd)) != 0) {
			xbps_object_release(edges);
			return rv;
		}
		if (!xbps_array_add(pkgs, pkgd)) {
			xbps_object_release(edges);
			return ENOMEM;
		}
	}
	/* ... remove dup edges at head */
	for (i = 0; i < xbps_array_count(edges); i++) {
		const char *pkgver = NULL;
		xbps_array_get_cstring_nocopy(edges, i, &pkgver);
		xbps_remove_pkg_from_array_by_pkgver(pkgs, pkgver);
	}
	xbps_object_release(edges);

	/*
	 * Do not perform any checks if XBPS_FLAG_DOWNLOAD_ONLY
	 * is set. We just need to download the archives (dependencies).
	 */
	if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY)
		goto out;

	/*
	 * If all pkgs in transaction are on hold, no need to check
	 * for anything else.
	 */
	xbps_dbg_printf("%s: checking on hold pkgs\n", __func__);
	for (i = 0; i < cnt; i++) {
		tpkgd = xbps_array_get(pkgs, i);
		if (xbps_transaction_pkg_type(tpkgd) != XBPS_TRANS_HOLD) {
			all_on_hold = false;
			break;
		}
	}
	if (all_on_hold)
		goto out;

	/*
	 * Check for packages to be replaced.
	 */
	xbps_dbg_printf("%s: checking replaces\n", __func__);
	if (!xbps_transaction_check_replaces(xhp, pkgs)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	/*
	 * Check if there are missing revdeps.
	 */
	xbps_dbg_printf("%s: checking revdeps\n", __func__);
	if (!xbps_transaction_check_revdeps(xhp, pkgs)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	if (xbps_dictionary_get(xhp->transd, "missing_deps")) {
		if (xhp->flags & XBPS_FLAG_FORCE_REMOVE_REVDEPS) {
			xbps_dbg_printf("[trans] continuing with broken reverse dependencies!");
		} else {
			return ENODEV;
		}
	}
	/*
	 * Check for package conflicts.
	 */
	xbps_dbg_printf("%s: checking conflicts\n", __func__);
	r = xbps_transaction_check_conflicts(xhp, pkgs);
	if (r < 0) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return -r;
	}
	if (xbps_dictionary_get(xhp->transd, "conflicts")) {
		return EAGAIN;
	}
	/*
	 * Check for unresolved shared libraries.
	 */
	xbps_dbg_printf("%s: checking shlibs\n", __func__);
	if (!xbps_transaction_check_shlibs(xhp, pkgs)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	if (xbps_dictionary_get(xhp->transd, "missing_shlibs")) {
		if (xhp->flags & XBPS_FLAG_FORCE_REMOVE_REVDEPS) {
			xbps_dbg_printf("[trans] continuing with unresolved shared libraries!");
		} else {
			return ENOEXEC;
		}
	}
out:
	/*
	 * Add transaction stats for total download/installed size,
	 * number of packages to be installed, updated, configured
	 * and removed to the transaction dictionary.
	 */
	xbps_dbg_printf("%s: computing stats\n", __func__);
	if ((rv = compute_transaction_stats(xhp)) != 0) {
		return rv;
	}
	/*
	 * Make transaction dictionary immutable.
	 */
	xbps_dictionary_make_immutable(xhp->transd);

	return 0;
}
