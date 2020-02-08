/*-
 * Copyright (c) 2013-2015 Juan Romero Pardines.
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
 * Verify reverse dependencies for packages in transaction.
 * This will catch cases where a package update would break its reverse dependencies:
 *
 * 	- foo-1.0 is being updated to 2.0.
 * 	- baz-1.1 depends on foo<2.0.
 * 	- foo is updated to 2.0, hence baz-1.1 is currently broken.
 *
 * Abort transaction if such case is found.
 */
static bool
check_virtual_pkgs(xbps_array_t mdeps,
		   xbps_dictionary_t trans_pkgd,
		   xbps_dictionary_t rev_pkgd)
{
	xbps_array_t provides;
	bool matched = false;

	provides = xbps_dictionary_get(trans_pkgd, "provides");
	for (unsigned int i = 0; i < xbps_array_count(provides); i++) {
		xbps_array_t rundeps;
		const char *pkgver, *revpkgver, *pkgpattern;
		char pkgname[XBPS_NAME_SIZE], vpkgname[XBPS_NAME_SIZE];
		char *vpkgver = NULL, *str = NULL;

		pkgver = revpkgver = pkgpattern = NULL;

		xbps_dictionary_get_cstring_nocopy(trans_pkgd, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(rev_pkgd, "pkgver", &revpkgver);
		xbps_array_get_cstring(provides, i, &vpkgver);

		if (!xbps_pkg_name(vpkgname, sizeof(vpkgname), vpkgver)) {
			break;
		}

		rundeps = xbps_dictionary_get(rev_pkgd, "run_depends");
		for (unsigned int x = 0; x < xbps_array_count(rundeps); x++) {
			xbps_array_get_cstring_nocopy(rundeps, x, &pkgpattern);

			if ((!xbps_pkgpattern_name(pkgname, sizeof(pkgname), pkgpattern)) &&
			    (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgpattern)))
				continue;

			if (strcmp(vpkgname, pkgname)) {
				continue;
			}
			if (!strcmp(vpkgver, pkgpattern) ||
			    xbps_pkgpattern_match(vpkgver, pkgpattern)) {
				continue;
			}

			str = xbps_xasprintf("%s broken, needs '%s' virtual pkg (got `%s')",
			    revpkgver, pkgpattern, vpkgver);
			xbps_array_add_cstring(mdeps, str);
			free(str);
			matched = true;
		}
		free(vpkgver);
	}
	return matched;
}

static void
broken_pkg(xbps_array_t mdeps, const char *dep, const char *pkg, const char *trans)
{
	char *str;

	str = xbps_xasprintf("%s (%s) breaks installed pkg `%s'", pkg, trans, dep);
	xbps_array_add_cstring(mdeps, str);
	free(str);
}

void HIDDEN
xbps_transaction_revdeps(struct xbps_handle *xhp, xbps_array_t pkgs)
{
	xbps_array_t mdeps;

	mdeps = xbps_dictionary_get(xhp->transd, "missing_deps");

	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++) {
		xbps_array_t pkgrdeps;
		xbps_object_t obj;
		const char *pkgver, *tract;
		char pkgname[XBPS_NAME_SIZE];

		obj = xbps_array_get(pkgs, i);
		/*
		 * if pkg in transaction is not installed,
		 * pass to next one.
		 */
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);

		/*
		 * If pkg is on hold, pass to the next one.
		 */
		if (strcmp(tract, "hold") == 0) {
			continue;
		}

		if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			abort();
		}

		if (xbps_pkg_is_installed(xhp, pkgname) == 0) {
			continue;
		}
		/*
		 * If pkg is installed but does not have revdeps,
		 * pass to next one.
		 */
		pkgrdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkgname);
		if (!xbps_array_count(pkgrdeps)) {
			continue;
		}
		/*
		 * If pkg is ignored, pass to the next one.
		 */
		if (xbps_pkg_is_ignored(xhp, pkgver)) {
			continue;
		}

		/*
		 * Time to validate revdeps for current pkg.
		 */
		for (unsigned int x = 0; x < xbps_array_count(pkgrdeps); x++) {
			xbps_array_t rundeps;
			xbps_dictionary_t revpkgd;
			const char *curpkgver = NULL, *revpkgver, *curdep = NULL, *curtract;
			char curpkgname[XBPS_NAME_SIZE];
			char curdepname[XBPS_NAME_SIZE];
			bool found = false;

			xbps_array_get_cstring_nocopy(pkgrdeps, x, &curpkgver);

			if (!xbps_pkg_name(pkgname, sizeof(pkgname), curpkgver)) {
				abort();
			}
			if ((revpkgd = xbps_find_pkg_in_array(pkgs, pkgname, NULL))) {
				xbps_dictionary_get_cstring_nocopy(revpkgd, "transaction", &curtract);
				if (strcmp(curtract, "remove") == 0)
					revpkgd = NULL;
			}
			if (revpkgd == NULL)
				revpkgd = xbps_pkgdb_get_pkg(xhp, curpkgver);

			xbps_dictionary_get_cstring_nocopy(revpkgd, "pkgver", &revpkgver);
			/*
			 * If target pkg is being removed, all its revdeps
			 * will be broken unless those revdeps are also in
			 * the transaction.
			 */
			if (strcmp(tract, "remove") == 0) {
				if (xbps_dictionary_get(obj, "replaced")) {
					continue;
				}
				if (xbps_find_pkg_in_array(pkgs, pkgname, "remove")) {
					continue;
				}
				broken_pkg(mdeps, curpkgver, pkgver, tract);
				continue;
			}
			/*
			 * First try to match any supported virtual package.
			 */
			if (check_virtual_pkgs(mdeps, obj, revpkgd)) {
				continue;
			}
			/*
			 * Try to match real dependencies.
			 */
			rundeps = xbps_dictionary_get(revpkgd, "run_depends");
			/*
			 * Find out what dependency is it.
			 */
			if (!xbps_pkg_name(curpkgname, sizeof(curpkgname), pkgver)) {
				abort();
			}

			for (unsigned int j = 0; j < xbps_array_count(rundeps); j++) {
				xbps_array_get_cstring_nocopy(rundeps, j, &curdep);
				if ((!xbps_pkgpattern_name(curdepname, sizeof(curdepname), curdep)) &&
				    (!xbps_pkg_name(curdepname, sizeof(curdepname), curdep))) {
					abort();
				}
				if (strcmp(curdepname, curpkgname) == 0) {
					found = true;
					break;
				}
			}

			if (!found) {
				continue;
			}
			if (xbps_match_pkgdep_in_array(rundeps, pkgver)) {
				continue;
			}
			/*
			 * Installed package conflicts with package
			 * in transaction being updated, check
			 * if a new version of this conflicting package
			 * is in the transaction.
			 */
			if (xbps_find_pkg_in_array(pkgs, pkgname, "update")) {
				continue;
			}
			broken_pkg(mdeps, curpkgver, pkgver, tract);
		}

	}
}
