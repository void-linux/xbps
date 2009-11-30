/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"

int
pkgindex_verify(const char *plist, const char *uri, bool only_sync)
{
	prop_dictionary_t d;
	const char *pkgidx_version;
	uint64_t total_pkgs;
	int rv = 0;

	assert(plist != NULL);

	d = prop_dictionary_internalize_from_file(plist);
	if (d == NULL) {
		printf("E: repository %s does not contain any "
		    "xbps pkgindex file.\n", uri);
		return errno;
	}

	if (!prop_dictionary_get_cstring_nocopy(d,
	    "pkgindex-version", &pkgidx_version)) {
		printf("E: missing 'pkgindex-version' object!\n");
		rv = errno;
		goto out;
	}

	if (!prop_dictionary_get_uint64(d, "total-pkgs", &total_pkgs)) {
		printf("E: missing 'total-pkgs' object!\n");
		rv = errno;
		goto out;
	}

	/* Reject empty repositories, how could this happen? :-) */
	if (total_pkgs == 0) {
		printf("E: empty package list!\n");
		rv = EINVAL;
		goto out;
	}

	printf("%s package index at %s (v%s) with %ju packages.\n",
	    only_sync ? "Updated" : "Added", uri, pkgidx_version, total_pkgs);

out:
	prop_object_release(d);
	if (rv != 0) {
		printf("W: removing incorrect pkg index file: '%s' ...\n",
		    plist);
		rv = remove(plist);
	}
	return rv;
}

bool
sanitize_url(char *buf, const char *path)
{
	char *dirnp, *basenp, *dir, *base, *tmp;
	bool rv = false;

	dir = strdup(path);
	if (dir == NULL)
		return false;

	base = strdup(path);
	if (base == NULL) {
		free(dir);
		return false;
	}

	dirnp = dirname(dir);
	if (strcmp(dirnp, ".") == 0)
		goto out;

	basenp = basename(base);
	if (strcmp(basenp, base) == 0)
		goto out;

	tmp = strncpy(buf, dirnp, PATH_MAX - 1);
	if (sizeof(*tmp) >= PATH_MAX)
		goto out;

	buf[strlen(buf) + 1] = '\0';
	if (strcmp(dirnp, "/"))
		strncat(buf, "/", 1);
	strncat(buf, basenp, PATH_MAX - strlen(buf) - 1);
	rv = true;

out:
	free(dir);
	free(base);

	return rv;
}

int
unregister_repository(const char *uri)
{
	char idxstr[PATH_MAX];
	int rv = 0;

	if (!sanitize_url(idxstr, uri))
		return errno;

	if ((rv = xbps_repository_unregister(idxstr)) != 0) {
		if (rv == ENOENT)
			printf("Repository '%s' not actually "
			    "registered.\n", idxstr);
		else
			printf("E: couldn't unregister "
			    "repository (%s)\n", strerror(rv));
	}

	return rv;
}

int
register_repository(const char *uri)
{
	char *plist, idxstr[PATH_MAX];
	int rv = 0;

	if (xbps_check_is_repo_string_remote(uri)) {
		if (!sanitize_url(idxstr, uri))
			return errno;

		printf("Fetching remote package index at %s...\n", uri);
		rv = xbps_repository_sync_pkg_index(idxstr);
		if (rv == -1) {
			printf("Error: could not fetch pkg index file: %s.\n",
			    xbps_fetch_error_string());
			return rv;
		} else if (rv == 0) {
			printf("Package index file is already "
			    "up to date.\n");
			return 0;
		}

		plist = xbps_get_pkg_index_plist(idxstr);
	} else {
		if (!sanitize_url(idxstr, uri))
			return errno;

		plist = xbps_get_pkg_index_plist(idxstr);
	}

	if (plist == NULL)
		return errno;

	if ((rv = pkgindex_verify(plist, idxstr, false)) != 0)
		goto out;

	if ((rv = xbps_repository_register(idxstr)) != 0) {
		printf("ERROR: couldn't register repository (%s)\n",
		    strerror(rv));
		goto out;
	}
	
out:
	if (plist != NULL)
		free(plist);

	return rv;
}

int
show_pkg_info_from_repolist(const char *pkgname)
{
	struct repository_pool *rp;
	prop_dictionary_t repo_pkgd, pkg_propsd;
	int rv = 0;

	SIMPLEQ_FOREACH(rp, &repopool_queue, chain) {
		char *url = NULL;
		repo_pkgd = xbps_find_pkg_in_dict(rp->rp_repod,
		    "packages", pkgname);
		if (repo_pkgd == NULL) {
			if (errno && errno != ENOENT) {
				rv = errno;
				break;
			}
			continue;
		}
		url = xbps_repository_get_path_from_pkg_dict(repo_pkgd,
		    rp->rp_uri);
		if (url == NULL) {
			rv = errno;
			break;
		}
		printf("Fetching info from: %s\n", rp->rp_uri);
		pkg_propsd = xbps_repository_get_pkg_plist_dict_from_url(url,
		    XBPS_PKGPROPS);
		if (pkg_propsd == NULL) {
			free(url);
			rv = errno;
			break;
		}
		show_pkg_info_only_repo(repo_pkgd);
		show_pkg_info(pkg_propsd);
		prop_object_release(pkg_propsd);
		break;
	}

	return rv;
}

int
show_pkg_deps_from_repolist(const char *pkgname)
{
	struct repository_pool *rd;
	prop_dictionary_t pkgd;
	const char *ver;
	int rv = 0;

	SIMPLEQ_FOREACH(rd, &repopool_queue, chain) {
		pkgd = xbps_find_pkg_in_dict(rd->rp_repod, "packages", pkgname);
		if (pkgd == NULL) {
			if (errno != ENOENT) {
				rv = errno;
				break;
			}
			continue;
		}
		if (!prop_dictionary_get_cstring_nocopy(pkgd,
		    "version", &ver)) {
			rv = errno;
			break;
		}
		printf("Repository %s [pkgver: %s]\n", rd->rp_uri, ver);
		(void)xbps_callback_array_iter_in_dict(pkgd,
		    "run_depends", list_strings_sep_in_array, NULL);
	}

	return rv;
}

int
repository_sync(void)
{
	struct repository_pool *rp;
	char *plist;
	int rv = 0;

	SIMPLEQ_FOREACH(rp, &repopool_queue, chain) {
		if (!xbps_check_is_repo_string_remote(rp->rp_uri))
			continue;

		printf("Syncing package index from: %s\n", rp->rp_uri);
		rv = xbps_repository_sync_pkg_index(rp->rp_uri);
		if (rv == -1) {
			printf("Failed! returned: %s\n",
			    xbps_fetch_error_string());
			break;
		} else if (rv == 0) {
			printf("Package index file is already up to date.\n");
			continue;
		}
		if ((plist = xbps_get_pkg_index_plist(rp->rp_uri)) == NULL) {
			rv = EINVAL;
			break;
		}
		(void)pkgindex_verify(plist, rp->rp_uri, true);
		free(plist);
	}

	return rv;
}
