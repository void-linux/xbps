/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-repo/defs.h"

static int
pkg_remove_and_purge(const char *pkgname, const char *version, bool purge)
{
	int rv = 0;

	printf("Removing package %s-%s ... ", pkgname, version);
	(void)fflush(stdout);

	if ((rv = xbps_remove_pkg(pkgname, version, false)) != 0) {
		fprintf(stderr, "\nE: unable to remove %s-%s (%s).\n",
		    pkgname, version, strerror(errno));
		return rv;
	}
	if (purge) {
		printf("purging ... ");
		(void)fflush(stdout);
		if ((rv = xbps_purge_pkg(pkgname, false)) != 0) {
			fprintf(stderr, "\nE: unable to purge %s-%s "
			    "(%s).\n", pkgname, version,
			    strerror(errno));
			return rv;
		}
	}
	printf("done.\n");

	return rv;
}

int
xbps_autoremove_pkgs(bool force, bool purge, bool only_show)
{
	prop_array_t orphans = NULL;
	prop_object_t obj = NULL;
	prop_object_iterator_t iter = NULL;
	const char *pkgver, *pkgname, *version;
	size_t cols = 0;
	int rv = 0;
	bool first = false;

	/*
	 * Removes orphan pkgs. These packages were installed
	 * as dependency and any installed package does not depend
	 * on it currently.
	 */
	orphans = xbps_find_orphan_packages();
	if (orphans == NULL)
		return errno;

	if (prop_array_count(orphans) == 0) {
		printf("There are not orphaned packages currently.\n");
		goto out;
	}

	iter = prop_array_iterator(orphans);
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	printf("The following packages were installed automatically\n"
	    "(as dependencies) and aren't needed anymore:\n\n");
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &pkgver)) {
			rv = errno;
			goto out;
		}
		cols += strlen(pkgver) + 4;
		if (cols <= 80) {
			if (first == false) {
				printf("  ");
				first = true;
			}
		} else {
			printf("\n  ");
			cols = strlen(pkgver) + 4;
		}
		printf("%s ", pkgver);
	}
	prop_object_iterator_reset(iter);
	printf("\n\n");

	if (only_show)
		goto out;

	if (!force && !xbps_noyes("Do you want to continue?")) {
		printf("Cancelled!\n");
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname)) {
			rv = errno;
			goto out;
		}
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "version", &version)) {
			rv = errno;
			goto out;
		}
		if ((rv = pkg_remove_and_purge(pkgname, version, purge)) != 0)
			goto out;
	}

out:
	if (iter)
		prop_object_iterator_release(iter);
	if (orphans)
		prop_object_release(orphans);

	return rv;
}

int
xbps_remove_installed_pkgs(int argc, char **argv, bool force, bool purge)
{
	prop_array_t reqby;
	prop_dictionary_t dict;
	size_t cols = 0;
	const char *version;
	int i, rv = 0;
	bool found = false, first = false, reqby_force = false;

	/*
	 * First check if package is required by other packages.
	 */
	for (i = 1; i < argc; i++) {
		dict = xbps_find_pkg_dict_installed(argv[i], false);
		if (dict == NULL) {
			printf("Package %s is not installed.\n", argv[i]);
			continue;
		}
		if (!prop_dictionary_get_cstring_nocopy(dict, "version",
		    &version))
			return errno;

		found = true;
		reqby = prop_dictionary_get(dict, "requiredby");
		if (reqby != NULL && prop_array_count(reqby) > 0) {
			printf("WARNING: %s-%s IS REQUIRED BY OTHER "
			    "PACKAGES!\n", argv[i], version);
			reqby_force = true;
		}
	}
	if (!found)
		return 0;

	/*
	 * Show the list of going-to-be removed packages.
	 */
	printf("The following packages will be removed:\n\n");
	for (i = 1; i < argc; i++) {
		dict = xbps_find_pkg_dict_installed(argv[i], false);
		if (dict == NULL)
			continue;
		prop_dictionary_get_cstring_nocopy(dict, "version", &version);
		cols += strlen(argv[i]) + strlen(version) + 4;
		if (cols <= 80) {
			if (first == false) {
				printf("  ");
				first = true;
			}
		} else {
			printf("\n  ");
			cols = strlen(argv[i]) + strlen(version) + 4;
		}
		printf("%s-%s ", argv[i], version);
	}
	printf("\n\n");
	if (!force && !xbps_noyes("Do you want to continue?")) {
		printf("Cancelling!\n");
		return 0;
	}
	if (reqby_force)
		printf("Forcing removal!\n");

	for (i = 1; i < argc; i++) {
		dict = xbps_find_pkg_dict_installed(argv[i], false);
		if (dict == NULL)
			continue;
		prop_dictionary_get_cstring_nocopy(dict, "version", &version);
		if ((rv = pkg_remove_and_purge(argv[i], version, purge)) != 0)
			return rv;
	}

	return 0;
}
