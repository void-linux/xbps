/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

/**
 * @file lib/transaction_dictionary.c
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
	uint64_t tsize, dlsize, instsize, rmsize;
	uint32_t inst_pkgcnt, up_pkgcnt, cf_pkgcnt, rm_pkgcnt;
	const char *tract, *pkgver, *repo;

	inst_pkgcnt = up_pkgcnt = cf_pkgcnt = rm_pkgcnt = 0;
	tsize = dlsize = instsize = rmsize = 0;

	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		/*
		 * Count number of pkgs to be removed, configured,
		 * installed and updated.
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		xbps_dictionary_get_cstring_nocopy(obj, "repository", &repo);

		if (strcmp(tract, "configure") == 0) {
			cf_pkgcnt++;
			continue;
		} else if (strcmp(tract, "install") == 0) {
			inst_pkgcnt++;
		} else if (strcmp(tract, "update") == 0) {
			up_pkgcnt++;
		} else if (strcmp(tract, "remove") == 0) {
			rm_pkgcnt++;
		}

		tsize = 0;
		/*
		 * If removing or updating a package, get installed_size
		 * from pkg's metadata dictionary.
		 */
		if ((strcmp(tract, "remove") == 0) ||
		    (strcmp(tract, "update") == 0)) {
			char *pkgname;

			pkgname = xbps_pkg_name(pkgver);
			assert(pkgname);
			pkg_metad = xbps_pkgdb_get_pkg_metadata(xhp, pkgname);
			free(pkgname);
			if (pkg_metad == NULL)
				continue;
			xbps_dictionary_get_uint64(pkg_metad,
			    "installed_size", &tsize);
			rmsize += tsize;
		}
		if ((strcmp(tract, "install") == 0) ||
		    (strcmp(tract, "update") == 0)) {
			xbps_dictionary_get_uint64(obj,
			    "installed_size", &tsize);
			instsize += tsize;
			if (xbps_repository_is_remote(repo)) {
				xbps_dictionary_get_uint64(obj,
				    "filename-size", &tsize);
				dlsize += tsize;
				instsize += tsize;
			}
		}
	}
	xbps_object_iterator_release(iter);

	if (inst_pkgcnt && !xbps_dictionary_set_uint32(xhp->transd,
				"total-install-pkgs", inst_pkgcnt))
		return EINVAL;
	if (up_pkgcnt && !xbps_dictionary_set_uint32(xhp->transd,
				"total-update-pkgs", up_pkgcnt))
		return EINVAL;
	if (cf_pkgcnt && !xbps_dictionary_set_uint32(xhp->transd,
				"total-configure-pkgs", cf_pkgcnt))
		return EINVAL;
	if (rm_pkgcnt && !xbps_dictionary_set_uint32(xhp->transd,
				"total-remove-pkgs", rm_pkgcnt))
		return EINVAL;

	if (instsize)
		instsize -= rmsize;
	if (rmsize)
		rmsize -= instsize;

	if (!xbps_dictionary_set_uint64(xhp->transd,
				"total-installed-size", instsize))
		return EINVAL;
	if (!xbps_dictionary_set_uint64(xhp->transd,
				"total-download-size", dlsize))
		return EINVAL;
	if (!xbps_dictionary_set_uint64(xhp->transd,
				"total-removed-size", rmsize))
		return EINVAL;

	return 0;
}

int HIDDEN
xbps_transaction_init(struct xbps_handle *xhp)
{
	xbps_array_t unsorted, mdeps, conflicts;

	if (xhp->transd != NULL)
		return 0;

	if ((xhp->transd = xbps_dictionary_create()) == NULL)
		return ENOMEM;

        if ((unsorted = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return ENOMEM;
	}
	if (!xbps_dictionary_set(xhp->transd, "unsorted_deps", unsorted)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	if ((mdeps = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return ENOMEM;
	}
	if (!xbps_dictionary_set(xhp->transd, "missing_deps", mdeps)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}
	if ((conflicts = xbps_array_create()) == NULL) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return ENOMEM;
	}
	if (!xbps_dictionary_set(xhp->transd, "conflicts", conflicts)) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return EINVAL;
	}

	return 0;
}

int
xbps_transaction_prepare(struct xbps_handle *xhp)
{
	xbps_array_t pkgs, mdeps, conflicts;
	int rv = 0;

	if (xhp->transd == NULL)
		return ENXIO;

	/*
	 * If there are missing deps or revdeps bail out.
	 */
	xbps_transaction_revdeps(xhp);
	mdeps = xbps_dictionary_get(xhp->transd, "missing_deps");
	if (xbps_array_count(mdeps))
		return ENODEV;

	pkgs = xbps_dictionary_get(xhp->transd, "unsorted_deps");
	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++)
		xbps_pkg_find_conflicts(xhp, pkgs, xbps_array_get(pkgs, i));
	/*
	 * If there are package conflicts bail out.
	 */
	conflicts = xbps_dictionary_get(xhp->transd, "conflicts");
	if (xbps_array_count(conflicts))
		return EAGAIN;

	/*
	 * Check for packages to be replaced.
	 */
	if ((rv = xbps_transaction_package_replace(xhp)) != 0) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return rv;
	}
	/*
	 * Sort package dependencies if necessary.
	 */
	if ((rv = xbps_transaction_sort(xhp)) != 0) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return rv;
	}
	/*
	 * Add transaction stats for total download/installed size,
	 * number of packages to be installed, updated, configured
	 * and removed to the transaction dictionary.
	 */
	if ((rv = compute_transaction_stats(xhp)) != 0) {
		xbps_object_release(xhp->transd);
		xhp->transd = NULL;
		return rv;
	}
	/*
	 * The missing deps and conflicts arrays are not necessary anymore.
	 */
	xbps_dictionary_remove(xhp->transd, "missing_deps");
	xbps_dictionary_remove(xhp->transd, "conflicts");
	xbps_dictionary_make_immutable(xhp->transd);

	return 0;
}
