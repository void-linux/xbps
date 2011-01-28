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
#include "queue.h"
#include "strlcpy.h"

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
pkgdep_find(const char *name)
{
	struct pkgdep *pd = NULL;

	TAILQ_FOREACH(pd, &pkgdep_list, pkgdep_entries)
		if (strcmp(pd->name, name) == 0)
			return pd;

	/* not found */
	return NULL;
}

static ssize_t
pkgdep_find_idx(const char *name)
{
	struct pkgdep *pd;
	ssize_t idx = 0;

	TAILQ_FOREACH(pd, &pkgdep_list, pkgdep_entries) {
		if (strcmp(pd->name, name) == 0)
			return idx;

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
pkgdep_alloc(prop_dictionary_t d, const char *name)
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
sort_pkg_rundeps(prop_dictionary_t transd,
		 struct pkgdep *pd,
		 prop_array_t pkg_rundeps)
{
	prop_dictionary_t curpkgd;
	prop_object_t obj;
	struct pkgdep *lpd, *pdn;
	const char *str;
	char *pkgnamedep;
	ssize_t pkgdepidx, curpkgidx = pkgdep_find_idx(pd->name);
	size_t i, idx = 0;
	int rv = 0;

	xbps_dbg_printf_append("\n");

again:
	for (i = idx; i < prop_array_count(pkg_rundeps); i++) {
		obj = prop_array_get(pkg_rundeps, i);
		str = prop_string_cstring_nocopy(obj);
		if (str == NULL) {
			rv = ENOMEM;
			break;
		}
		pkgnamedep = xbps_get_pkgpattern_name(str);
		if (pkgnamedep == NULL) {
			rv = ENOMEM;
			break;
		}
		xbps_dbg_printf("  Required dependency '%s': ", str);
		pdn = pkgdep_find(pkgnamedep);
		if ((pdn == NULL) &&
		    xbps_check_is_installed_pkg_by_name(pkgnamedep)) {
			/*
			 * Package dependency is installed, just add to
			 * the list but just mark it as "installed", to avoid
			 * calling xbps_check_is_installed_pkg_by_name(),
			 * which is expensive.
			 */
			xbps_dbg_printf_append("installed.\n");
			lpd = pkgdep_alloc(NULL, pkgnamedep);
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
			xbps_dbg_printf_append("installed.\n");
			free(pkgnamedep);
			continue;
		}
		curpkgd = xbps_find_pkg_in_dict_by_name(transd,
		    "unsorted_deps", pkgnamedep);
		assert(curpkgd != NULL);
		lpd = pkgdep_alloc(curpkgd, pkgnamedep);
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
			xbps_dbg_printf_append("added into the tail, "
			    "checking again...\n");
			free(pkgnamedep);
			goto again;
		}
		/*
		 * Find package dependency index.
		 */
		pkgdepidx = pkgdep_find_idx(pkgnamedep);
		/*
		 * If package dependency index is less than current
		 * package index, it's already sorted.
		 */
		free(pkgnamedep);
		if (pkgdepidx < curpkgidx) {
			xbps_dbg_printf_append("already sorted.\n");
			pkgdep_release(lpd);
		} else {
			/*
			 * Remove package dependency from list and move
			 * it before current package.
			 */
			TAILQ_REMOVE(&pkgdep_list, pdn, pkgdep_entries);
			pkgdep_release(pdn);
			TAILQ_INSERT_BEFORE(pd, lpd, pkgdep_entries);
			xbps_dbg_printf_append("added before `%s'.\n", pd->name);
		}
	}

	return rv;
}

int HIDDEN
xbps_sort_pkg_deps(void)
{
	prop_dictionary_t transd;
	prop_array_t sorted, unsorted, rundeps;
	prop_object_t obj;
	prop_object_iterator_t iter;
	struct pkgdep *pd;
	size_t ndeps = 0, cnt = 0;
	const char *pkgname, *pkgver;
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
	/*
	 * The sorted array should have the same capacity than
	 * all objects in the unsorted array.
	 */
	ndeps = prop_array_count(unsorted);
	if (!prop_array_ensure_capacity(sorted, ndeps)) {
		xbps_error_printf("failed to set capacity to the sorted "
		    "pkgdeps array\n");
		return ENOMEM;
	}
	iter = prop_array_iterator(unsorted);
	if (iter == NULL) {
		rv = ENOMEM;
		goto out;
	}
	/*
	 * Iterate over the unsorted package dictionaries and sort all
	 * its package dependencies.
	 */
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dbg_printf("Sorting package '%s': ", pkgver);

		pd = pkgdep_find(pkgname);
		if (pd == NULL) {
			/*
			 * If package not in list, just add to the tail.
			 */
			pd = pkgdep_alloc(obj, pkgname);
			if (pd == NULL) {
				pkgdep_end(NULL);
				prop_object_iterator_release(iter);
				rv = ENOMEM;
				goto out;
			}
			TAILQ_INSERT_TAIL(&pkgdep_list, pd, pkgdep_entries);
		}
		/*
		 * Packages that don't have deps go unsorted, because
		 * it doesn't matter.
		 */
		rundeps = prop_dictionary_get(obj, "run_depends");
		if (rundeps == NULL || prop_array_count(rundeps) == 0) {
			xbps_dbg_printf_append("added (no rundeps) into "
			    "the sorted queue.\n");
			cnt++;
			continue;
		}
		/*
		 * Sort package run-time dependencies for this package.
		 */
		if ((rv = sort_pkg_rundeps(transd, pd, rundeps)) != 0) {
			pkgdep_end(NULL);
			prop_object_iterator_release(iter);
			goto out;
		}
		cnt++;
	}
	prop_object_iterator_release(iter);
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
	prop_dictionary_remove(transd, "unsorted_deps");
out:
	if (rv != 0)
		prop_dictionary_remove(transd, "packages");

	prop_object_release(sorted);

	return rv;
}
