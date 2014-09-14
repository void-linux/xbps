/*-
 * Copyright (c) 2014 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include "xbps_api_impl.h"

/*
 * Verify shlib-{provides,requires} for packages in transaction.
 * This will catch cases where a package update would break its reverse
 * dependencies due to an incompatible SONAME bump:
 *
 * 	- foo-1.0 is installed and provides the 'libfoo.so.0' soname.
 * 	- foo-2.0 provides the 'libfoo.so.1' soname.
 * 	- baz-1.0 requires 'libfoo.so.0'.
 * 	- foo is updated to 2.0, hence baz-1.0 is now broken.
 *
 * Abort transaction if such case is found.
 */
static bool
shlib_trans_matched(struct xbps_handle *xhp, const char *pkgver, const char *shlib)
{
	xbps_array_t unsorted, shrequires;
	xbps_dictionary_t pkgd;
	const char *tract;
	char *pkgname;

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);

	unsorted = xbps_dictionary_get(xhp->transd, "unsorted_deps");
	if ((pkgd = xbps_find_pkg_in_array(unsorted, pkgname, NULL)) == NULL) {
		free(pkgname);
		return false;
	}
	free(pkgname);

	xbps_dictionary_get_cstring_nocopy(pkgd, "transaction", &tract);
	if (strcmp(tract, "update"))
		return false;

	shrequires = xbps_dictionary_get(pkgd, "shlib-requires");
	if (!shrequires)
		return false;

	return xbps_match_string_in_array(shrequires, shlib);
}

static bool
shlib_matched(struct xbps_handle *xhp, xbps_array_t mshlibs,
		const char *pkgver, const char *shlib)
{
	xbps_array_t revdeps;
	const char *shlibver;
	char *pkgname, *shlibname;
	bool found = true;

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);
	revdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkgname);
	free(pkgname);
	if (!revdeps)
		return true;

	shlibver = strchr(shlib, '.');
	shlibname = strdup(shlib);
	shlibname[strlen(shlib) - strlen(shlibver)] = '\0';
	assert(shlibname);

	/* Iterate over its revdeps and match the provided shlib */
	for (unsigned int i = 0; i < xbps_array_count(revdeps); i++) {
		xbps_array_t shrequires;
		xbps_dictionary_t pkgd;
		const char *rpkgver;

		xbps_array_get_cstring_nocopy(revdeps, i, &rpkgver);
		pkgd = xbps_pkgdb_get_pkg(xhp, rpkgver);
		shrequires = xbps_dictionary_get(pkgd, "shlib-requires");

		for (unsigned int x = 0; x < xbps_array_count(shrequires); x++) {
			const char *rshlib, *rshlibver;
			char *rshlibname;

			xbps_array_get_cstring_nocopy(shrequires, x, &rshlib);
			rshlibver = strchr(rshlib, '.');
			rshlibname = strdup(rshlib);
			rshlibname[strlen(rshlib) - strlen(rshlibver)] = '\0';

			if ((strcmp(shlibname, rshlibname) == 0) &&
			    (strcmp(shlibver, rshlibver))) {
				/*
				 * The shared library version did not match the
				 * installed pkg; find out if there's an update
				 * in the transaction with the matching version.
				 */
				if (!shlib_trans_matched(xhp, rpkgver, shlib)) {
					char *buf;
					/* shlib not matched */
					buf = xbps_xasprintf("%s breaks `%s' "
					    "(needs `%s%s', got '%s')",
					    pkgver, rpkgver, shlibname,
					    rshlibver, shlib);
					xbps_array_add_cstring(mshlibs, buf);
					free(buf);
					found = false;
				}
			}
			free(rshlibname);
		}
	}
	free(shlibname);

	return found;
}


bool HIDDEN
xbps_transaction_shlibs(struct xbps_handle *xhp)
{
	xbps_array_t unsorted, mshlibs;
	bool unmatched = false;

	mshlibs = xbps_dictionary_get(xhp->transd, "missing_shlibs");
	unsorted = xbps_dictionary_get(xhp->transd, "unsorted_deps");

	for (unsigned int i = 0; i < xbps_array_count(unsorted); i++) {
		xbps_array_t shprovides;
		xbps_object_t obj, pkgd;
		const char *pkgver, *tract;
		char *pkgname;

		obj = xbps_array_get(unsorted, i);
		/*
		 * If pkg does not have 'shlib-provides' obj, pass to next one.
		 */
		if ((shprovides = xbps_dictionary_get(obj, "shlib-provides")) == NULL)
			continue;
		/*
		 * Only process pkgs that are being updated.
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if (strcmp(tract, "update"))
			continue;

		/*
		 * If there's no change in shlib-provides, pass to next one.
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);
		pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
		assert(pkgd);
		free(pkgname);
		if (xbps_array_equals(shprovides, xbps_dictionary_get(pkgd, "shlib-provides")))
			continue;

		for (unsigned int x = 0; x < xbps_array_count(shprovides); x++) {
			const char *shlib;

			xbps_array_get_cstring_nocopy(shprovides, x, &shlib);
			/*
			 * Check that all shlibs provided by this pkg are used by
			 * its revdeps.
			 */
			if (!shlib_matched(xhp, mshlibs, pkgver, shlib))
				unmatched = true;
		}
	}
	return unmatched;
}
