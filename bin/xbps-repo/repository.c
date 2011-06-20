/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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
#include <limits.h>
#include <libgen.h>
#include <assert.h>

#include <xbps_api.h>
#include "../xbps-bin/defs.h"
#include "defs.h"
#include "config.h"

struct repoinfo {
	char *pkgidxver;
	uint64_t totalpkgs;
};

static struct repoinfo *
pkgindex_verify(const char *plist, const char *uri)
{
	struct repoinfo *rpi = NULL;
	prop_dictionary_t d;
	int rv = 0;

	assert(plist != NULL);

	d = prop_dictionary_internalize_from_zfile(plist);
	if (d == NULL) {
		xbps_error_printf("xbps-repo: failed to add `%s' "
		    "repository: %s\n", uri, strerror(errno));
		return NULL;
	}

	if ((rpi = malloc(sizeof(*rpi))) == NULL) {
		rv = errno;
		goto out;
	}

	if (!prop_dictionary_get_cstring(d,
	    "pkgindex-version", &rpi->pkgidxver)) {
		xbps_error_printf("xbps-repo: missing 'pkgindex-version' "
		    "object!\n");
		rv = errno;
		goto out;
	}

	if (!prop_dictionary_get_uint64(d, "total-pkgs",
	    &rpi->totalpkgs)) {
		xbps_error_printf("xbps-repo: missing 'total-pkgs' object!\n");
		rv = errno;
		goto out;
	}

	/* Reject empty repositories, how could this happen? :-) */
	if (rpi->totalpkgs == 0) {
		xbps_error_printf("xbps-repo: `%s' empty package list!\n", uri);
		rv = EINVAL;
		goto out;
	}

out:
	prop_object_release(d);
	if (rv != 0) {
		xbps_error_printf("xbps-repo: removing incorrect "
		    "pkg-index file for `%s'.\n", uri);
		(void)remove(plist);
		if (rpi) {
			free(rpi);
			rpi = NULL;
		}
	}
	return rpi;
}

int
show_pkg_info_from_repolist(const char *pkgname)
{
	prop_dictionary_t pkgd;

	pkgd = xbps_repository_pool_find_pkg(pkgname, false, false);
	if (pkgd == NULL)
		return errno;

	show_pkg_info(pkgd);
	prop_object_release(pkgd);

	return 0;
}

int
show_pkg_deps_from_repolist(const char *pkgname)
{
	prop_dictionary_t pkgd;
	const char *ver, *repoloc;

	pkgd = xbps_repository_pool_find_pkg(pkgname, false, false);
	if (pkgd == NULL)
		return errno;

	prop_dictionary_get_cstring_nocopy(pkgd, "version", &ver);
	prop_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc);

	printf("Repository %s [pkgver: %s]\n", repoloc, ver);
	(void)xbps_callback_array_iter_in_dict(pkgd,
	    "run_depends", list_strings_sep_in_array, NULL);

	prop_object_release(pkgd);
	return 0;
}

static int
repo_sync_pkg_index_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repoinfo *rp;
	char *plist;
	int rv = 0;

	(void)arg;
	(void)done;

	if (!xbps_check_is_repository_uri_remote(rpi->rpi_uri))
		return 0;

	printf("Synchronizing package index for `%s' ...\n", rpi->rpi_uri);
	rv = xbps_repository_sync_pkg_index(rpi->rpi_uri);
	if (rv == -1) {
		xbps_error_printf("xbps-repo: failed to sync `%s': (%s %s)\n",
		    rpi->rpi_uri, strerror(rv), xbps_fetch_error_string());
		return rv;
	} else if (rv == 0) {
		printf("Package index file is already up to date.\n");
		return 0;
	}
	if ((plist = xbps_pkg_index_plist(rpi->rpi_uri)) == NULL)
		return EINVAL;

	if ((rp = pkgindex_verify(plist, rpi->rpi_uri)) == NULL)
		return errno;

	printf("Updated package index at %s (v%s) with %ju packages.\n",
	    rpi->rpi_uri, rp->pkgidxver, rp->totalpkgs);
	free(rp);
	free(plist);

	return 0;
}

int
repository_sync(void)
{
	return xbps_repository_pool_foreach(repo_sync_pkg_index_cb, NULL);
}
