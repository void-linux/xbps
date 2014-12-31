/*-
 * Copyright (c) 2012-2014 Juan Romero Pardines.
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
#include <sys/stat.h>
#include <fcntl.h>

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
static int pkgdb_fd;

int
xbps_pkgdb_lock(struct xbps_handle *xhp)
{
	int rv;
	/*
	 * Use a mandatory file lock to only allow one writer to pkgdb,
	 * other writers will block.
	 */
	xhp->pkgdb_plist = xbps_xasprintf("%s/%s", xhp->metadir, XBPS_PKGDB);
	if (xbps_pkgdb_init(xhp) == ENOENT) {
		/* if metadir does not exist, create it */
		if (access(xhp->metadir, R_OK|X_OK) == -1) {
			if (errno != ENOENT)
				return errno;

			if (xbps_mkpath(xhp->metadir, 0755) == -1) {
				rv = errno;
				xbps_dbg_printf(xhp, "[pkgdb] failed to create metadir "
				    "%s: %s\n", xhp->metadir, strerror(rv));
				return rv;
			}
		}
		/* if pkgdb is unexistent, create it with an empty dictionary */
		xhp->pkgdb = xbps_dictionary_create();
		if (!xbps_dictionary_externalize_to_file(xhp->pkgdb, xhp->pkgdb_plist)) {
			rv = errno;
			xbps_dbg_printf(xhp, "[pkgdb] failed to create pkgdb "
			    "%s: %s\n", xhp->pkgdb_plist, strerror(rv));
			return rv;
		}
	}
	if ((pkgdb_fd = open(xhp->pkgdb_plist, O_CREAT|O_RDWR, 0664)) == -1) {
		rv = errno;
		xbps_dbg_printf(xhp, "[pkgdb] cannot open pkgdb for locking "
		    "%s: %s\n", xhp->pkgdb_plist, strerror(rv));
		free(xhp->pkgdb_plist);
		return rv;
	}
	if (lockf(pkgdb_fd, F_TLOCK, 0) == -1) {
		rv = errno;
		xbps_dbg_printf(xhp, "[pkgdb] cannot lock pkgdb: %s\n", strerror(rv));
		return rv;
	}
	return 0;
}

void
xbps_pkgdb_unlock(struct xbps_handle *xhp)
{
	(void)xbps_pkgdb_update(xhp, true);

	if (lockf(pkgdb_fd, F_ULOCK, 0) == -1)
		xbps_dbg_printf(xhp, "[pkgdb] failed to unlock pkgdb: %s\n", strerror(errno));

	(void)close(pkgdb_fd);
	pkgdb_fd = -1;
}

static int
pkgdb_map_vpkgs(struct xbps_handle *xhp)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	int rv = 0;

	if (!xbps_dictionary_count(xhp->pkgdb))
		return 0;

	if (xhp->vpkgd == NULL) {
		xhp->vpkgd = xbps_dictionary_create();
		assert(xhp->vpkgd);
	}
	/*
	 * This maps all pkgs that have virtualpkgs in pkgdb.
	 */
	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t provides;
		xbps_dictionary_t pkgd;
		const char *pkgver;
		char *pkgname;

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		provides = xbps_dictionary_get(pkgd, "provides");
		if (provides == NULL)
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		pkgname = xbps_pkg_name(pkgver);

		for (unsigned int i = 0; i < xbps_array_count(provides); i++) {
			const char *vpkg;

			xbps_array_get_cstring_nocopy(provides, i, &vpkg);
			if (!xbps_dictionary_set_cstring(xhp->vpkgd, vpkg, pkgname)) {
				xbps_dbg_printf(xhp, "%s: set_cstring vpkg "
				    "%s pkgname %s\n", __func__, vpkg, pkgname);
				rv = EINVAL;
			} else {
				xbps_dbg_printf(xhp, "[pkgdb] added vpkg %s for %s\n",
				    vpkg, pkgname);
			}
		}
		free(pkgname);
	}
	xbps_object_iterator_release(iter);
	return rv;
}

int HIDDEN
xbps_pkgdb_init(struct xbps_handle *xhp)
{
	int rv;

	assert(xhp != NULL);

	if (xhp->pkgdb_plist == NULL)
		xhp->pkgdb_plist = xbps_xasprintf("%s/%s", xhp->metadir, XBPS_PKGDB);

	if ((rv = xbps_pkgdb_conversion(xhp)) != 0)
		return rv;

	if (xhp->pkgdb != NULL)
		return 0;

	if ((rv = xbps_pkgdb_update(xhp, false)) != 0) {
		if (rv != ENOENT)
			xbps_dbg_printf(xhp, "[pkgdb] cannot internalize "
			    "pkgdb array: %s\n", strerror(rv));

		return rv;
	}
	if ((rv = pkgdb_map_vpkgs(xhp)) != 0) {
		xbps_dbg_printf(xhp, "[pkgdb] pkgdb_map_vpkgs %s\n", strerror(rv));
		return rv;
	}
	xbps_dbg_printf(xhp, "[pkgdb] initialized ok.\n");

	return 0;
}

int
xbps_pkgdb_update(struct xbps_handle *xhp, bool flush)
{
	xbps_dictionary_t pkgdb_storage;
	static int cached_rv;
	int rv = 0;

	if (cached_rv && !flush)
		return cached_rv;

	if (xhp->pkgdb && flush) {
		pkgdb_storage = xbps_dictionary_internalize_from_file(xhp->pkgdb_plist);
		if (pkgdb_storage == NULL ||
		    !xbps_dictionary_equals(xhp->pkgdb, pkgdb_storage)) {
			/* flush dictionary to storage */
			if (!xbps_dictionary_externalize_to_file(xhp->pkgdb, xhp->pkgdb_plist))
				return errno;
		}
		if (pkgdb_storage)
			xbps_object_release(pkgdb_storage);

		xbps_object_release(xhp->pkgdb);
		xhp->pkgdb = NULL;
		cached_rv = 0;
	}
	/* update copy in memory */
	if ((xhp->pkgdb = xbps_dictionary_internalize_from_file(xhp->pkgdb_plist)) == NULL) {
		if (errno == ENOENT)
			xhp->pkgdb = xbps_dictionary_create();
		else
			xbps_error_printf("cannot access to pkgdb: %s\n", strerror(errno));

		cached_rv = rv = errno;
	}

	return rv;
}

void HIDDEN
xbps_pkgdb_release(struct xbps_handle *xhp)
{
	assert(xhp != NULL);

	if (xhp->pkgdb == NULL)
		return;

	xbps_object_release(xhp->pkgdb);
	xhp->pkgdb = NULL;
	xbps_dbg_printf(xhp, "[pkgdb] released ok.\n");
}

int
xbps_pkgdb_foreach_cb(struct xbps_handle *xhp,
		int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
		void *arg)
{
	xbps_array_t allkeys;
	int rv;

	if ((rv = xbps_pkgdb_init(xhp)) != 0)
		return rv;

	allkeys = xbps_dictionary_all_keys(xhp->pkgdb);
	assert(allkeys);
	rv = xbps_array_foreach_cb(xhp, allkeys, xhp->pkgdb, fn, arg);
	xbps_object_release(allkeys);
	return rv;
}

int
xbps_pkgdb_foreach_cb_multi(struct xbps_handle *xhp,
		int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
		void *arg)
{
	xbps_array_t allkeys;
	int rv;

	if ((rv = xbps_pkgdb_init(xhp)) != 0)
		return rv;

	allkeys = xbps_dictionary_all_keys(xhp->pkgdb);
	assert(allkeys);
	rv = xbps_array_foreach_cb_multi(xhp, allkeys, xhp->pkgdb, fn, arg);
	xbps_object_release(allkeys);
	return rv;
}

xbps_dictionary_t
xbps_pkgdb_get_pkg(struct xbps_handle *xhp, const char *pkg)
{
	if (xbps_pkgdb_init(xhp) != 0)
		return NULL;

	return xbps_find_pkg_in_dict(xhp->pkgdb, pkg);
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

	if (xhp->pkgdb_revdeps)
		return;

	xhp->pkgdb_revdeps = xbps_dictionary_create();
	assert(xhp->pkgdb_revdeps);

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t rundeps;
		xbps_dictionary_t pkgd;

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		rundeps = xbps_dictionary_get(pkgd, "run_depends");
		if (!xbps_array_count(rundeps))
			continue;

		for (unsigned int i = 0; i < xbps_array_count(rundeps); i++) {
			xbps_array_t pkg;
			const char *pkgdep, *pkgver, *vpkgname;
			char *curpkgname;
			bool alloc = false;

			xbps_array_get_cstring_nocopy(rundeps, i, &pkgdep);
			curpkgname = xbps_pkgpattern_name(pkgdep);
			if (curpkgname == NULL)
				curpkgname = xbps_pkg_name(pkgdep);
			assert(curpkgname);
			vpkgname = vpkg_user_conf(xhp, curpkgname);
			if (vpkgname == NULL)
				vpkgname = curpkgname;

			pkg = xbps_dictionary_get(xhp->pkgdb_revdeps, vpkgname);
			if (pkg == NULL) {
				alloc = true;
				pkg = xbps_array_create();
			}
			xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
			if (!xbps_match_string_in_array(pkg, pkgver)) {
				xbps_array_add_cstring_nocopy(pkg, pkgver);
				xbps_dictionary_set(xhp->pkgdb_revdeps, vpkgname, pkg);
			}
			free(curpkgname);
			if (alloc)
				xbps_object_release(pkg);
		}
	}
	xbps_object_iterator_release(iter);
}

xbps_array_t
xbps_pkgdb_get_pkg_revdeps(struct xbps_handle *xhp, const char *pkg)
{
	xbps_array_t res;
	xbps_dictionary_t pkgd;
	const char *pkgver;
	char *pkgname;

	if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkg)) == NULL)
		return NULL;

	generate_full_revdeps_tree(xhp);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if ((pkgname = xbps_pkg_name(pkgver)) == NULL)
		return NULL;

	res = xbps_dictionary_get(xhp->pkgdb_revdeps, pkgname);
	free(pkgname);

	return res;
}

xbps_array_t
xbps_pkgdb_get_pkg_fulldeptree(struct xbps_handle *xhp, const char *pkg)
{
	return xbps_get_pkg_fulldeptree(xhp, pkg, false);
}

xbps_dictionary_t
xbps_pkgdb_get_pkg_files(struct xbps_handle *xhp, const char *pkg)
{
	xbps_dictionary_t pkgd, pkgfilesd;
	const char *pkgver;
	char *pkgname, *plist;

	if (pkg == NULL)
		return NULL;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return NULL;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);

	plist = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
	free(pkgname);
	pkgfilesd = xbps_dictionary_internalize_from_zfile(plist);
	free(plist);

	if (pkgfilesd == NULL) {
		xbps_dbg_printf(xhp, "[pkgdb] cannot read %s metadata: %s\n",
		    pkgver, strerror(errno));
		return NULL;
	}

	return pkgfilesd;
}
