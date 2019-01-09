/*-
 * Copyright (c) 2014-2015 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

struct pkgdep {
	SLIST_ENTRY(pkgdep) pkgdep_entries;
	const char *pkg;
	xbps_array_t rdeps;
};

static SLIST_HEAD(pkgdep_head, pkgdep) pkgdep_list =
    SLIST_HEAD_INITIALIZER(pkgdep_list);

static xbps_dictionary_t pkgdep_pvmap;

static int
collect_rdeps(struct xbps_handle *xhp, xbps_dictionary_t pkgd, bool rpool)
{
	xbps_array_t rdeps, currdeps, provides = NULL;
	struct pkgdep *pd;
	const char *pkgver;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	assert(pkgver);
	rdeps = xbps_dictionary_get(pkgd, "run_depends");
	provides = xbps_dictionary_get(pkgd, "provides");

	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++) {
		xbps_dictionary_t curpkgd;
		const char *curdep, *curpkgver;
		char *curdepname;
		bool virtual = false, found = false;

		xbps_array_get_cstring_nocopy(rdeps, i, &curdep);
		if (rpool) {
			if ((curpkgd = xbps_rpool_get_pkg(xhp, curdep)) == NULL) {
				curpkgd = xbps_rpool_get_virtualpkg(xhp, curdep);
				virtual = true;
			}
		} else {
			if ((curpkgd = xbps_pkgdb_get_pkg(xhp, curdep)) == NULL) {
				curpkgd = xbps_pkgdb_get_virtualpkg(xhp, curdep);
				virtual = true;
			}
		}
		if (curpkgd == NULL) {
			xbps_dbg_printf(xhp, "%s: cannot find `%s' dependency\n",
			    __func__, curdep);
			return ENOENT;
		}
		if (((curdepname = xbps_pkgpattern_name(curdep)) == NULL) &&
		    ((curdepname = xbps_pkg_name(curdep)) == NULL))
			return EINVAL;

		xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &curpkgver);
		currdeps = xbps_dictionary_get(curpkgd, "run_depends");

		if (provides && xbps_match_pkgname_in_array(provides, curdepname)) {
			xbps_dbg_printf(xhp, "%s: ignoring dependency %s "
			    "already in provides\n", pkgver, curdep);
			free(curdepname);
			continue;
		}
		if (virtual) {
			xbps_dictionary_set_cstring_nocopy(pkgdep_pvmap, curdepname, pkgver);
		}
		free(curdepname);
		/* uniquify dependencies, sorting will be done later */
		SLIST_FOREACH(pd, &pkgdep_list, pkgdep_entries) {
			if (strcmp(pd->pkg, curpkgver) == 0) {
				found = true;
				break;
			}
		}
		if (!found) {
			pd = malloc(sizeof(*pd));
			assert(pd);
			pd->pkg = curpkgver;
			pd->rdeps = xbps_array_copy(currdeps);
			SLIST_INSERT_HEAD(&pkgdep_list, pd, pkgdep_entries);
			if (xbps_array_count(currdeps)) {
				int rv;

				if ((rv = collect_rdeps(xhp, curpkgd, rpool)) != 0)
					return rv;
			}
		}
	}
	return 0;
}

static xbps_array_t
sortfulldeptree(void)
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
				if (((pkgname = xbps_pkgpattern_name(pkgdep)) == NULL) &&
				    ((pkgname = xbps_pkg_name(pkgdep)) == NULL))
					return NULL;

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
				free(pkgname);
			}
			if (mdeps == rdeps) {
				found = true;
				break;
			}
		}
		if (found && !xbps_match_string_in_array(result, pd->pkg)) {
			xbps_array_add_cstring_nocopy(result, pd->pkg);
			SLIST_REMOVE(&pkgdep_list, pd, pkgdep, pkgdep_entries);
			free(pd);
		}
	}
	return result;
}

xbps_array_t HIDDEN
xbps_get_pkg_fulldeptree(struct xbps_handle *xhp, const char *pkg, bool rpool)
{
	xbps_dictionary_t pkgd;
	int rv;

	if (rpool) {
		if (((pkgd = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL))
			return NULL;
	} else {
		if (((pkgd = xbps_pkgdb_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkgd = xbps_pkgdb_get_virtualpkg(xhp, pkg)) == NULL))
			return NULL;
	}
	if (pkgdep_pvmap == NULL)
		pkgdep_pvmap = xbps_dictionary_create();

	if ((rv = collect_rdeps(xhp, pkgd, rpool)) != 0)
		return NULL;

	return sortfulldeptree();
}
