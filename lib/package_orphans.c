/*-
 * Copyright (c) 2009-2015 Juan Romero Pardines.
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
xbps_find_pkg_orphans(struct xbps_handle *xhp, xbps_array_t orphans_user UNUSED)
{
	xbps_array_t rdeps, reqby, array = NULL;
	xbps_dictionary_t pkgd, deppkgd;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *curpkgver, *deppkgver, *reqbydep;
	bool automatic = false;
	unsigned int i, cnt, reqbycnt;

	if (xbps_pkgdb_init(xhp) != 0)
		return NULL;
	if ((array = xbps_array_create()) == NULL)
		return NULL;

	/*
	 * Add all packages specified by the client.
	 */
	for (i = 0; i < xbps_array_count(orphans_user); i++) {
		xbps_array_get_cstring_nocopy(orphans_user, i, &curpkgver);
		pkgd = xbps_pkgdb_get_pkg(xhp, curpkgver);
		if (pkgd == NULL)
			continue;
		xbps_array_add(array, pkgd);
	}
	if (orphans_user)
		goto add_orphans;

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);
	/*
	 * First pass: track pkgs that were installed manually and
	 * without reverse dependencies.
	 */
	while ((obj = xbps_object_iterator_next(iter))) {
		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		/*
		 * Skip packages that were not installed automatically.
		 */
		automatic = false;
		xbps_dictionary_get_bool(pkgd, "automatic-install", &automatic);
		if (!automatic)
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &curpkgver);
		reqby = xbps_pkgdb_get_pkg_revdeps(xhp, curpkgver);
		if (xbps_array_count(reqby) == 0) {
			/*
			 * Add packages with empty revdeps.
			 */
			xbps_array_add(array, pkgd);
			continue;
		}
	}
	xbps_object_iterator_release(iter);

add_orphans:
	for (i = 0; i < xbps_array_count(array); i++) {
		pkgd = xbps_array_get(array, i);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &curpkgver);
		rdeps = xbps_pkgdb_get_pkg_fulldeptree(xhp, curpkgver);
		for (unsigned int x = 0; x < xbps_array_count(rdeps); x++) {
			cnt = 0;
			xbps_array_get_cstring_nocopy(rdeps, x, &deppkgver);
			if (xbps_find_pkg_in_array(array, deppkgver, NULL))
				continue;
			deppkgd = xbps_pkgdb_get_pkg(xhp, deppkgver);
			automatic = false;
			xbps_dictionary_get_bool(deppkgd, "automatic-install", &automatic);
			if (!automatic)
				continue;
			reqby = xbps_pkgdb_get_pkg_revdeps(xhp, deppkgver);
			if (reqby == NULL)
				continue;
			reqbycnt = xbps_array_count(reqby);
			for (unsigned int j = 0; j < reqbycnt; j++) {
				xbps_array_get_cstring_nocopy(reqby, j, &reqbydep);
				if (xbps_find_pkg_in_array(array, reqbydep, NULL))
					cnt++;
			}
			if (cnt == reqbycnt)
				xbps_array_add(array, deppkgd);
		}
	}

	return array;
}
