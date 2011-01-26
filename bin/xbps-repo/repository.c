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

static const char *
sanitize_url(const char *path)
{
	static char buf[PATH_MAX];
	const char *res = NULL;
	char *dirnp, *basenp, *dir, *base;
	int r = 0;

	memset(&buf, 0, sizeof(buf));

	if ((dir = strdup(path)) == NULL)
		return NULL;

	if ((base = strdup(path)) == NULL) {
		free(dir);
		return NULL;
	}

	dirnp = dirname(dir);
	if (strcmp(dirnp, ".") == 0)
		goto out;

	basenp = basename(base);
	if (strcmp(basenp, base) == 0)
		goto out;

	r = snprintf(buf, sizeof(buf) - 1, "%s/%s", dirnp, basenp);
	if (r < 0 || r >= (int)sizeof(buf) - 1)
		goto out;

	res = buf;
out:
	free(dir);
	free(base);

	return res;
}

static struct repoinfo *
pkgindex_verify(const char *plist, const char *uri)
{
	struct repoinfo *rpi = NULL;
	prop_dictionary_t d;
	int rv = 0;

	assert(plist != NULL);

	d = prop_dictionary_internalize_from_zfile(plist);
	if (d == NULL) {
		fprintf(stderr, "E: failed to add '%s' repository: %s\n",
		    uri, strerror(errno));
		return NULL;
	}

	if ((rpi = malloc(sizeof(*rpi))) == NULL) {
		rv = errno;
		goto out;
	}

	if (!prop_dictionary_get_cstring(d,
	    "pkgindex-version", &rpi->pkgidxver)) {
		fprintf(stderr,
		    "E: missing 'pkgindex-version' object!\n");
		rv = errno;
		goto out;
	}

	if (!prop_dictionary_get_uint64(d, "total-pkgs",
	    &rpi->totalpkgs)) {
		fprintf(stderr, "E: missing 'total-pkgs' object!\n");
		rv = errno;
		goto out;
	}

	/* Reject empty repositories, how could this happen? :-) */
	if (rpi->totalpkgs == 0) {
		fprintf(stderr, "E: empty package list!\n");
		rv = EINVAL;
		goto out;
	}

out:
	prop_object_release(d);
	if (rv != 0) {
		fprintf(stderr,
		    "W: removing incorrect pkg index file: '%s' ...\n",
		    plist);
		(void)remove(plist);
		if (rpi) {
			free(rpi);
			rpi = NULL;
		}
	}
	return rpi;
}

int
unregister_repository(const char *uri)
{
	const char *idxstr = NULL;
	int rv = 0;

	if ((idxstr = sanitize_url(uri)) == NULL)
		return errno;

	if ((rv = xbps_repository_unregister(idxstr)) == 0)
		return 0;

	if (rv == ENOENT) {
		fprintf(stderr, "Repository '%s' not actually "
		    "registered.\n", idxstr);
	} else {
		fprintf(stderr, "E: couldn't unregister "
		    "repository (%s)\n", strerror(rv));
	}

	return rv;
}

int
register_repository(const char *uri)
{
	struct repoinfo *rpi = NULL;
	struct xbps_fetch_progress_data xfpd;
	const char *idxstr = NULL;
	char *metadir, *plist;
	int rv = 0;

	if ((idxstr = sanitize_url(uri)) == NULL)
		return errno;

	if (xbps_check_is_repo_string_remote(idxstr)) {
		printf("Fetching remote package index at %s...\n", idxstr);
		rv = xbps_repository_sync_pkg_index(idxstr,
		    fetch_file_progress_cb, &xfpd);
		if (rv == -1) {
			fprintf(stderr,
			    "E: could not fetch pkg index file: %s.\n",
			    xbps_fetch_error_string());
			return rv;
		} else if (rv == 0) {
			printf("Package index file is already "
			    "up to date.\n");
			return 0;
		}

		plist = xbps_get_pkg_index_plist(idxstr);
	} else {
		/*
		 * Create metadir if necessary.
		 */
		metadir = xbps_xasprintf("%s/%s", xbps_get_rootdir(),
		    XBPS_META_PATH);
		if (metadir == NULL)
			return errno;

		if (xbps_mkpath(metadir, 0755) == -1) {
			fprintf(stderr,
			    "E: couldn't create metadata dir! (%s)\n",
			    strerror(errno));
			free(metadir);
			return EXIT_FAILURE;
		}
		free(metadir);
		plist = xbps_get_pkg_index_plist(idxstr);
	}

	if (plist == NULL)
		return errno;

	if ((rpi = pkgindex_verify(plist, idxstr)) == NULL)
		goto out;

	if ((rv = xbps_repository_register(idxstr)) != 0) {
		if (rv == EEXIST) {
			fprintf(stderr, "W: repository already registered.\n");
		} else {
			fprintf(stderr, "E: couldn't register repository "
			    "(%s)\n", strerror(errno));
		}
		goto out;
	}

	printf("Added package index at %s (v%s) with %ju packages.\n",
	    idxstr, rpi->pkgidxver, rpi->totalpkgs);

out:
	if (rpi != NULL) {
		if (rpi->pkgidxver != NULL)
			free(rpi->pkgidxver);
		free(rpi);
	}
	if (plist != NULL)
		free(plist);

	return rv;
}

int
show_pkg_info_from_repolist(const char *pkgname)
{
	prop_dictionary_t pkgd, pkg_propsd;
	const char *repoloc;
	char *url = NULL;

	pkgd = xbps_repository_pool_find_pkg(pkgname, false, false);
	if (pkgd == NULL)
		return errno;

	prop_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc);
	url = xbps_get_binpkg_repo_uri(pkgd, repoloc);
	if (url == NULL) {
		prop_object_release(pkgd);
		return errno;
	}
	printf("Fetching info from: %s\n", repoloc);
	pkg_propsd =
	    xbps_repository_get_pkg_plist_dict_from_url(url, XBPS_PKGPROPS);
	if (pkg_propsd == NULL) {
		free(url);
		prop_object_release(pkgd);
		return errno;
	}
	free(url);
	show_pkg_info(pkgd, true);
	show_pkg_info(pkg_propsd, false);
	prop_object_release(pkg_propsd);
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
	struct xbps_fetch_progress_data xfpd;
	struct repoinfo *rp;
	char *plist;
	int rv = 0;

	(void)arg;
	(void)done;

	if (!xbps_check_is_repo_string_remote(rpi->rpi_uri))
		return 0;

	printf("Syncing package index from: %s\n", rpi->rpi_uri);
	rv = xbps_repository_sync_pkg_index(rpi->rpi_uri,
	    fetch_file_progress_cb, &xfpd);
	if (rv == -1) {
		fprintf(stderr, "E: returned: %s\n", xbps_fetch_error_string());
		return rv;
	} else if (rv == 0) {
		printf("Package index file is already up to date.\n");
		return 0;
	}
	if ((plist = xbps_get_pkg_index_plist(rpi->rpi_uri)) == NULL)
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
