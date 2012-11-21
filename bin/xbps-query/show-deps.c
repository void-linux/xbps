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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

int
show_pkg_deps(struct xbps_handle *xhp, const char *pkgname)
{
	prop_dictionary_t propsd;
	int rv = 0;

	assert(pkgname != NULL);

	/*
	 * Check for props.plist metadata file.
	 */
	propsd = xbps_metadir_get_pkgd(xhp, pkgname);
	if (propsd == NULL)
		return ENOENT;

	rv = xbps_callback_array_iter_in_dict(xhp, propsd, "run_depends",
	     list_strings_sep_in_array, NULL);

	return rv;
}

int
show_pkg_revdeps(struct xbps_handle *xhp, const char *pkgname)
{
	prop_dictionary_t pkgd;
	int rv = 0;

	pkgd = xbps_find_virtualpkg_dict_installed(xhp, pkgname, false);
	if (pkgd == NULL) {
		pkgd = xbps_find_pkg_dict_installed(xhp, pkgname, false);
		if (pkgd == NULL)
			return ENOENT;
	}
	rv = xbps_callback_array_iter_in_dict(xhp, pkgd, "requiredby",
	    list_strings_sep_in_array, NULL);

	return rv;
}

int
repo_show_pkg_deps(struct xbps_handle *xhp, const char *pattern)
{
	prop_dictionary_t pkgd;

	if (xbps_pkgpattern_version(pattern))
		pkgd = xbps_rpool_find_pkg(xhp, pattern, true, false);
	else
		pkgd = xbps_rpool_find_pkg(xhp, pattern, false, true);

	if (pkgd == NULL)
		return errno;

	(void)xbps_callback_array_iter_in_dict(xhp, pkgd,
	    "run_depends", list_strings_sep_in_array, NULL);

	return 0;
}

static int
repo_revdeps_cb(struct xbps_handle *xhp,
		struct xbps_rpool_index *rpi,
		void *arg,
		bool *done)
{
	prop_dictionary_t pkgd;
	prop_array_t pkgdeps;
	const char *pkgver, *arch, *pattern = arg;
	size_t i;

	(void)done;

	for (i = 0; i < prop_array_count(rpi->repo); i++) {
		pkgd = prop_array_get(rpi->repo, i);
		pkgdeps = prop_dictionary_get(pkgd, "run_depends");
		if (pkgdeps == NULL || prop_array_count(pkgdeps) == 0)
			continue;

		if (xbps_match_pkgdep_in_array(pkgdeps, pattern)) {
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (xbps_pkg_arch_match(xhp, arch, NULL)) {
				prop_dictionary_get_cstring_nocopy(pkgd,
				    "pkgver", &pkgver);
				printf("%s\n", pkgver);
			}
		}
	}

	return 0;
}

int
repo_show_pkg_revdeps(struct xbps_handle *xhp, const char *pkg)
{
	char *pattern;

	if (xbps_pkg_version(pkg))
		pattern = strdup(pkg);
	else
		pattern = xbps_xasprintf("%s-9999999", pkg);

	return xbps_rpool_foreach(xhp, repo_revdeps_cb, pattern);
}
