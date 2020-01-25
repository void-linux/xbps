/*-
 * Copyright (c) 2014-2016 Juan Romero Pardines.
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

static void
shlib_register(xbps_dictionary_t d, const char *shlib, const char *pkgver)
{
	xbps_array_t array;
	bool alloc = false;

	if ((array = xbps_dictionary_get(d, shlib)) == NULL) {
		alloc = true;
		array = xbps_array_create();
		xbps_dictionary_set(d, shlib, array);
	}
	if (!xbps_match_string_in_array(array, pkgver))
		xbps_array_add_cstring_nocopy(array, pkgver);
	if (alloc)
		xbps_object_release(array);
}

static xbps_dictionary_t
collect_shlibs(struct xbps_handle *xhp, xbps_array_t pkgs, bool req)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_dictionary_t d, pd;
	const char *pkgver;

	d = xbps_dictionary_create();
	assert(d);

	/* copy pkgdb to out temporary dictionary */
	pd = xbps_dictionary_copy(xhp->pkgdb);
	assert(pd);

	/*
	 * copy pkgs from transaction to our dictionary, overriding them
	 * if they were there from pkgdb.
	 */
	iter = xbps_array_iterator(pkgs);
	assert(iter);
	while ((obj = xbps_object_iterator_next(iter))) {
		const char *trans = NULL;
		char *pkgname = NULL;

		if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver))
			continue;

		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);

		/* ignore shlibs if pkg is on hold mode */
		if (xbps_dictionary_get_cstring_nocopy(obj, "transaction", &trans)) {
			if (!strcmp(trans, "hold")) {
				free(pkgname);
				continue;
			}
		}

		xbps_dictionary_set(pd, pkgname, obj);
		free(pkgname);
	}
	xbps_object_iterator_release(iter);

	/*
	 * iterate over our dictionary to collect shlib-{requires,provides}.
	 */
	iter = xbps_dictionary_iterator(pd);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t shobjs;
		xbps_dictionary_t pkgd;
		const char *trans;

		pkgd = xbps_dictionary_get_keysym(pd, obj);
		if (xbps_dictionary_get_cstring_nocopy(pkgd, "transaction", &trans)) {
			if (!strcmp(trans, "remove"))
				continue;
		}
		/*
		 * If pkg does not have the required obj, pass to next one.
		 */
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		shobjs = xbps_dictionary_get(pkgd,
				req ? "shlib-requires" : "shlib-provides");
		if (shobjs == NULL)
			continue;

		for (unsigned int i = 0; i < xbps_array_count(shobjs); i++) {
			const char *shlib = NULL;

			xbps_array_get_cstring_nocopy(shobjs, i, &shlib);
			xbps_dbg_printf(xhp, "%s: registering %s for %s\n",
			    pkgver, shlib, req ? "shlib-requires" : "shlib-provides");
			if (req)
				shlib_register(d, shlib, pkgver);
			else
				xbps_dictionary_set_cstring_nocopy(d, shlib, pkgver);
		}
	}
	xbps_object_iterator_release(iter);
	xbps_object_release(pd);
	return d;
}

bool HIDDEN
xbps_transaction_shlibs(struct xbps_handle *xhp, xbps_array_t pkgs, xbps_array_t mshlibs)
{
	xbps_object_t obj, obj2;
	xbps_object_iterator_t iter;
	xbps_dictionary_t shrequires, shprovides;
	bool unmatched = false;

	shrequires = collect_shlibs(xhp, pkgs, true);
	shprovides = collect_shlibs(xhp, pkgs, false);

	/* iterate over shlib-requires to find unmatched shlibs */
	iter = xbps_dictionary_iterator(shrequires);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t array;
		const char *pkgver = NULL, *shlib = NULL;
		char *buf;

		shlib = xbps_dictionary_keysym_cstring_nocopy(obj);
		xbps_dbg_printf(xhp, "%s: checking for `%s': ", __func__, shlib);
		if ((obj2 = xbps_dictionary_get(shprovides, shlib))) {
			xbps_dbg_printf_append(xhp, "provided by `%s'\n",
			    xbps_string_cstring_nocopy(obj2));
			continue;
		}
		xbps_dbg_printf_append(xhp, "not found\n");

		unmatched = true;
		array = xbps_dictionary_get_keysym(shrequires, obj);
		for (unsigned int i = 0; i < xbps_array_count(array); i++) {
			xbps_array_get_cstring_nocopy(array, i, &pkgver);
			buf = xbps_xasprintf("%s: broken, unresolvable "
			    "shlib `%s'", pkgver, shlib);
			xbps_array_add_cstring(mshlibs, buf);
			free(buf);
		}
		xbps_object_release(array);
	}
	xbps_object_iterator_release(iter);
	/* XXX: not possible to free shrequires without copying values */
	xbps_object_release(shprovides);

	return unmatched;
}
