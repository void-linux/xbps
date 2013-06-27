/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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

void HIDDEN
xbps_pkg_find_conflicts(struct xbps_handle *xhp,
			xbps_array_t unsorted,
			xbps_dictionary_t pkg_repod)
{
	xbps_array_t pkg_cflicts, trans_cflicts;
	xbps_dictionary_t pkgd;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *cfpkg, *repopkgver, *pkgver;
	char *pkgname, *repopkgname, *buf;

	pkg_cflicts = xbps_dictionary_get(pkg_repod, "conflicts");
	if (xbps_array_count(pkg_cflicts) == 0)
		return;

	trans_cflicts = xbps_dictionary_get(xhp->transd, "conflicts");
	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &repopkgver);
	repopkgname = xbps_pkg_name(repopkgver);
	assert(repopkgname);

	iter = xbps_array_iterator(pkg_cflicts);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		cfpkg = xbps_string_cstring_nocopy(obj);

		/*
		 * Check if current pkg conflicts with an installed package.
		 */
		if ((pkgd = xbps_pkgdb_get_pkg(xhp, cfpkg)) ||
		    (pkgd = xbps_pkgdb_get_virtualpkg(xhp, cfpkg))) {
			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &pkgver);
			pkgname = xbps_pkg_name(pkgver);
			assert(pkgname);
			if (strcmp(pkgname, repopkgname) == 0) {
				free(pkgname);
				continue;
			}
			free(pkgname);
			xbps_dbg_printf(xhp, "found conflicting installed "
			    "pkg %s with pkg in transaction %s\n", pkgver,
			    repopkgver);
			buf = xbps_xasprintf("CONFLICT: %s with "
			    "installed pkg %s", repopkgver, pkgver);
			xbps_array_add_cstring(trans_cflicts, buf);
			free(buf);
			continue;
		}
		/*
		 * Check if current pkg conflicts with any pkg in transaction.
		 */
		if ((pkgd = xbps_find_pkg_in_array(unsorted, cfpkg)) ||
		    (pkgd = xbps_find_virtualpkg_in_array(xhp, unsorted, cfpkg))) {
			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &pkgver);
			pkgname = xbps_pkg_name(pkgver);
			assert(pkgname);
			if (strcmp(pkgname, repopkgname) == 0) {
				free(pkgname);
				continue;
			}
			free(pkgname);
			xbps_dbg_printf(xhp, "found conflicting pkgs in "
			    "transaction %s <-> %s\n", pkgver, repopkgver);
			buf = xbps_xasprintf("CONFLICT: %s with "
			   "%s in transaction", repopkgver, pkgver);
			xbps_array_add_cstring(trans_cflicts, buf);
			free(buf);
			continue;
		}
	}
	xbps_object_iterator_release(iter);
	free(repopkgname);
}
