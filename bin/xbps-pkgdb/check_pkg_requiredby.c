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
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

static int
check_reqby_pkg_cb(struct xbps_handle *xhp,
		   prop_object_t obj,
		   void *arg,
		   bool *done)
{
	prop_dictionary_t pkgd = arg;
	prop_array_t curpkg_rdeps, provides, pkgd_reqby;
	prop_dictionary_t curpkg_propsd;
	prop_string_t curpkgver;
	const char *curpkgn, *pkgname, *pkgver;

	(void)done;

	prop_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &curpkgn);
	/* skip same pkg */
	if (strcmp(curpkgn, pkgname) == 0)
		return 0;

	/*
	 * Internalize current pkg props dictionary from its
	 * installed metadata directory.
	 */
	curpkg_propsd = xbps_metadir_get_pkgd(xhp, curpkgn);
	if (curpkg_propsd == NULL) {
		xbps_error_printf("%s: missing %s metadata file!\n",
		    curpkgn, XBPS_PKGPROPS);
		return -1;
	}
	curpkg_rdeps =
	    prop_dictionary_get(curpkg_propsd, "run_depends");
	if (curpkg_rdeps == NULL) {
		/* package has no rundeps, skip */
		return 0;
	}
	/*
	 * Check for pkgpattern match with real packages...
	 */
	if (!xbps_match_pkgdep_in_array(curpkg_rdeps, pkgver)) {
		/*
		 * ... otherwise check if package provides any virtual
		 * package and is matched against any object in
		 * run_depends.
		 */
		provides = prop_dictionary_get(pkgd, "provides");
		if (provides == NULL) {
			/* doesn't provide any virtual pkg */
			return 0;
		}
		if (!xbps_match_any_virtualpkg_in_rundeps(curpkg_rdeps,
		    provides)) {
			/* doesn't match any virtual pkg */
			return 0;
		}
	}
	pkgd_reqby = prop_dictionary_get(pkgd, "requiredby");
	curpkgver = prop_dictionary_get(curpkg_propsd, "pkgver");
	if (pkgd_reqby != NULL) {
		/*
		 * Now check that current pkgver has been registered into
		 * its requiredby array.
		 */
		if (xbps_match_string_in_array(pkgd_reqby,
		    prop_string_cstring_nocopy(curpkgver))) {
			/*
			 * Current package already requires our package,
			 * this is good so skip it.
			 */
			return 0;
		}
	} else {
		/*
		 * Missing requiredby array object, create it.
		 */
		pkgd_reqby = prop_array_create();
		assert(pkgd_reqby);
	}
	/*
	 * Added pkgdep into pkg's requiredby array.
	 */
	if (!prop_array_add(pkgd_reqby, curpkgver))
		return -1;

	printf("%s: added requiredby entry for %s.\n",
	    pkgver, prop_string_cstring_nocopy(curpkgver));

	return 0;
}

/*
 * Removes unused entries in pkg's requiredby array.
 */
static void
remove_stale_entries_in_reqby(struct xbps_handle *xhp, prop_dictionary_t pkgd)
{
	prop_array_t reqby;
	const char *pkgver;
	char *str;
	size_t i;

	reqby = prop_dictionary_get(pkgd, "requiredby");
	if (reqby == NULL || prop_array_count(reqby) == 0)
		return;

	prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	for (i = 0; i < prop_array_count(reqby); i++) {
		prop_array_get_cstring(reqby, i, &str);
		if ((pkgd = xbps_pkgdb_get_pkgd_by_pkgver(xhp, str)) != NULL)
			continue;

		if (!xbps_remove_string_from_array(xhp, reqby, str))
			fprintf(stderr, "%s: failed to remove %s from "
			    "requiredby!\n", pkgver, str);
		else
			printf("%s: removed stale entry in requiredby `%s'\n",
			    pkgver, str);
	}
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
check_pkg_requiredby(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	prop_dictionary_t pkgd = arg;
	int rv;

	(void)pkgname;

	/* missing reqby entries in pkgs */
	rv = xbps_pkgdb_foreach_cb(xhp, check_reqby_pkg_cb, pkgd);
	if (rv != 0)
		return rv;

	/* remove stale entries in pkg's reqby */
	remove_stale_entries_in_reqby(xhp, pkgd);

	return 0;
}
