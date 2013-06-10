/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"

static void
print_rdeps(struct xbps_handle *xhp, prop_array_t rdeps,
	    bool full, bool repo, bool origin, int *indent)
{
	prop_array_t currdeps;
	prop_dictionary_t pkgd;
	const char *pkgdep;
	size_t i;
	int j;

	if (!origin)
		(*indent)++;

	for (i = 0; i < prop_array_count(rdeps); i++) {
		prop_array_get_cstring_nocopy(rdeps, i, &pkgdep);
		if (!origin || !full)
			for (j = 0; j < *indent; j++)
				putchar(' ');

		printf("%s\n", pkgdep);
		if (!full)
			continue;

		if (repo) {
			pkgd = xbps_rpool_get_pkg(xhp, pkgdep);
			if (pkgd == NULL)
				pkgd = xbps_rpool_get_virtualpkg(xhp, pkgdep);
		} else {
			pkgd = xbps_pkgdb_get_pkg(xhp, pkgdep);
			if (pkgd == NULL)
				pkgd = xbps_pkgdb_get_virtualpkg(xhp, pkgdep);
		}
		if (pkgd != NULL) {
			currdeps = prop_dictionary_get(pkgd, "run_depends");
			if (currdeps != NULL)
				print_rdeps(xhp, currdeps,
				    full, repo, false, indent);
		}
	}
	(*indent)--;
}

int
show_pkg_deps(struct xbps_handle *xhp, const char *pkgname, bool full)
{
	prop_array_t rdeps;
	prop_dictionary_t pkgd;
	int indent = 0;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
	if (pkgd == NULL)
		return ENOENT;

	rdeps = prop_dictionary_get(pkgd, "run_depends");
	if (rdeps != NULL)
		print_rdeps(xhp, rdeps, full, false, true, &indent);

	return 0;
}

int
show_pkg_revdeps(struct xbps_handle *xhp, const char *pkg)
{
	prop_array_t reqby;
	const char *pkgdep;
	size_t i;

	if ((reqby = xbps_pkgdb_get_pkg_revdeps(xhp, pkg)) != NULL) {
		for (i = 0; i < prop_array_count(reqby); i++) {
			prop_array_get_cstring_nocopy(reqby, i, &pkgdep);
			printf("%s\n", pkgdep);
		}
	}
	return 0;
}

int
repo_show_pkg_deps(struct xbps_handle *xhp, const char *pattern, bool full)
{
	prop_array_t rdeps;
	prop_dictionary_t pkgd;
	int indent = 0;

	if (((pkgd = xbps_rpool_get_pkg(xhp, pattern)) == NULL) &&
	    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pattern)) == NULL))
		return errno;

	rdeps = prop_dictionary_get(pkgd, "run_depends");
	if (rdeps != NULL)
		print_rdeps(xhp, rdeps, full, true, true, &indent);

	return 0;
}

static int
repo_revdeps_cb(struct xbps_repo *repo, void *arg, bool *done)
{
	prop_dictionary_t pkgd;
	prop_array_t pkgdeps;
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *pkgver, *arch, *pattern = arg;

	(void)done;

	iter = prop_dictionary_iterator(repo->idx);
	assert(iter);
	while ((obj = prop_object_iterator_next(iter))) {
		pkgd = prop_dictionary_get_keysym(repo->idx, obj);
		pkgdeps = prop_dictionary_get(pkgd, "run_depends");
		if (pkgdeps == NULL || prop_array_count(pkgdeps) == 0)
			continue;

		if (xbps_match_pkgdep_in_array(pkgdeps, pattern)) {
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (xbps_pkg_arch_match(repo->xhp, arch, NULL)) {
				prop_dictionary_get_cstring_nocopy(pkgd,
				    "pkgver", &pkgver);
				printf("%s\n", pkgver);
			}
		}
	}
	prop_object_iterator_release(iter);

	return 0;
}

int
repo_show_pkg_revdeps(struct xbps_handle *xhp, const char *pkg)
{
	prop_dictionary_t pkgd;
	const char *pkgver;

	if (xbps_pkg_version(pkg))
		pkgver = pkg;
	else {
		if (((pkgd = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL))
			return ENOENT;

		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	}

	return xbps_rpool_foreach(xhp, repo_revdeps_cb, __UNCONST(pkgver));
}
