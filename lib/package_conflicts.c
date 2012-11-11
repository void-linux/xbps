/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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
xbps_pkg_find_conflicts(struct xbps_handle *xhp, prop_dictionary_t pkg_repod)
{
	prop_array_t pkg_cflicts, trans_cflicts;
	prop_dictionary_t pkgd;
	const char *cfpkg, *repopkgver, *pkgver;
	char *buf;
	size_t i;

	pkg_cflicts = prop_dictionary_get(pkg_repod, "conflicts");
	if (pkg_cflicts == NULL || prop_array_count(pkg_cflicts) == 0)
		return;

	trans_cflicts = prop_dictionary_get(xhp->transd, "conflicts");
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &repopkgver);

	for (i = 0; i < prop_array_count(pkg_cflicts); i++) {
		prop_array_get_cstring_nocopy(pkg_cflicts, i, &cfpkg);
		/*
		 * Check if current pkg conflicts with an installed package.
		 */
		if ((pkgd = xbps_pkgdb_get_pkgd(xhp, cfpkg, true))) {
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &pkgver);
			buf = xbps_xasprintf("%s conflicts with "
			    "installed pkg %s", repopkgver, pkgver);
			prop_array_add_cstring(trans_cflicts, buf);
			free(buf);
			continue;
		}
		/*
		 * Check if current pkg conflicts with any pkg in transaction.
		 */
		pkgd = xbps_find_pkg_in_dict_by_pattern(xhp, xhp->transd,
		    "unsorted_deps", cfpkg);
		if (pkgd != NULL) {
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &pkgver);
			buf = xbps_xasprintf("%s conflicts with "
			   "%s in transaction", repopkgver, pkgver);
			prop_array_add_cstring(trans_cflicts, buf);
			free(buf);
			continue;
		}
	}
}
