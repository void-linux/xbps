/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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
#include <errno.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

/*
 * Sorting algorithm for packages in the transaction dictionary.
 * The transaction dictionary contains all package dictionaries found from
 * the repository plist index file in the "unsorted_deps" array.
 *
 * When a package has no rundeps or all rundeps are satisfied, the package
 * dictionary is added into the "packages" array and it is removed from the
 * "unsorted_deps" array; that means the package has been sorted in the
 * transaction.
 *
 * It will loop until all packages are processed and will check that
 * the number of packages added into the "packages" array is the same than
 * it was in the "unsorted_deps" array.
 */
int HIDDEN
xbps_sort_pkg_deps(void)
{
	prop_dictionary_t transd;
	prop_array_t sorted, unsorted, rundeps;
	prop_object_t obj, obj2;
	prop_object_iterator_t iter, iter2;
	size_t ndeps = 0, rundepscnt = 0, cnt = 0;
	const char *pkgname, *pkgver, *str;
	char *pkgnamedep;
	int rv = 0;

	if ((transd = xbps_transaction_dictionary_get()) == NULL)
		return EINVAL;

	sorted = prop_array_create();
	if (sorted == NULL)
		return ENOMEM;
	/*
	 * Add sorted packages array into transaction dictionary (empty).
	 */
	if (!prop_dictionary_set(transd, "packages", sorted)) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * All required deps are satisfied (already installed).
	 */
	unsorted = prop_dictionary_get(transd, "unsorted_deps");
	if (prop_array_count(unsorted) == 0) {
		prop_dictionary_set(transd, "packages", sorted);
		return 0;
	}
	ndeps = prop_array_count(unsorted);

	iter = prop_array_iterator(unsorted);
	if (iter == NULL) {
		prop_object_release(sorted);
		return ENOMEM;
	}
again:
	/*
	 * Order all deps by looking at its run_depends array.
	 */
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dbg_printf("Sorting package '%s': ", pkgver);

		if (xbps_find_pkg_in_dict_by_name(transd,
		    "packages", pkgname)) {
			xbps_dbg_printf_append("skipping, already queued.\n",
			    pkgname);
			continue;
		}

		/*
		 * Packages that don't have deps go unsorted, because
		 * it doesn't matter.
		 */
		rundeps = prop_dictionary_get(obj, "run_depends");
		if (rundeps == NULL || prop_array_count(rundeps) == 0) {
			xbps_dbg_printf_append("added (no rundeps) into "
			    "the sorted queue.\n");
			prop_array_add(sorted, obj);
			if (!xbps_remove_pkg_from_dict(transd,
			    "unsorted_deps", pkgname)) {
				xbps_dbg_printf("can't remove %s from "
				    "unsorted_deps array!\n", pkgname);
			}
			cnt++;
			continue;
		}
		iter2 = prop_array_iterator(rundeps);
		if (iter2 == NULL) {
			rv = ENOMEM;
			goto out;
		}

		/*
		 * Iterate over the run_depends array, and find out if they
		 * were already added in the sorted list.
		 */
		xbps_dbg_printf_append("\n");
		xbps_dbg_printf("Checking '%s' run depends for sorting...\n",
		    pkgver);
		while ((obj2 = prop_object_iterator_next(iter2)) != NULL) {
			str = prop_string_cstring_nocopy(obj2);
			if (str == NULL) {
				rv = EINVAL;
				goto out;
			}
			pkgnamedep = xbps_get_pkgpattern_name(str);
			if (pkgnamedep == NULL) {
				rv = EINVAL;
				goto out;
			}
			xbps_dbg_printf("  Required dependency '%s': ", str);
			/*
			 * If dependency is already satisfied or queued,
			 * pass to the next one.
			 */
			if (xbps_check_is_installed_pkg(str)) {
				rundepscnt++;
				xbps_dbg_printf_append("installed.\n");
			} else if (xbps_find_pkg_in_dict_by_name(transd,
			    "packages", pkgnamedep)) {
				xbps_dbg_printf_append("queued.\n");
				rundepscnt++;
			} else {
				xbps_dbg_printf_append("not installed.\n");
			}
			free(pkgnamedep);
		}
		prop_object_iterator_release(iter2);

		/* Add dependency if all its required deps are already added */
		if (prop_array_count(rundeps) == rundepscnt) {
			prop_array_add(sorted, obj);
			if (!xbps_remove_pkg_from_dict(transd,
			    "unsorted_deps", pkgname)) {
				xbps_dbg_printf("can't remove %s from "
				    "unsorted_deps array!\n", pkgname);
			}
			xbps_dbg_printf("Added package '%s' to the sorted "
			    "queue (all rundeps satisfied).\n\n", pkgver);
			rundepscnt = 0;
			cnt++;
			continue;
		}
		xbps_dbg_printf("Unsorted package '%s' has missing "
		    "rundeps (missing %zu).\n\n", pkgver,
		    prop_array_count(rundeps) - rundepscnt);
		rundepscnt = 0;
	}
	/* Iterate until all deps are processed. */
	if (cnt < ndeps) {
		xbps_dbg_printf("Missing required deps! queued: %zu "
		    "required: %zu.\n", cnt, ndeps);
		prop_object_iterator_reset(iter);
		xbps_dbg_printf("total iteratons %zu\n", cnt);
		goto again;
	}
	prop_object_iterator_release(iter);

	/*
	 * Sanity check that the array contains the same number of
	 * objects than the total number of required dependencies.
	 */
	if (ndeps != prop_array_count(sorted)) {
		xbps_dbg_printf("wrong sorted deps cnt %zu vs %zu\n",
		    ndeps, prop_array_count(sorted));
		rv = EINVAL;
		goto out;
	}
	/*
	 * We are done, all packages were sorted... remove the
	 * temporary array with unsorted packages.
	 */
	prop_dictionary_remove(transd, "unsorted_deps");

out:
	if (rv != 0)
		prop_dictionary_remove(transd, "packages");

	prop_object_release(sorted);

	return rv;
}
