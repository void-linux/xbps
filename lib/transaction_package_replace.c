/*-
 * Copyright (c) 2011-2012 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

int HIDDEN
xbps_transaction_package_replace(prop_dictionary_t transd)
{
	prop_array_t replaces, instd_reqby, transd_unsorted;
	prop_dictionary_t instd, pkg_repod, reppkgd;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pattern, *pkgname, *curpkgname, *pkgver, *curpkgver;
	bool instd_auto, sr;
	size_t idx;

	assert(prop_object_type(transd) == PROP_TYPE_DICTIONARY);

	transd_unsorted = prop_dictionary_get(transd, "unsorted_deps");

	for (idx = 0; idx < prop_array_count(transd_unsorted); idx++) {
		pkg_repod = prop_array_get(transd_unsorted, idx);
		replaces = prop_dictionary_get(pkg_repod, "replaces");
		if (replaces == NULL || prop_array_count(replaces) == 0)
			continue;

		iter = prop_array_iterator(replaces);
		if (iter == NULL)
			return ENOMEM;

		while ((obj = prop_object_iterator_next(iter)) != NULL) {
			pattern = prop_string_cstring_nocopy(obj);
			assert(pattern != NULL);
			/*
			 * Find the installed package that matches the pattern
			 * to be replaced.
			 */
			instd = xbps_find_pkg_dict_installed(pattern, true);
			if (instd == NULL) {
				/*
				 * No package installed has been matched,
				 * try looking for a virtual package.
				 */
				instd = xbps_find_virtualpkg_dict_installed(
				    pattern, true);
				if (instd == NULL)
					continue;
			}
			prop_dictionary_get_cstring_nocopy(pkg_repod,
			    "pkgname", &pkgname);
			prop_dictionary_get_cstring_nocopy(pkg_repod,
			    "pkgver", &pkgver);
			prop_dictionary_get_cstring_nocopy(instd,
			    "pkgname", &curpkgname);
			prop_dictionary_get_cstring_nocopy(instd,
			    "pkgver", &curpkgver);
			xbps_dbg_printf("Package `%s' will be replaced by `%s', "
			    "matched with `%s'\n", curpkgver, pkgver, pattern);
			/*
			 * Check that we are not replacing the same package,
			 * due to virtual packages.
			 */
			if (strcmp(pkgname, curpkgname) == 0) {
				xbps_dbg_printf("replaced and new package "
				    "are equal (%s)\n", pkgname);
				prop_object_release(instd);
				continue;
			}
			instd_reqby = prop_dictionary_get(instd, "requiredby");
			instd_auto = false;
			prop_dictionary_get_bool(instd,
			    "automatic-install", &instd_auto);
			/*
			 * Package contains replaces="pkgpattern", but the
			 * package that should be replaced is also in the
			 * transaction and it's going to be updated.
			 */
			reppkgd = xbps_find_pkg_in_array_by_name(
			    transd_unsorted, curpkgname);
			if (reppkgd) {
				xbps_dbg_printf("found replaced pkg "
				    "in transaction\n");
				prop_dictionary_set_bool(instd,
				    "remove-and-update", true);
				if (instd_reqby &&
				    prop_array_count(instd_reqby)) {
					prop_dictionary_set(reppkgd,
					    "requiredby", instd_reqby);
				}
				prop_dictionary_set_bool(reppkgd,
				    "automatic-install", instd_auto);
				prop_dictionary_set_bool(reppkgd,
				    "skip-obsoletes", true);
				xbps_array_replace_dict_by_name(transd_unsorted,
				   reppkgd, curpkgname);
			}
			/*
			 * If new package is providing a virtual package to the
			 * package that we want to replace we should respect
			 * its requiredby and automatic-install objects, so copy
			 * them to the pkg's dictionary in transaction.
			 */
			if (xbps_match_virtual_pkg_in_dict(pkg_repod,
			    pattern, true) ||
			    xbps_match_virtual_pkg_in_dict(instd,
			    pkgname, false)) {
				if (instd_reqby &&
				    prop_array_count(instd_reqby)) {
					prop_dictionary_set(pkg_repod,
					    "requiredby", instd_reqby);
				}
				prop_dictionary_set_bool(pkg_repod,
				    "automatic-install", instd_auto);
			}
			/*
			 * Copy requiredby and automatic-install objects
			 * from replaced package into pkg's dictionary
			 * for "softreplace" packages.
			 */
			sr = false;
			prop_dictionary_get_bool(pkg_repod, "softreplace", &sr);
			if (sr) {
				if (instd_reqby &&
				    prop_array_count(instd_reqby)) {
					prop_dictionary_set(pkg_repod,
					    "requiredby", instd_reqby);
				}
				prop_dictionary_set_bool(pkg_repod,
				    "automatic-install", instd_auto);
				prop_dictionary_set_bool(instd,
				    "softreplace", true);
			}
			/*
			 * Add package dictionary into the transaction and mark
			 * it as to be "removed".
			 */
			prop_dictionary_set_cstring_nocopy(instd,
			    "transaction", "remove");
			if (!xbps_add_obj_to_array(transd_unsorted, instd)) {
				prop_object_release(instd);
				prop_object_iterator_release(iter);
				return EINVAL;
			}
		}
		prop_object_iterator_release(iter);
	}

	return 0;
}
