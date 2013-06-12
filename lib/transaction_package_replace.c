/*-
 * Copyright (c) 2011-2013 Juan Romero Pardines.
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
xbps_transaction_package_replace(struct xbps_handle *xhp)
{
	prop_array_t replaces, unsorted;
	prop_dictionary_t instd, reppkgd, filesd;
	prop_object_t obj, obj2;
	prop_object_iterator_t iter;
	const char *pattern, *pkgver, *curpkgver;
	char *buf, *pkgname, *curpkgname;
	bool instd_auto, sr;
	unsigned int i;

	unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");

	for (i = 0; i < prop_array_count(unsorted); i++) {
		obj = prop_array_get(unsorted, i);
		replaces = prop_dictionary_get(obj, "replaces");
		if (replaces == NULL || prop_array_count(replaces) == 0)
			continue;

		iter = prop_array_iterator(replaces);
		assert(iter);

		while ((obj2 = prop_object_iterator_next(iter)) != NULL) {
			pattern = prop_string_cstring_nocopy(obj2);
			/*
			 * Find the installed package that matches the pattern
			 * to be replaced.
			 */
			if (((instd = xbps_pkgdb_get_pkg(xhp, pattern)) == NULL) &&
			    ((instd = xbps_pkgdb_get_virtualpkg(xhp, pattern)) == NULL))
				continue;

			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &pkgver);
			prop_dictionary_get_cstring_nocopy(instd,
			    "pkgver", &curpkgver);
			pkgname = xbps_pkg_name(pkgver);
			assert(pkgname);
			curpkgname = xbps_pkg_name(curpkgver);
			assert(curpkgver);
			/*
			 * Check that we are not replacing the same package,
			 * due to virtual packages.
			 */
			if (strcmp(pkgname, curpkgname) == 0) {
				free(pkgname);
				free(curpkgname);
				continue;
			}

			xbps_dbg_printf(xhp,
			    "Package `%s' will be replaced by `%s', "
			    "matched with `%s'\n", curpkgver, pkgver, pattern);
			instd_auto = false;
			prop_dictionary_get_bool(instd,
			    "automatic-install", &instd_auto);
			/*
			 * Package contains replaces="pkgpattern", but the
			 * package that should be replaced is also in the
			 * transaction and it's going to be updated.
			 */
			if ((reppkgd = xbps_find_pkg_in_array(unsorted, curpkgname))) {
				xbps_dbg_printf(xhp,
				    "found replaced pkg "
				    "in transaction\n");
				prop_dictionary_set_bool(instd,
				    "remove-and-update", true);
				prop_dictionary_set_bool(reppkgd,
				    "automatic-install", instd_auto);
				prop_dictionary_set_bool(reppkgd,
				    "skip-obsoletes", true);
				xbps_array_replace_dict_by_name(unsorted,
				   reppkgd, curpkgname);
			}
			/*
			 * If new package is providing a virtual package to the
			 * package that we want to replace we should respect
			 * the automatic-install object.
			 */
			if (xbps_match_virtual_pkg_in_dict(obj,
			    pattern, true) ||
			    xbps_match_virtual_pkg_in_dict(instd,
			    pkgname, false)) {
				prop_dictionary_set_bool(obj,
				    "automatic-install", instd_auto);
			}
			sr = false;
			prop_dictionary_get_bool(obj, "softreplace", &sr);
			if (sr) {
				prop_dictionary_set_bool(obj,
				    "automatic-install", instd_auto);
				prop_dictionary_set_bool(instd,
				    "softreplace", true);
				buf = xbps_xasprintf("%s/.%s.plist",
				    xhp->metadir, curpkgname);
				filesd = prop_dictionary_internalize_from_file(buf);
				free(buf);
				assert(filesd != NULL);
				buf = xbps_xasprintf("%s/.%s.plist",
				    xhp->metadir, pkgname);
				if (!prop_dictionary_externalize_to_file(filesd, buf)) {
					free(buf);
					prop_object_release(filesd);
					prop_object_iterator_release(iter);
					free(pkgname);
					free(curpkgname);
					return errno;
				}
				prop_object_release(filesd);
				free(buf);
			}
			/*
			 * Add package dictionary into the transaction and mark
			 * it as to be "removed".
			 */
			prop_dictionary_set_cstring_nocopy(instd,
			    "transaction", "remove");
			prop_array_add(unsorted, instd);
			free(pkgname);
			free(curpkgname);
		}
		prop_object_iterator_release(iter);
	}

	return 0;
}
