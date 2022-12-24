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

#include "xbps_api_impl.h"

/**
 * @file lib/package_orphans.c
 * @brief Package orphans handling routines
 * @defgroup pkg_orphans Package orphans handling functions
 *
 * Functions to find installed package orphans.
 *
 * Package orphans were installed automatically by another package,
 * but currently no other packages are depending on.
 *
 * The following image shown below shows the registered packages database
 * dictionary (the array returned by xbps_find_pkg_orphans() will
 * contain a package dictionary per orphan found):
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

xbps_array_t
xbps_find_pkg_orphans(struct xbps_handle *xhp, xbps_array_t orphans_user)
{
	xbps_array_t array = NULL;
	xbps_object_t obj;
	xbps_object_iterator_t iter;

	if (xbps_pkgdb_init(xhp) != 0)
		return NULL;

	if ((array = xbps_array_create()) == NULL)
		return NULL;

	if (!orphans_user) {
		/* automatic mode (xbps-query -O, xbps-remove -o) */
		iter = xbps_dictionary_iterator(xhp->pkgdb);
		assert(iter);
		/*
		 * Iterate on pkgdb until no more orphans are found.
		 */
		for (;;) {
			bool added = false;
			while ((obj = xbps_object_iterator_next(iter))) {
				xbps_array_t revdeps;
				xbps_dictionary_t pkgd;
				unsigned int cnt = 0, revdepscnt = 0;
				const char *pkgver = NULL;
				bool automatic = false;

				pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
				if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
					/* _XBPS_ALTERNATIVES_ */
					continue;
				}
				xbps_dbg_printf(" %s checking %s\n", __func__, pkgver);
				xbps_dictionary_get_bool(pkgd, "automatic-install", &automatic);
				if (!automatic) {
					xbps_dbg_printf(" %s skipped (!automatic)\n", pkgver);
					continue;
				}
				if (xbps_find_pkg_in_array(array, pkgver, 0)) {
					xbps_dbg_printf(" %s orphan (queued)\n", pkgver);
					continue;
				}
				revdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkgver);
				revdepscnt = xbps_array_count(revdeps);

				if (revdepscnt == 0) {
					added = true;
					xbps_array_add(array, pkgd);
					xbps_dbg_printf(" %s orphan (automatic and !revdeps)\n", pkgver);
					continue;
				}
				/* verify all revdeps are seen */
				for (unsigned int i = 0; i < revdepscnt; i++) {
					const char *revdepver = NULL;

					xbps_array_get_cstring_nocopy(revdeps, i, &revdepver);
					if (xbps_find_pkg_in_array(array, revdepver, 0))
						cnt++;
				}
				if (cnt == revdepscnt) {
					added = true;
					xbps_array_add(array, pkgd);
					xbps_dbg_printf(" %s orphan (automatic and all revdeps)\n", pkgver);
				}

			}
			xbps_dbg_printf("orphans pkgdb iter: added %s\n", added ? "true" : "false");
			xbps_object_iterator_reset(iter);
			if (!added)
				break;
		}
		xbps_object_iterator_release(iter);

		return array;
	}

	/*
	 * Recursive removal mode (xbps-remove -R).
	 */
	for (unsigned int i = 0; i < xbps_array_count(orphans_user); i++) {
		xbps_dictionary_t pkgd;
		const char *pkgver = NULL;

		xbps_array_get_cstring_nocopy(orphans_user, i, &pkgver);
		pkgd = xbps_pkgdb_get_pkg(xhp, pkgver);
		if (pkgd == NULL)
			continue;
		xbps_array_add(array, pkgd);
	}

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		xbps_array_t rdeps;
		xbps_dictionary_t pkgd;
		const char *pkgver = NULL;
		unsigned int cnt = 0, reqbycnt = 0;

		pkgd = xbps_array_get(array, i);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		rdeps = xbps_pkgdb_get_pkg_fulldeptree(xhp, pkgver);
		if (xbps_array_count(rdeps) == 0) {
			continue;
		}

		xbps_dbg_printf(" processing rdeps for %s\n", pkgver);
		for (unsigned int x = 0; x < xbps_array_count(rdeps); x++) {
			xbps_array_t reqby;
			xbps_dictionary_t deppkgd;
			const char *deppkgver = NULL;
			bool automatic = false;

			cnt = 0;
			xbps_array_get_cstring_nocopy(rdeps, x, &deppkgver);
			if (xbps_find_pkg_in_array(array, deppkgver, 0)) {
				xbps_dbg_printf(" rdep %s already queued\n", deppkgver);
				continue;
			}
			deppkgd = xbps_pkgdb_get_pkg(xhp, deppkgver);
			xbps_dictionary_get_bool(deppkgd, "automatic-install", &automatic);
			if (!automatic) {
				xbps_dbg_printf(" rdep %s skipped (!automatic)\n", deppkgver);
				continue;
			}

			reqby = xbps_pkgdb_get_pkg_revdeps(xhp, deppkgver);
			reqbycnt = xbps_array_count(reqby);
			for (unsigned int j = 0; j < reqbycnt; j++) {
				const char *reqbydep = NULL;

				xbps_array_get_cstring_nocopy(reqby, j, &reqbydep);
				xbps_dbg_printf(" %s processing revdep %s\n", pkgver, reqbydep);
				if (xbps_find_pkg_in_array(array, reqbydep, 0))
					cnt++;
			}
			if (cnt == reqbycnt) {
				xbps_array_add(array, deppkgd);
				xbps_dbg_printf(" added %s orphan\n", deppkgver);
			}
		}
	}

	return array;
}
