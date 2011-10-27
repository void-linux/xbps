/*-
 * Copyright (c) 2011 Juan Romero Pardines.
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
#include <assert.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps_api.h>
#include "defs.h"

static int
write_pkgd_to_regpkgdb(prop_dictionary_t pkgd,
		       prop_array_t regpkgs,
		       const char *pkgn)
{
	struct xbps_handle *xhp = xbps_handle_get();
	char *path;
	int rv;

	rv = xbps_array_replace_dict_by_name(regpkgs, pkgd, pkgn);
	if (rv != 0) {
		xbps_error_printf("%s: failed to replace pkgd: %s\n",
		    pkgn, strerror(rv));
		return -1;
	}
	if (!prop_dictionary_set(xhp->regpkgdb_dictionary,
	    "packages", regpkgs)) {
		xbps_error_printf("%s: failed to set new regpkgdb "
		    "packages array: %s", pkgn, strerror(errno));
		return -1;
	}
	path = xbps_xasprintf("%s/%s/%s",
	    prop_string_cstring_nocopy(xhp->rootdir),
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (path == NULL)
		return -1;

	if (!prop_dictionary_externalize_to_zfile(
	    xhp->regpkgdb_dictionary, path)) {
		xbps_error_printf("%s: failed to write regpkgdb plist:"
		    " %s\n", pkgn, strerror(errno));
		free(path);
		return -1;
	}
	free(path);

	return 0;
}

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for missing reverse dependencies (aka requiredby)
 * 	  entries in pkg's regpkgdb dictionary.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */
int
check_pkg_requiredby(prop_dictionary_t pkgd_regpkgdb,
		     prop_dictionary_t pkg_propsd,
		     prop_dictionary_t pkg_filesd)
{
	prop_array_t regpkgs, reqby, curpkg_rdeps, provides;
	prop_dictionary_t curpkg_propsd;
	prop_object_t obj;
	prop_string_t curpkgver;
	struct xbps_handle *xhp = xbps_handle_get();
	const char *curpkgn, *pkgname, *pkgver;
	size_t i;

	(void)pkg_propsd;
	(void)pkg_filesd;

	prop_dictionary_get_cstring_nocopy(pkgd_regpkgdb, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkgd_regpkgdb, "pkgver", &pkgver);

	regpkgs = prop_dictionary_get(xhp->regpkgdb_dictionary, "packages");

	for (i = 0; i < prop_array_count(regpkgs); i++) {
		obj = prop_array_get(regpkgs, i);
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &curpkgn);
		/* skip same pkg */
		if (strcmp(curpkgn, pkgname) == 0)
			continue;
		/*
		 * Internalize current pkg props dictionary from its
		 * installed metadata directory.
		 */
		curpkg_propsd =
		    xbps_dictionary_from_metadata_plist(curpkgn, XBPS_PKGPROPS);
		if (curpkg_propsd == NULL) {
			xbps_error_printf("%s: missing %s metadata file!\n",
			    curpkgn, XBPS_PKGPROPS);
			return -1;
		}
		curpkg_rdeps =
		    prop_dictionary_get(curpkg_propsd, "run_depends");
		if (prop_object_type(curpkg_rdeps) != PROP_TYPE_ARRAY) {
			/* package has no rundeps, skip */
			prop_object_release(curpkg_propsd);
			continue;
		}
		/*
		 * Check for pkgpattern match with real packages...
		 */
		if (!xbps_match_pkgpattern_in_array(curpkg_rdeps, pkgver)) {
			/*
			 * ... otherwise check if package provides any virtual
			 * package and is matched against any object in
			 * run_depends.
			 */
			provides = prop_dictionary_get(pkgd_regpkgdb, "provides");
			if (prop_object_type(provides) != PROP_TYPE_ARRAY) {
				/* doesn't provide any virtual pkg */
				prop_object_release(curpkg_propsd);
				continue;
			}
			if (!xbps_match_any_virtualpkg_in_rundeps(curpkg_rdeps,
			    provides)) {
				/* doesn't match any virtual pkg */
				prop_object_release(curpkg_propsd);
				continue;
			}
		}
		reqby = prop_dictionary_get(pkgd_regpkgdb, "requiredby");
		curpkgver = prop_dictionary_get(curpkg_propsd, "pkgver");
		if (prop_object_type(reqby) == PROP_TYPE_ARRAY) {
			/*
			 * Now check that current pkgver has been registered into
			 * its requiredby array.
			 */
			if (xbps_match_string_in_array(reqby,
			    prop_string_cstring_nocopy(curpkgver))) {
				/*
				 * Current package already requires our package,
				 * this is good so skip it.
				 */
				prop_object_release(curpkg_propsd);
				continue;
			}
		} else {
			/*
			 * Missing requiredby array object, create it.
			 */
			reqby = prop_array_create();
			if (reqby == NULL) {
				prop_object_release(curpkg_propsd);
				return -1;
			}
		}
		/*
		 * Replace current obj in regpkgdb and write new plist
		 * file to disk.
		 */
		prop_array_add(reqby, curpkgver);
		prop_dictionary_set(pkgd_regpkgdb, "requiredby", reqby);
		if (write_pkgd_to_regpkgdb(pkgd_regpkgdb, regpkgs, pkgname) != 0)
			return -1;

		printf("%s: added requiredby entry for %s.\n",
		    pkgver, prop_string_cstring_nocopy(curpkgver));
		prop_object_release(curpkg_propsd);
	}

	return 0;
}
