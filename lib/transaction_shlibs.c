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
shlib_in_pkgdb(struct xbps_handle *xhp, const char *pkgver, const char *shlib)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	bool found = false;

	if (!xbps_dictionary_count(xhp->pkgdb))
		return found;

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t shprovides;
		xbps_dictionary_t pkgd, pkgmetad;
		const char *curpkgver;

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &curpkgver);
		pkgmetad = xbps_pkgdb_get_pkg_metadata(xhp, curpkgver);
		assert(pkgmetad);
		shprovides = xbps_dictionary_get(pkgmetad, "shlib-provides");
		if (!shprovides)
			continue;
		if (xbps_match_string_in_array(shprovides, shlib)) {
			/* shlib matched */
			xbps_dbg_printf(xhp, "[trans] %s requires `%s': "
			    "matched by `%s' (pkgdb)\n", pkgver, shlib, curpkgver);
			found = true;
			break;
		}
	}
	xbps_object_iterator_release(iter);
	return found;
}

static bool
shlib_in_transaction(struct xbps_handle *xhp, const char *pkgver, const char *shlib)
{
	xbps_array_t unsorted;

	unsorted = xbps_dictionary_get(xhp->transd, "unsorted_deps");
	for (unsigned int i = 0; i < xbps_array_count(unsorted); i++) {
		xbps_array_t shprovides;
		xbps_object_t obj;

		obj = xbps_array_get(unsorted, i);
		shprovides = xbps_dictionary_get(obj, "shlib-provides");
		if (!shprovides)
			continue;
		if (xbps_match_string_in_array(shprovides, shlib)) {
			/* shlib matched */
			const char *curpkgver;

			xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &curpkgver);
			xbps_dbg_printf(xhp, "[trans] %s requires `%s': "
			    "matched by `%s' (trans)\n", pkgver, shlib, curpkgver);
			return true;
		}
	}
	return false;
}

bool HIDDEN
xbps_transaction_shlibs(struct xbps_handle *xhp)
{
	xbps_array_t unsorted;
	bool unmatched = false;

	unsorted = xbps_dictionary_get(xhp->transd, "unsorted_deps");
	for (unsigned int i = 0; i < xbps_array_count(unsorted); i++) {
		xbps_array_t shrequires;
		xbps_object_t obj;
		const char *pkgver, *tract;

		obj = xbps_array_get(unsorted, i);
		/*
		 * Only process pkgs that are being installed or updated.
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if (strcmp(tract, "install") && strcmp(tract, "update"))
			continue;
		/*
		 * If pkg does not have 'shlib-requires' obj, pass to next one.
		 */
		if ((shrequires = xbps_dictionary_get(obj, "shlib-requires")) == NULL)
			continue;
		/*
		 * Check if all required shlibs are provided by:
		 * 	- an installed pkg
		 * 	- a pkg in the transaction
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		for (unsigned int x = 0; x < xbps_array_count(shrequires); x++) {
			const char *shlib;

			xbps_array_get_cstring_nocopy(shrequires, x, &shlib);
			if ((!shlib_in_pkgdb(xhp, pkgver, shlib)) &&
			    (!shlib_in_transaction(xhp, pkgver, shlib))) {
				xbps_dbg_printf(xhp, "[trans] %s: needs `%s' "
				    "not provided by any pkg!\n", pkgver, shlib);
				unmatched = true;
			}
		}
	}
	return unmatched;
}
