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
#include <sys/param.h>

#include <xbps_api.h>
#include "defs.h"

struct check_reqby_data {
	prop_dictionary_t pkgd;
	prop_array_t pkgd_reqby;
	const char *pkgname;
	const char *pkgver;
	bool pkgdb_update;
	bool pkgd_reqby_alloc;
};

static int
check_reqby_pkg_cb(prop_object_t obj, void *arg, bool *done)
{
	struct check_reqby_data *crd = arg;
	prop_array_t curpkg_rdeps, provides;
	prop_dictionary_t curpkg_propsd;
	prop_string_t curpkgver;
	const char *curpkgn;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &curpkgn);
	/* skip same pkg */
	if (strcmp(curpkgn, crd->pkgname) == 0)
		return 0;

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
	if (curpkg_rdeps == NULL) {
		/* package has no rundeps, skip */
		prop_object_release(curpkg_propsd);
		return 0;
	}
	/*
	 * Check for pkgpattern match with real packages...
	 */
	if (!xbps_match_pkgdep_in_array(curpkg_rdeps, crd->pkgver)) {
		/*
		 * ... otherwise check if package provides any virtual
		 * package and is matched against any object in
		 * run_depends.
		 */
		provides = prop_dictionary_get(obj, "provides");
		if (provides == NULL) {
			/* doesn't provide any virtual pkg */
			prop_object_release(curpkg_propsd);
			return 0;
		}
		if (!xbps_match_any_virtualpkg_in_rundeps(curpkg_rdeps,
		    provides)) {
			/* doesn't match any virtual pkg */
			prop_object_release(curpkg_propsd);
			return 0;
		}
	}
	crd->pkgd_reqby = prop_dictionary_get(crd->pkgd, "requiredby");
	curpkgver = prop_dictionary_get(curpkg_propsd, "pkgver");
	if (crd->pkgd_reqby != NULL) {
		/*
		 * Now check that current pkgver has been registered into
		 * its requiredby array.
		 */
		if (xbps_match_string_in_array(crd->pkgd_reqby,
		    prop_string_cstring_nocopy(curpkgver))) {
			/*
			 * Current package already requires our package,
			 * this is good so skip it.
			 */
			prop_object_release(curpkg_propsd);
			return 0;
		}
	} else {
		/*
		 * Missing requiredby array object, create it.
		 */
		crd->pkgd_reqby = prop_array_create();
		if (crd->pkgd_reqby == NULL) {
			prop_object_release(curpkg_propsd);
			return -1;
		}
		crd->pkgd_reqby_alloc = true;
	}
	/*
	 * Added pkgdep into pkg's requiredby array.
	 */
	if (!prop_array_add(crd->pkgd_reqby, curpkgver)) {
		prop_object_release(curpkg_propsd);
		return -1;
	}
	printf("%s: added missing requiredby entry for %s.\n",
	    crd->pkgname, prop_string_cstring_nocopy(curpkgver));
	prop_object_release(curpkg_propsd);
	crd->pkgdb_update = true;

	return 0;
}

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for missing reverse dependencies (aka requiredby)
 * 	  entries in pkg's pkgdb dictionary.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */
int
check_pkg_requiredby(const char *pkgname, void *arg, bool *pkgdb_update)
{
	struct check_reqby_data crd;
	int rv;

	crd.pkgd = arg;
	crd.pkgd_reqby = NULL;
	crd.pkgd_reqby_alloc = false;
	crd.pkgname = pkgname;
	crd.pkgdb_update = false;
	prop_dictionary_get_cstring_nocopy(crd.pkgd, "pkgver", &crd.pkgver);

	rv = xbps_pkgdb_foreach_pkg_cb(check_reqby_pkg_cb, &crd);
	*pkgdb_update = crd.pkgdb_update;

	if (crd.pkgdb_update) {
		if (!prop_dictionary_set(crd.pkgd, "requiredby", crd.pkgd_reqby))
			rv = -1;
		if (crd.pkgd_reqby_alloc)
			prop_object_release(crd.pkgd_reqby);
	}
	if (rv != 0)
		*pkgdb_update = false;

	return rv;
}
