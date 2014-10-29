/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
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
#include <assert.h>

#include <xbps.h>
#include "defs.h"
#include "queue.h"

struct pkgdep {
	SLIST_ENTRY(pkgdep) pkgdep_entries;
	const char *pkg;
	xbps_array_t rdeps;
};

static SLIST_HEAD(pkgdep_head, pkgdep) pkgdep_list =
    SLIST_HEAD_INITIALIZER(pkgdep_list);

static xbps_dictionary_t pkgdep_pvmap;

static void
print_rdeps(struct xbps_handle *xhp, xbps_array_t rdeps, bool full, bool repo)
{
	xbps_array_t currdeps;
	xbps_dictionary_t pkgd;
	const char *curdep;

	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++) {
		struct pkgdep *pd;
		const char *pkgver;
		bool virtual = false, found = false;

		xbps_array_get_cstring_nocopy(rdeps, i, &curdep);
		if (!full) {
			printf("%s\n", curdep);
			continue;
		}
		if (repo) {
			if ((pkgd = xbps_rpool_get_pkg(xhp, curdep)) == NULL) {
				pkgd = xbps_rpool_get_virtualpkg(xhp, curdep);
				virtual = true;
			}
		} else {
			if ((pkgd = xbps_pkgdb_get_pkg(xhp, curdep)) == NULL) {
				pkgd = xbps_pkgdb_get_virtualpkg(xhp, curdep);
				virtual = true;
			}
		}
		assert(pkgd);
		currdeps = xbps_dictionary_get(pkgd, "run_depends");
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		assert(pkgver);
		if (virtual) {
			char *p;

			if ((p = xbps_pkgpattern_name(curdep)) == NULL)
				p = xbps_pkg_name(curdep);

			assert(p);
			if (pkgdep_pvmap == NULL)
				pkgdep_pvmap = xbps_dictionary_create();

			xbps_dictionary_set_cstring_nocopy(pkgdep_pvmap, p, pkgver);
			free(p);
		}
		/* uniquify dependencies, sorting will be done later */
		SLIST_FOREACH(pd, &pkgdep_list, pkgdep_entries) {
			if (strcmp(pd->pkg, pkgver) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			pd = malloc(sizeof(*pd));
			assert(pd);
			pd->pkg = pkgver;
			pd->rdeps = xbps_array_copy(currdeps);
			SLIST_INSERT_HEAD(&pkgdep_list, pd, pkgdep_entries);
			//printf("Added %s into the slist\n", pd->pkg);
		}
		if (xbps_array_count(currdeps))
			print_rdeps(xhp, currdeps, full, repo);
	}
}

static xbps_array_t
sort_rdeps(void)
{
	struct pkgdep *pd;
	xbps_array_t result;
	unsigned int ndeps = 0;

	result = xbps_array_create();
	assert(result);

	SLIST_FOREACH(pd, &pkgdep_list, pkgdep_entries) {
		if (!pd->rdeps) {
			xbps_array_add_cstring_nocopy(result, pd->pkg);
			SLIST_REMOVE(&pkgdep_list, pd, pkgdep, pkgdep_entries);
		}
		ndeps++;
	}
	while (xbps_array_count(result) < ndeps) {
		bool found = false;

		SLIST_FOREACH(pd, &pkgdep_list, pkgdep_entries) {
			unsigned int i = 0, mdeps = 0, rdeps = 0;

			rdeps = xbps_array_count(pd->rdeps);
			for (i = 0; i < rdeps; i++) {
				const char *pkgdep;
				char *pkgname;

				xbps_array_get_cstring_nocopy(pd->rdeps, i, &pkgdep);
				if ((pkgname = xbps_pkgpattern_name(pkgdep)) == NULL)
					pkgname = xbps_pkg_name(pkgdep);

				assert(pkgname);
				if (xbps_match_pkgname_in_array(result, pkgname)) {
					mdeps++;
					free(pkgname);
					continue;
				}
				if (xbps_dictionary_get(pkgdep_pvmap, pkgname)) {
					mdeps++;
					free(pkgname);
					continue;
				}
				//printf("%s: missing dep %s\n", pd->pkg, pkgdep);
				free(pkgname);
			}
			if (mdeps == rdeps) {
				found = true;
				break;
			}
			/*
			printf("%s ndeps: %u result: %u rdeps: %u mdeps: %u\n",
				pd->pkg, ndeps, xbps_array_count(result), rdeps, mdeps);
			*/
		}
		if (found && !xbps_match_string_in_array(result, pd->pkg)) {
			xbps_array_add_cstring_nocopy(result, pd->pkg);
			SLIST_REMOVE(&pkgdep_list, pd, pkgdep, pkgdep_entries);
		}
	}
	return result;
}

int
show_pkg_deps(struct xbps_handle *xhp, const char *pkgname, bool repomode, bool full)
{
	xbps_array_t rdeps, res;
	xbps_dictionary_t pkgd;

	if (repomode) {
		if (((pkgd = xbps_rpool_get_pkg(xhp, pkgname)) == NULL) &&
		    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pkgname)) == NULL))
			return errno;
	} else {
		if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL)
			return ENOENT;
	}
	if ((rdeps = xbps_dictionary_get(pkgd, "run_depends")))
		print_rdeps(xhp, rdeps, full, repomode);
	if (full) {
		res = sort_rdeps();
		for (unsigned int i = 0; i < xbps_array_count(res); i++) {
			const char *pkgdep;

			xbps_array_get_cstring_nocopy(res, i, &pkgdep);
			printf("%s\n", pkgdep);
		}
	}
	return 0;
}

int
show_pkg_revdeps(struct xbps_handle *xhp, const char *pkg, bool repomode)
{
	xbps_array_t revdeps;
	const char *pkgdep;

	if (repomode)
		revdeps = xbps_rpool_get_pkg_revdeps(xhp, pkg);
	else
		revdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkg);

	if (revdeps == NULL)
		return ENOENT;

	for (unsigned int i = 0; i < xbps_array_count(revdeps); i++) {
		xbps_array_get_cstring_nocopy(revdeps, i, &pkgdep);
		printf("%s\n", pkgdep);
	}
	return 0;
}
