/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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
#include <unistd.h>
#include <libgen.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_transaction_package_replace(struct xbps_handle *xhp, xbps_array_t pkgs)
{
	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++) {
		xbps_array_t replaces;
		xbps_object_t obj, obj2;
		xbps_object_iterator_t iter;
		const char *pkgver;
		char pkgname[XBPS_NAME_SIZE];

		obj = xbps_array_get(pkgs, i);
		replaces = xbps_dictionary_get(obj, "replaces");
		if (replaces == NULL || xbps_array_count(replaces) == 0)
			continue;

		iter = xbps_array_iterator(replaces);
		assert(iter);

		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		if (!xbps_pkg_name(pkgname, XBPS_NAME_SIZE, pkgver)) {
			abort();
		}

		while ((obj2 = xbps_object_iterator_next(iter)) != NULL) {
			xbps_dictionary_t instd, reppkgd;
			const char *pattern = NULL, *curpkgver = NULL;
			char curpkgname[XBPS_NAME_SIZE];
			bool instd_auto = false, hold = false;

			pattern = xbps_string_cstring_nocopy(obj2);
			/*
			 * Find the installed package that matches the pattern
			 * to be replaced.
			 */
			if (((instd = xbps_pkgdb_get_pkg(xhp, pattern)) == NULL) &&
			    ((instd = xbps_pkgdb_get_virtualpkg(xhp, pattern)) == NULL))
				continue;

			xbps_dictionary_get_cstring_nocopy(instd,
			    "pkgver", &curpkgver);
			/* ignore pkgs on hold mode */
			if (xbps_dictionary_get_bool(instd, "hold", &hold) && hold)
				continue;

			if (!xbps_pkg_name(curpkgname, XBPS_NAME_SIZE, curpkgver)) {
				abort();
			}
			/*
			 * Check that we are not replacing the same package,
			 * due to virtual packages.
			 */
			if (strcmp(pkgname, curpkgname) == 0) {
				continue;
			}
			/*
			 * Make sure to not add duplicates.
			 */
			xbps_dictionary_get_bool(instd, "automatic-install", &instd_auto);
			reppkgd = xbps_find_pkg_in_array(pkgs, curpkgname, NULL);
			if (reppkgd) {
				const char *rpkgver = NULL, *tract = NULL;

				xbps_dictionary_get_cstring_nocopy(reppkgd,
				    "pkgver", &rpkgver);
				xbps_dictionary_get_cstring_nocopy(reppkgd,
				    "transaction", &tract);
				if (!strcmp(tract, "remove") || !strcmp(tract, "hold"))
					continue;
				if (!xbps_match_virtual_pkg_in_dict(reppkgd, pattern) &&
				    !xbps_pkgpattern_match(rpkgver, pattern))
					continue;
				/*
				 * Package contains replaces="pkgpattern", but the
				 * package that should be replaced is also in the
				 * transaction and it's going to be updated.
				 */
				xbps_dictionary_set_bool(reppkgd,
				    "automatic-install", instd_auto);
				xbps_dictionary_set_cstring_nocopy(reppkgd,
				    "transaction", "remove");
				xbps_dictionary_set_bool(reppkgd,
				    "replaced", true);
				xbps_array_replace_dict_by_name(pkgs,
				    reppkgd, curpkgname);
				xbps_dbg_printf(xhp,
				    "Package `%s' in transaction will be "
				    "replaced by `%s', matched with `%s'\n",
				    curpkgver, pkgver, pattern);
				continue;
			}
			/*
			 * If new package is providing a virtual package to the
			 * package that we want to replace we should respect
			 * the automatic-install object.
			 */
			if (xbps_match_virtual_pkg_in_dict(obj, pattern)) {
				xbps_dictionary_set_bool(obj,
				    "automatic-install", instd_auto);
			}
			xbps_dbg_printf(xhp,
			    "Package `%s' will be replaced by `%s', "
			    "matched with `%s'\n", curpkgver, pkgver, pattern);
			/*
			 * Add package dictionary into the transaction and mark
			 * it as to be "removed".
			 */
			xbps_dictionary_set_cstring_nocopy(instd,
			    "transaction", "remove");
			xbps_dictionary_set_bool(instd, "replaced", true);
			if (!xbps_array_add_first(pkgs, instd)) {
				xbps_object_iterator_release(iter);
				return EINVAL;
			}
		}
		xbps_object_iterator_release(iter);
	}

	return 0;
}
