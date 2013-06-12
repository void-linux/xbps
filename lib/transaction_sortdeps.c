/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

/*
 * Sorting algorithm for packages in the transaction dictionary.
 * The transaction dictionary contains all package dictionaries found from
 * the repository plist index file in the "unsorted_deps" array.
 *
 * Any package in the unsorted_deps array is added into the tail and
 * later if a package dependency has an index greater than current
 * package, the package dependency is moved just before it.
 *
 * Once that all package dependencies for a package are in the correct
 * index, the counter is increased. When all packages in the "unsorted_deps"
 * array are processed the loop is stopped and the counter should have
 * the same number than the "unsorted_deps" array... otherwise something
 * went wrong.
 */

struct pkgdep {
	TAILQ_ENTRY(pkgdep) pkgdep_entries;
	prop_dictionary_t d;
	char *name;
};

static TAILQ_HEAD(pkgdep_head, pkgdep) pkgdep_list =
    TAILQ_HEAD_INITIALIZER(pkgdep_list);

static struct pkgdep *
pkgdep_find(const char *pkg)
{
	struct pkgdep *pd = NULL, *pd_new = NULL;
	const char *pkgver, *tract;

	TAILQ_FOREACH_SAFE(pd, &pkgdep_list, pkgdep_entries, pd_new) {
		if (pd->d == NULL) {
			/* ignore entries without dictionary */
			continue;
		}
		prop_dictionary_get_cstring_nocopy(pd->d,
		    "transaction", &tract);
		/* ignore pkgs to be removed */
		if (strcmp(tract, "remove") == 0)
			continue;
		/* simple match */
		prop_dictionary_get_cstring_nocopy(pd->d, "pkgver", &pkgver);
		if (strcmp(pkgver, pkg) == 0)
			return pd;
		/* pkg expression match */
		if (xbps_pkgpattern_match(pkgver, pkg))
			return pd;
		/* virtualpkg expression match */
		if (xbps_match_virtual_pkg_in_dict(pd->d, pkg, true))
			return pd;
	}

	/* not found */
	return NULL;
}

static int32_t
pkgdep_find_idx(const char *pkg)
{
	struct pkgdep *pd, *pd_new;
	int32_t idx = 0;
	const char *pkgver, *tract;

	TAILQ_FOREACH_SAFE(pd, &pkgdep_list, pkgdep_entries, pd_new) {
		if (pd->d == NULL) {
			/* ignore entries without dictionary */
			idx++;
			continue;
		}
		prop_dictionary_get_cstring_nocopy(pd->d,
		    "transaction", &tract);
		/* ignore pkgs to be removed */
		if (strcmp(tract, "remove") == 0) {
			idx++;
			continue;
		}
		/* simple match */
		prop_dictionary_get_cstring_nocopy(pd->d, "pkgver", &pkgver);
		if (strcmp(pkgver, pkg) == 0)
			return idx;
		/* pkg expression match */
		if (xbps_pkgpattern_match(pkgver, pkg))
			return idx;
		/* virtualpkg expression match */
		if (xbps_match_virtual_pkg_in_dict(pd->d, pkg, true))
			return idx;

		idx++;
	}
	/* not found */
	return -1;
}

static void
pkgdep_release(struct pkgdep *pd)
{
	free(pd->name);
	free(pd);
}

static struct pkgdep *
pkgdep_alloc(prop_dictionary_t d, const char *pkg)
{
	struct pkgdep *pd;

	pd = malloc(sizeof(*pd));
	assert(pd);
	pd->d = d;
	pd->name = strdup(pkg);

	return pd;
}

static void
pkgdep_end(prop_array_t sorted)
{
	struct pkgdep *pd;

	while ((pd = TAILQ_FIRST(&pkgdep_list)) != NULL) {
		TAILQ_REMOVE(&pkgdep_list, pd, pkgdep_entries);
		if (sorted != NULL && pd->d != NULL)
			prop_array_add(sorted, pd->d);

		pkgdep_release(pd);
	}
}

static int
sort_pkg_rundeps(struct xbps_handle *xhp,
		 struct pkgdep *pd,
		 prop_array_t pkg_rundeps,
		 prop_array_t unsorted)
{
	prop_dictionary_t curpkgd;
	struct pkgdep *lpd, *pdn;
	const char *str, *tract;
	int32_t pkgdepidx, curpkgidx;
	uint32_t i, idx = 0;
	int rv = 0;

	xbps_dbg_printf_append(xhp, "\n");
	curpkgidx = pkgdep_find_idx(pd->name);

again:
	for (i = idx; i < prop_array_count(pkg_rundeps); i++) {
		prop_array_get_cstring_nocopy(pkg_rundeps, i, &str);
		xbps_dbg_printf(xhp, "  Required dependency '%s': ", str);

		pdn = pkgdep_find(str);
		if ((pdn == NULL) && xbps_pkg_is_installed(xhp, str)) {
			/*
			 * Package dependency is installed, just add to
			 * the list but just mark it as "installed", to avoid
			 * calling xbps_check_is_installed_pkg_by_name(),
			 * which is expensive.
			 */
			xbps_dbg_printf_append(xhp, "installed.\n");
			lpd = pkgdep_alloc(NULL, str);
			TAILQ_INSERT_TAIL(&pkgdep_list, lpd, pkgdep_entries);
			continue;
		} else if (pdn != NULL && pdn->d == NULL) {
			/*
			 * Package was added previously into the list
			 * and is installed, skip.
			 */
			xbps_dbg_printf_append(xhp, "installed.\n");
			continue;
		}
		if (((curpkgd = xbps_find_pkg_in_array(unsorted, str)) == NULL) &&
		    ((curpkgd = xbps_find_virtualpkg_in_array(xhp, unsorted, str)) == NULL)) {
			rv = EINVAL;
			break;
		}
		if ((xbps_match_virtual_pkg_in_dict(curpkgd, str, true)) ||
		    (xbps_match_virtual_pkg_in_dict(curpkgd, str, false))) {
			xbps_dbg_printf_append(xhp, "ignore wrong "
			    "dependency %s (depends on itself)\n", str);
			continue;
		}
		prop_dictionary_get_cstring_nocopy(curpkgd,
		    "transaction", &tract);
		lpd = pkgdep_alloc(curpkgd, str);

		if (pdn == NULL) {
			/*
			 * If package is not in the list, add to the tail
			 * and iterate at the same position.
			 */
			TAILQ_INSERT_TAIL(&pkgdep_list, lpd, pkgdep_entries);
			idx = i;
			xbps_dbg_printf_append(xhp, "added into the tail, "
			    "checking again...\n");
			goto again;
		}
		/*
		 * Find package dependency index.
		 */
		pkgdepidx = pkgdep_find_idx(str);
		/*
		 * If package dependency index is less than current
		 * package index, it's already sorted.
		 */
		if (pkgdepidx < curpkgidx) {
			xbps_dbg_printf_append(xhp, "already sorted.\n");
			pkgdep_release(lpd);
		} else {
			/*
			 * Remove package dependency from list and move
			 * it before current package.
			 */
			TAILQ_REMOVE(&pkgdep_list, pdn, pkgdep_entries);
			pkgdep_release(pdn);
			TAILQ_INSERT_BEFORE(pd, lpd, pkgdep_entries);
			xbps_dbg_printf_append(xhp,
			    "added before `%s'.\n", pd->name);
		}
	}

	return rv;
}

int HIDDEN
xbps_transaction_sort(struct xbps_handle *xhp)
{
	prop_array_t provides, sorted, unsorted, rundeps;
	prop_object_t obj;
	struct pkgdep *pd;
	unsigned int i, j, ndeps = 0, cnt = 0;
	const char *pkgname, *pkgver, *tract, *vpkgdep;
	int rv = 0;
	bool vpkg_found;

	if ((sorted = prop_array_create()) == NULL)
		return ENOMEM;
	/*
	 * Add sorted packages array into transaction dictionary (empty).
	 */
	if (!prop_dictionary_set(xhp->transd, "packages", sorted)) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * All required deps are satisfied (already installed).
	 */
	unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");
	if (prop_array_count(unsorted) == 0) {
		prop_dictionary_set(xhp->transd, "packages", sorted);
		prop_object_release(sorted);
		return 0;
	}
	/*
	 * The sorted array should have the same capacity than
	 * all objects in the unsorted array.
	 */
	ndeps = prop_array_count(unsorted);
	/*
	 * Iterate over the unsorted package dictionaries and sort all
	 * its package dependencies.
	 */
	for (i = 0; i < ndeps; i++) {
		vpkg_found = false;
		obj = prop_array_get(unsorted, i);
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		provides = prop_dictionary_get(obj, "provides");
		xbps_dbg_printf(xhp, "Sorting package '%s' (%s): ", pkgver, tract);

		if (provides) {
			/*
			 * If current pkgdep provides any virtual pkg check
			 * if any of them was previously added. If true, don't
			 * add it into the list again, just order its deps.
			 */
			for (j = 0; j < prop_array_count(provides); j++) {
				prop_array_get_cstring_nocopy(provides,
				    j, &vpkgdep);
				pd = pkgdep_find(vpkgdep);
				if (pd != NULL) {
					xbps_dbg_printf_append(xhp, "already "
					    "sorted via `%s' vpkg.", vpkgdep);
					vpkg_found = true;
					break;
				}
			}
		}
		if (!vpkg_found && (pd = pkgdep_find(pkgver)) == NULL) {
			/*
			 * If package not in list, just add to the tail.
			 */
			pd = pkgdep_alloc(obj, pkgver);
			if (pd == NULL) {
				pkgdep_end(NULL);
				rv = ENOMEM;
				goto out;
			}
			if (strcmp(tract, "remove") == 0) {
				xbps_dbg_printf_append(xhp, "added into head.\n");
				TAILQ_INSERT_HEAD(&pkgdep_list, pd,
				    pkgdep_entries);
				cnt++;
				continue;
			} else {
				xbps_dbg_printf_append(xhp, "added into tail.");
				TAILQ_INSERT_TAIL(&pkgdep_list, pd,
				    pkgdep_entries);
			}
		}
		/*
		 * Packages that don't have deps go at head, because
		 * it doesn't matter.
		 */
		rundeps = prop_dictionary_get(obj, "run_depends");
		if (rundeps == NULL || prop_array_count(rundeps) == 0) {
			xbps_dbg_printf_append(xhp, "\n");
			cnt++;
			continue;
		}
		/*
		 * Sort package run-time dependencies for this package.
		 */
		if ((rv = sort_pkg_rundeps(xhp, pd, rundeps, unsorted)) != 0) {
			pkgdep_end(NULL);
			goto out;
		}
		cnt++;
	}
	/*
	 * We are done, now we have to copy all pkg dictionaries
	 * from the sorted list into the "packages" array, and at
	 * the same time freeing memory used for temporary sorting.
	 */
	pkgdep_end(sorted);
	/*
	 * Sanity check that the array contains the same number of
	 * objects than the total number of required dependencies.
	 */
	assert(cnt == prop_array_count(unsorted));
	/*
	 * We are done, all packages were sorted... remove the
	 * temporary array with unsorted packages.
	 */
	prop_dictionary_remove(xhp->transd, "unsorted_deps");
out:
	if (rv != 0)
		prop_dictionary_remove(xhp->transd, "packages");

	prop_object_release(sorted);

	return rv;
}
