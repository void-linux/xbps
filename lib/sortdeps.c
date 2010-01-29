/*-
 * Copyright (c) 2009 Juan Romero Pardines.
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

struct sorted_dependency {
	SIMPLEQ_ENTRY(sorted_dependency) chain;
	prop_dictionary_t dict;
};

static SIMPLEQ_HEAD(sdep_head, sorted_dependency) sdep_list =
    SIMPLEQ_HEAD_INITIALIZER(sdep_list);

static struct sorted_dependency *
find_sorteddep_by_name(const char *pkgname)
{
	struct sorted_dependency *sdep = NULL;
	const char *curpkgname;
	bool found = false;

	SIMPLEQ_FOREACH(sdep, &sdep_list, chain) {
		prop_dictionary_get_cstring_nocopy(sdep->dict,
		    "pkgname", &curpkgname);
		if (strcmp(pkgname, curpkgname) == 0) {
			found = true;
			break;
		}
	}
	if (!found)
		return NULL;

	return sdep;
}

int HIDDEN
xbps_sort_pkg_deps(prop_dictionary_t chaindeps)
{
	prop_array_t sorted, unsorted, rundeps, missingdeps;
	prop_object_t obj, obj2;
	prop_object_iterator_t iter, iter2;
	struct sorted_dependency *sdep;
	size_t ndeps = 0, rundepscnt = 0, cnt = 0;
	const char *pkgname, *pkgver, *str;
	char *pkgnamedep;
	int rv = 0;

	assert(chaindeps != NULL);

	/*
	 * If there are missing dependencies, bail out.
	 */
	missingdeps = prop_dictionary_get(chaindeps, "missing_deps");
	if (prop_array_count(missingdeps) > 0)
		return ENOENT;

	sorted = prop_array_create();
	if (sorted == NULL)
		return ENOMEM;

	/*
	 * All required deps are satisfied (already installed).
	 */
	unsorted = prop_dictionary_get(chaindeps, "unsorted_deps");
	if (prop_array_count(unsorted) == 0) {
		prop_dictionary_set(chaindeps, "packages", sorted);
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
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname)) {
			rv = errno;
			goto out;
		}
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &pkgver)) {
			rv = errno;
			goto out;
		}
		DPRINTF(("Sorting package: %s\n", pkgver));
		if (find_sorteddep_by_name(pkgname) != NULL) {
			DPRINTF(("Skipping %s already queued.\n", pkgname));
			continue;
		}

		sdep = malloc(sizeof(*sdep));
		if (sdep == NULL) {
			rv = ENOMEM;
			goto out;
		}
		/*
		 * Packages that don't have deps go unsorted, because
		 * it doesn't matter.
		 */
		rundeps = prop_dictionary_get(obj, "run_depends");
		if (rundeps == NULL || prop_array_count(rundeps) == 0) {
			DPRINTF(("Adding %s (no rundeps) into the sorted "
			    "queue.\n", pkgver));
			sdep->dict = prop_dictionary_copy(obj);
			SIMPLEQ_INSERT_TAIL(&sdep_list, sdep, chain);
			cnt++;
			continue;
		}
		iter2 = prop_array_iterator(rundeps);
		if (iter2 == NULL) {
			free(sdep);
			rv = ENOMEM;
			goto out;
		}
		/*
		 * Iterate over the run_depends array, and find out if they
		 * were already added in the sorted list.
		 */
		DPRINTF(("Checking %s run_depends for sorting...\n", pkgver));
		while ((obj2 = prop_object_iterator_next(iter2)) != NULL) {
			str = prop_string_cstring_nocopy(obj2);
			if (str == NULL) {
				free(sdep);
				rv = EINVAL;
				goto out;
			}
			pkgnamedep = xbps_get_pkgpattern_name(str);
			if (pkgnamedep == NULL) {
				free(sdep);
				rv = errno;
				goto out;
			}
			DPRINTF(("Required dependency %s: ", str));
			/*
			 * If dependency is already satisfied or queued,
			 * pass to the next one.
			 */
			if (xbps_check_is_installed_pkg(str)) {
				rundepscnt++;
				DPRINTF(("installed.\n"));
			} else if (find_sorteddep_by_name(pkgnamedep) != NULL) {
				DPRINTF(("queued.\n"));
				rundepscnt++;
			} else {
				DPRINTF(("not installed or queued.\n"));
			}
			free(pkgnamedep);
		}
		prop_object_iterator_release(iter2);

		/* Add dependency if all its required deps are already added */
		if (prop_array_count(rundeps) == rundepscnt) {
			DPRINTF(("Adding package %s to the sorted queue.\n",
			    pkgver));
			sdep->dict = prop_dictionary_copy(obj);
			SIMPLEQ_INSERT_TAIL(&sdep_list, sdep, chain);
			rundepscnt = 0;
			cnt++;
			continue;
		}
		DPRINTF(("Unsorted package %s has missing rundeps.\n", pkgver));
		free(sdep);
		rundepscnt = 0;
	}

	/* Iterate until all deps are processed. */
	if (cnt < ndeps) {
		DPRINTF(("Missing required deps! cnt: %zu ndeps: %zu\n",
		    cnt, ndeps));
		prop_object_iterator_reset(iter);
		goto again;
	}
	prop_object_iterator_release(iter);

	/*
	 * Add all sorted dependencies into the sorted deps array.
	 */
	while ((sdep = SIMPLEQ_FIRST(&sdep_list)) != NULL) {
		if (!prop_array_add(sorted, sdep->dict)) {
			free(sdep);
			rv = errno;
			goto out;
		}
		SIMPLEQ_REMOVE(&sdep_list, sdep, sorted_dependency, chain);
		prop_object_release(sdep->dict);
		free(sdep);
	}

	/*
	 * Sanity check that the array contains the same number of
	 * objects than the total number of required dependencies.
	 */
	if (ndeps != prop_array_count(sorted)) {
		rv = EINVAL;
		goto out;
	}

	if (!prop_dictionary_set(chaindeps, "packages", sorted))
		rv = EINVAL;

	prop_dictionary_remove(chaindeps, "unsorted_deps");

out:
	/*
	 * Release resources used by temporary sorting.
	 */
	prop_object_release(sorted);
	while ((sdep = SIMPLEQ_FIRST(&sdep_list)) != NULL) {
		SIMPLEQ_REMOVE(&sdep_list, sdep, sorted_dependency, chain);
		prop_object_release(sdep->dict);
		free(sdep);
	}

	return rv;
}
