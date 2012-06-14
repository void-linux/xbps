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
	const char *trans;
};

static TAILQ_HEAD(pkgdep_head, pkgdep) pkgdep_list =
    TAILQ_HEAD_INITIALIZER(pkgdep_list);

static struct pkgdep *
pkgdep_find(const char *name, const char *trans)
{
	struct pkgdep *pd = NULL, *pd_new = NULL;

	TAILQ_FOREACH_SAFE(pd, &pkgdep_list, pkgdep_entries, pd_new) {
		if (strcmp(pd->name, name) == 0) {
			if (trans == NULL)
				return pd;
			if (strcmp(pd->trans, trans) == 0)
				return pd;
		}
		if (pd->d == NULL)
			continue;
		if (xbps_match_virtual_pkg_in_dict(pd->d, name, false)) {
			if (trans && pd->trans &&
			    (strcmp(trans, pd->trans) == 0))
				return pd;
		}
	}

	/* not found */
	return NULL;
}

static ssize_t
pkgdep_find_idx(const char *name, const char *trans)
{
	struct pkgdep *pd, *pd_new;
	ssize_t idx = 0;

	TAILQ_FOREACH_SAFE(pd, &pkgdep_list, pkgdep_entries, pd_new) {
		if (strcmp(pd->name, name) == 0) {
			if (trans == NULL)
				return idx;
		    	if (strcmp(pd->trans, trans) == 0)
				return idx;
		}
		if (pd->d == NULL)
			continue;
		if (xbps_match_virtual_pkg_in_dict(pd->d, name, false)) {
			if (trans && pd->trans &&
			    (strcmp(trans, pd->trans) == 0))
				return idx;
		}

		idx++;
	}
	/* not found */
	return -1;
}

static void
pkgdep_release(struct pkgdep *pd)
{
	if (pd->d != NULL)
		prop_object_release(pd->d);

	free(pd->name);
	free(pd);
	pd = NULL;
}

static struct pkgdep *
pkgdep_alloc(prop_dictionary_t d, const char *name, const char *trans)
{
	struct pkgdep *pd;
	size_t len;

	if ((pd = malloc(sizeof(*pd))) == NULL)
		return NULL;

	len = strlen(name) + 1;
	if ((pd->name = malloc(len)) == NULL) {
		free(pd);
		return NULL;
	}
	if (d != NULL)
		pd->d = prop_dictionary_copy(d);
	else
		pd->d = NULL;

	(void)strlcpy(pd->name, name, len);
	pd->trans = trans;

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
		 prop_array_t pkg_rundeps)
{
	prop_dictionary_t curpkgd;
	struct pkgdep *lpd, *pdn;
	const char *str, *tract;
	char *pkgnamedep;
	ssize_t pkgdepidx, curpkgidx;
	size_t i, idx = 0;
	int rv = 0;

	xbps_dbg_printf_append(xhp, "\n");
	curpkgidx = pkgdep_find_idx(pd->name, pd->trans);

again:
	for (i = idx; i < prop_array_count(pkg_rundeps); i++) {
		prop_array_get_cstring_nocopy(pkg_rundeps, i, &str);
		pkgnamedep = xbps_pkgpattern_name(str);
		if (pkgnamedep == NULL) {
			rv = ENOMEM;
			break;
		}
		xbps_dbg_printf(xhp, "  Required dependency '%s': ", str);
		pdn = pkgdep_find(pkgnamedep, NULL);
		if ((pdn == NULL) &&
		    xbps_check_is_installed_pkg_by_name(xhp, pkgnamedep)) {
			/*
			 * Package dependency is installed, just add to
			 * the list but just mark it as "installed", to avoid
			 * calling xbps_check_is_installed_pkg_by_name(),
			 * which is expensive.
			 */
			xbps_dbg_printf_append(xhp, "installed.\n");
			lpd = pkgdep_alloc(NULL, pkgnamedep, "installed");
			if (lpd == NULL) {
				rv = ENOMEM;
				break;
			}
			free(pkgnamedep);
			TAILQ_INSERT_TAIL(&pkgdep_list, lpd, pkgdep_entries);
			continue;
		} else if (pdn != NULL && pdn->d == NULL) {
			/*
			 * Package was added previously into the list
			 * and is installed, skip.
			 */
			xbps_dbg_printf_append(xhp, "installed.\n");
			free(pkgnamedep);
			continue;
		}
		/* Find pkg by name */
		curpkgd = xbps_find_pkg_in_dict_by_name(xhp->transd,
		    "unsorted_deps", pkgnamedep);
		if (curpkgd == NULL) {
			/* find virtualpkg by name if no match */
			curpkgd =
			    xbps_find_virtualpkg_in_dict_by_name(xhp->transd,
			    "unsorted_deps", pkgnamedep);
		}
		if (curpkgd == NULL) {
			free(pkgnamedep);
			rv = EINVAL;
			break;
		}
		prop_dictionary_get_cstring_nocopy(curpkgd,
		    "transaction", &tract);
		lpd = pkgdep_alloc(curpkgd, pkgnamedep, tract);
		if (lpd == NULL) {
			free(pkgnamedep);
			rv = ENOMEM;
			break;
		}
		if (pdn == NULL) {
			/*
			 * If package is not in the list, add to the tail
			 * and iterate at the same position.
			 */
			TAILQ_INSERT_TAIL(&pkgdep_list, lpd, pkgdep_entries);
			idx = i;
			xbps_dbg_printf_append(xhp, "added into the tail, "
			    "checking again...\n");
			free(pkgnamedep);
			goto again;
		}
		/*
		 * Find package dependency index.
		 */
		pkgdepidx = pkgdep_find_idx(pkgnamedep, tract);
		/*
		 * If package dependency index is less than current
		 * package index, it's already sorted.
		 */
		free(pkgnamedep);
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
xbps_transaction_sort_pkg_deps(struct xbps_handle *xhp)
{
	prop_array_t sorted, unsorted, rundeps;
	prop_object_t obj;
	struct pkgdep *pd;
	size_t i, ndeps = 0, cnt = 0;
	const char *pkgname, *pkgver, *tract;
	int rv = 0;

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
		obj = prop_array_get(unsorted, i);
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		xbps_dbg_printf(xhp, "Sorting package '%s' (%s): ", pkgver, tract);

		pd = pkgdep_find(pkgname, tract);
		if (pd == NULL) {
			/*
			 * If package not in list, just add to the tail.
			 */
			pd = pkgdep_alloc(obj, pkgname, tract);
			if (pd == NULL) {
				pkgdep_end(NULL);
				rv = ENOMEM;
				goto out;
			}
			if (strcmp(pd->trans, "remove") == 0) {
				xbps_dbg_printf_append(xhp, "added into head.");
				TAILQ_INSERT_HEAD(&pkgdep_list, pd,
				    pkgdep_entries);
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
		if ((rv = sort_pkg_rundeps(xhp, pd, rundeps)) != 0) {
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
