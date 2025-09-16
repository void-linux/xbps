/*-
 * Copyright (c) 2012-2020 Juan Romero Pardines.
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

#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xbps.h"
#include "xbps_api_impl.h"

/**
 * @file lib/pkgdb.c
 * @brief Package database handling routines
 * @defgroup pkgdb Package database handling functions
 *
 * Functions to manipulate the main package database plist file (pkgdb).
 *
 * The following image shown below shows the proplib structure used
 * by the main package database plist:
 *
 * @image html images/xbps_pkgdb_dictionary.png
 *
 * Legend:
 *  - <b>Salmon filled box</b>: \a pkgdb plist internalized.
 *  - <b>White filled box</b>: mandatory objects.
 *  - <b>Grey filled box</b>: optional objects.
 *  - <b>Green filled box</b>: possible value set in the object, only one
 *    of them is set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e array, bool, integer, string,
 * dictionary.
 */

int
xbps_pkgdb_lock(struct xbps_handle *xhp)
{
	char path[PATH_MAX];
	mode_t prev_umask;
	int r = 0;

	if (access(xhp->rootdir, W_OK) == -1 && errno != ENOENT) {
		return xbps_error_errno(errno,
		    "failed to check whether the root directory is writable: "
		    "%s: %s\n",
		    xhp->rootdir, strerror(errno));
	}

	if (xbps_path_join(path, sizeof(path), xhp->metadir, "lock", (char *)NULL) == -1) {
		return xbps_error_errno(errno,
		    "failed to create lockfile path: %s\n", strerror(errno));
	}

	prev_umask = umask(022);

	/* if metadir does not exist, create it */
	if (access(xhp->metadir, R_OK|X_OK) == -1) {
		if (errno != ENOENT) {
			umask(prev_umask);
			return xbps_error_errno(errno,
			    "failed to check access to metadir: %s: %s\n",
			    xhp->metadir, strerror(-r));
		}
		if (xbps_mkpath(xhp->metadir, 0755) == -1 && errno != EEXIST) {
			umask(prev_umask);
			return xbps_error_errno(errno,
			    "failed to create metadir: %s: %s\n",
			    xhp->metadir, strerror(errno));
		}
	}

	xhp->lock_fd = open(path, O_CREAT|O_WRONLY|O_CLOEXEC, 0664);
	if (xhp->lock_fd  == -1) {
		return xbps_error_errno(errno,
		    "failed to create lock file: %s: %s\n", path,
		    strerror(errno));
	}
	umask(prev_umask);

	if (flock(xhp->lock_fd, LOCK_EX|LOCK_NB) == -1) {
		if (errno != EWOULDBLOCK)
			goto err;
		xbps_warn_printf("package database locked, waiting...\n");
	}

	if (flock(xhp->lock_fd, LOCK_EX) == -1) {
err:
		close(xhp->lock_fd);
		xhp->lock_fd = -1;
		return xbps_error_errno(errno, "failed to lock file: %s: %s\n",
		    path, strerror(errno));
	}

	return 0;
}

void
xbps_pkgdb_unlock(struct xbps_handle *xhp)
{
	if (xhp->lock_fd == -1)
		return;
	close(xhp->lock_fd);
	xhp->lock_fd = -1;
}

static int
pkgdb_map_vpkgs(struct xbps_handle *xhp)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	int r = 0;

	if (!xbps_dictionary_count(xhp->pkgdb))
		return 0;

	if (xhp->vpkgd == NULL) {
		xhp->vpkgd = xbps_dictionary_create();
		if (!xhp->vpkgd) {
			r = -errno;
			xbps_error_printf("failed to create dictionary\n");
			return r;
		}
	}

	/*
	 * This maps all pkgs that have virtualpkgs in pkgdb.
	 */
	iter = xbps_dictionary_iterator(xhp->pkgdb);
	if (!iter) {
		r = -errno;
		xbps_error_printf("failed to create iterator");
		return r;
	}

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t provides;
		xbps_dictionary_t pkgd;
		const char *pkgver = NULL;
		const char *pkgname = NULL;
		unsigned int cnt;

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		provides = xbps_dictionary_get(pkgd, "provides");
		cnt = xbps_array_count(provides);
		if (!cnt)
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);
		assert(pkgname);

		for (unsigned int i = 0; i < cnt; i++) {
			char vpkgname[XBPS_NAME_SIZE];
			const char *vpkg = NULL;
			xbps_dictionary_t providers;
			bool alloc = false;

			xbps_array_get_cstring_nocopy(provides, i, &vpkg);
			if (!xbps_pkg_name(vpkgname, sizeof(vpkgname), vpkg)) {
				xbps_warn_printf("%s: invalid provides: %s\n", pkgver, vpkg);
				continue;
			}

			providers = xbps_dictionary_get(xhp->vpkgd, vpkgname);
			if (!providers) {
				providers = xbps_dictionary_create();
				if (!providers) {
					r = -errno;
					xbps_error_printf("failed to create dictionary\n");
					goto out;
				}
				if (!xbps_dictionary_set(xhp->vpkgd, vpkgname, providers)) {
					r = -errno;
					xbps_error_printf("failed to set dictionary entry\n");
					xbps_object_release(providers);
					goto out;
				}
				alloc = true;
			}

			if (!xbps_dictionary_set_cstring(providers, vpkg, pkgname)) {
				r = -errno;
				xbps_error_printf("failed to set dictionary entry\n");
				if (alloc)
					xbps_object_release(providers);
				goto out;
			}
			if (alloc)
				xbps_object_release(providers);
			xbps_dbg_printf("[pkgdb] added vpkg %s for %s\n", vpkg, pkgname);
		}
	}
out:
	xbps_object_iterator_release(iter);
	return r;
}

static int
pkgdb_map_names(struct xbps_handle *xhp)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	int rv = 0;

	if (!xbps_dictionary_count(xhp->pkgdb))
		return 0;

	/*
	 * This maps all pkgs in pkgdb to have the "pkgname" string property.
	 * This way we do it once and not multiple times.
	 */
	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkgd;
		const char *pkgver;
		char pkgname[XBPS_NAME_SIZE] = {0};

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
			continue;
		}
		if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			break;
		}
		if (!xbps_dictionary_set_cstring(pkgd, "pkgname", pkgname)) {
			rv = EINVAL;
			break;
		}
	}
	xbps_object_iterator_release(iter);
	return rv;
}

int HIDDEN
xbps_pkgdb_init(struct xbps_handle *xhp)
{
	int rv;

	assert(xhp);

	if (xhp->pkgdb)
		return 0;

	if (!xhp->pkgdb_plist)
		xhp->pkgdb_plist = xbps_xasprintf("%s/%s", xhp->metadir, XBPS_PKGDB);

#if 0
	if ((rv = xbps_pkgdb_conversion(xhp)) != 0)
		return rv;
#endif


	if ((rv = xbps_pkgdb_update(xhp, false, true)) != 0) {
		if (rv != ENOENT)
			xbps_error_printf("failed to initialize pkgdb: %s\n", strerror(rv));
		return rv;
	}
	if ((rv = pkgdb_map_names(xhp)) != 0) {
		xbps_dbg_printf("[pkgdb] pkgdb_map_names %s\n", strerror(rv));
		return rv;
	}
	if ((rv = pkgdb_map_vpkgs(xhp)) != 0) {
		xbps_dbg_printf("[pkgdb] pkgdb_map_vpkgs %s\n", strerror(rv));
		return rv;
	}
	assert(xhp->pkgdb);
	xbps_dbg_printf("[pkgdb] initialized ok.\n");

	return 0;
}

int
xbps_pkgdb_update(struct xbps_handle *xhp, bool flush, bool update)
{
	xbps_dictionary_t pkgdb_storage;
	mode_t prev_umask;
	static int cached_rv;
	int rv = 0;

	if (cached_rv && !flush)
		return cached_rv;

	if (xhp->pkgdb && flush) {
		pkgdb_storage = xbps_dictionary_internalize_from_file(xhp->pkgdb_plist);
		if (pkgdb_storage == NULL ||
		    !xbps_dictionary_equals(xhp->pkgdb, pkgdb_storage)) {
			/* flush dictionary to storage */
			prev_umask = umask(022);
			if (!xbps_dictionary_externalize_to_file(xhp->pkgdb, xhp->pkgdb_plist)) {
				umask(prev_umask);
				return errno;
			}
			umask(prev_umask);
		}
		if (pkgdb_storage)
			xbps_object_release(pkgdb_storage);

		xbps_object_release(xhp->pkgdb);
		xhp->pkgdb = NULL;
		cached_rv = 0;
	}
	if (!update)
		return rv;

	/* update copy in memory */
	if ((xhp->pkgdb = xbps_dictionary_internalize_from_file(xhp->pkgdb_plist)) == NULL) {
		rv = errno;
		if (!rv)
			rv = EINVAL;

		if (rv == ENOENT)
			xhp->pkgdb = xbps_dictionary_create();
		else
			xbps_error_printf("cannot access to pkgdb: %s\n", strerror(rv));

		cached_rv = rv = errno;
	}

	return rv;
}

void HIDDEN
xbps_pkgdb_release(struct xbps_handle *xhp)
{
	assert(xhp);

	xbps_pkgdb_unlock(xhp);
	if (xhp->pkgdb)
		xbps_object_release(xhp->pkgdb);
	xbps_dbg_printf("[pkgdb] released ok.\n");
}

int
xbps_pkgdb_foreach_cb(struct xbps_handle *xhp,
		int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
		void *arg)
{
	xbps_array_t allkeys;
	int r;

	// XXX: this should be done before calling the function...
	if ((r = xbps_pkgdb_init(xhp)) != 0)
		return r > 0 ? -r : r;

	allkeys = xbps_dictionary_all_keys(xhp->pkgdb);
	assert(allkeys);
	r = xbps_array_foreach_cb(xhp, allkeys, xhp->pkgdb, fn, arg);
	xbps_object_release(allkeys);
	return r;
}

int
xbps_pkgdb_foreach_cb_multi(struct xbps_handle *xhp,
		int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
		void *arg)
{
	xbps_array_t allkeys;
	int r;

	// XXX: this should be done before calling the function...
	if ((r = xbps_pkgdb_init(xhp)) != 0)
		return r > 0 ? -r : r;

	allkeys = xbps_dictionary_all_keys(xhp->pkgdb);
	if (!allkeys)
		return xbps_error_oom();

	r = xbps_array_foreach_cb_multi(xhp, allkeys, xhp->pkgdb, fn, arg);
	xbps_object_release(allkeys);
	return r;
}

xbps_dictionary_t
xbps_pkgdb_get_pkg(struct xbps_handle *xhp, const char *pkg)
{
	xbps_dictionary_t pkgd;

	if (xbps_pkgdb_init(xhp) != 0)
		return NULL;

	pkgd = xbps_find_pkg_in_dict(xhp->pkgdb, pkg);
	if (!pkgd)
		errno = ENOENT;
	return pkgd;
}

xbps_dictionary_t
xbps_pkgdb_get_virtualpkg(struct xbps_handle *xhp, const char *vpkg)
{
	if (xbps_pkgdb_init(xhp) != 0)
		return NULL;

	return xbps_find_virtualpkg_in_dict(xhp, xhp->pkgdb, vpkg);
}

static void
generate_full_revdeps_tree(struct xbps_handle *xhp)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_dictionary_t vpkg_cache;

	if (xhp->pkgdb_revdeps)
		return;

	xhp->pkgdb_revdeps = xbps_dictionary_create();
	assert(xhp->pkgdb_revdeps);

	vpkg_cache = xbps_dictionary_create();
	assert(vpkg_cache);

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t rundeps;
		xbps_dictionary_t pkgd;
		const char *pkgver = NULL;

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		rundeps = xbps_dictionary_get(pkgd, "run_depends");
		if (!xbps_array_count(rundeps))
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		for (unsigned int i = 0; i < xbps_array_count(rundeps); i++) {
			xbps_array_t pkg;
			const char *pkgdep = NULL, *v;
			char curpkgname[XBPS_NAME_SIZE];
			bool alloc = false;

			xbps_array_get_cstring_nocopy(rundeps, i, &pkgdep);
			if ((!xbps_pkgpattern_name(curpkgname, sizeof(curpkgname), pkgdep)) &&
			    (!xbps_pkg_name(curpkgname, sizeof(curpkgname), pkgdep))) {
					abort();
			}

			/* TODO: this is kind of a workaround, to avoid calling vpkg_user_conf
			 * over and over again for the same packages which is slow. A better
			 * solution for itself vpkg_user_conf being slow should probably be
			 * implemented at some point.
			 */
			if (!xbps_dictionary_get_cstring_nocopy(vpkg_cache, curpkgname, &v)) {
				const char *vpkgname = vpkg_user_conf(xhp, curpkgname);
				if (vpkgname) {
					v = vpkgname;
				} else {
					v = curpkgname;
				}
				errno = 0;
				if (!xbps_dictionary_set_cstring_nocopy(vpkg_cache, curpkgname, v)) {
					xbps_error_printf("%s\n", strerror(errno ? errno : ENOMEM));
					abort();
				}
			}

			pkg = xbps_dictionary_get(xhp->pkgdb_revdeps, v);
			if (pkg == NULL) {
				alloc = true;
				pkg = xbps_array_create();
			}
			if (!xbps_match_string_in_array(pkg, pkgver)) {
				xbps_array_add_cstring_nocopy(pkg, pkgver);
				xbps_dictionary_set(xhp->pkgdb_revdeps, v, pkg);
			}
			if (alloc)
				xbps_object_release(pkg);
		}
	}
	xbps_object_iterator_release(iter);
	xbps_object_release(vpkg_cache);
}

xbps_array_t
xbps_pkgdb_get_pkg_revdeps(struct xbps_handle *xhp, const char *pkg)
{
	xbps_dictionary_t pkgd;
	const char *pkgver = NULL;
	char pkgname[XBPS_NAME_SIZE];

	if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkg)) == NULL)
		return NULL;

	generate_full_revdeps_tree(xhp);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) 
		return NULL;

	return xbps_dictionary_get(xhp->pkgdb_revdeps, pkgname);
}

xbps_array_t
xbps_pkgdb_get_pkg_fulldeptree(struct xbps_handle *xhp, const char *pkg)
{
	return xbps_get_pkg_fulldeptree(xhp, pkg, false);
}

xbps_dictionary_t
xbps_pkgdb_get_pkg_files(struct xbps_handle *xhp, const char *pkg)
{
	xbps_dictionary_t pkgd;
	const char *pkgver = NULL;
	char pkgname[XBPS_NAME_SIZE], plist[PATH_MAX];

	if (pkg == NULL)
		return NULL;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return NULL;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver))
		return NULL;

	snprintf(plist, sizeof(plist)-1, "%s/.%s-files.plist", xhp->metadir, pkgname);
	return xbps_plist_dictionary_from_file(plist);
}
