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

static bool sanitize_localpath(char *, const char *);
static int  pkgindex_verify(const char *, const char *, bool);
static void usage(void);

static void
usage(void)
{
	printf("Usage: xbps-repo [options] [action] [arguments]\n\n"
	" Available actions:\n"
        "    add, genindex, list, remove, search, show, show-deps, sync\n"
	" Actions with arguments:\n"
	"    add\t\t<URI>\n"
	"    genindex\t<path>\n"
	"    remove\t<URI>\n"
	"    search\t<string>\n"
	"    show\t<pkgname>\n"
	"    show-deps\t<pkgname>\n"
	" Options shared by all actions:\n"
	"    -r\t\t<rootdir>\n"
	"    -V\t\tPrints xbps release version\n"
	"\n"
	" Examples:\n"
	"    $ xbps-repo add /path/to/directory\n"
	"    $ xbps-repo add http://www.location.org/xbps-repo\n"
	"    $ xbps-repo list\n"
	"    $ xbps-repo remove /path/to/directory\n"
	"    $ xbps-repo search klibc\n"
	"    $ xbps-repo show klibc\n"
	"    $ xbps-repo genindex /pkgdir\n");
	exit(EXIT_FAILURE);
}

static int
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

static bool
sanitize_localpath(char *buf, const char *path)
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

static int
add_repository(const char *uri)
{
	char *plist, idxstr[PATH_MAX];
	int rv = 0;

	if (xbps_check_is_repo_string_remote(uri)) {
		if (!sanitize_localpath(idxstr, uri))
			return errno;

		printf("Fetching remote package index at %s...\n", uri);
		rv = xbps_sync_repository_pkg_index(idxstr);
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
		if (!sanitize_localpath(idxstr, uri))
			return errno;

		plist = xbps_get_pkg_index_plist(idxstr);
	}

	if (plist == NULL)
		return errno;

	if ((rv = pkgindex_verify(plist, idxstr, false)) != 0)
		goto out;

	if ((rv = xbps_register_repository(idxstr)) != 0) {
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
main(int argc, char **argv)
{
	prop_dictionary_t pkgd;
	struct repository_data *rdata;
	const char *pkgver;
	char dpkgidx[PATH_MAX], *plist, *root;
	int c, rv = 0;

	while ((c = getopt(argc, argv, "Vr:")) != -1) {
		switch (c) {
		case 'r':
			/* To specify the root directory */
			root = optarg;
			xbps_set_rootdir(root);
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if ((rv = xbps_prepare_repolist_data()) != 0) {
		if (rv != ENOENT) {
			printf("E: cannot get repository list pool! %s\n",
			    strerror(rv));
			exit(EXIT_FAILURE);
		}
	}

	if (strcasecmp(argv[0], "add") == 0) {
		/* Adds a new repository to the pool. */
		if (argc != 2)
			usage();

		rv = add_repository(argv[1]);

	} else if (strcasecmp(argv[0], "list") == 0) {
		/* Lists all repositories registered in pool. */
		if (argc != 1)
			usage();

		SIMPLEQ_FOREACH(rdata, &repodata_queue, chain)
			printf("%s\n", rdata->rd_uri);

	} else if ((strcasecmp(argv[0], "rm") == 0) ||
		   (strcasecmp(argv[0], "remove") == 0)) {
		/* Remove a repository from the pool. */
		if (argc != 2)
			usage();

		if (!sanitize_localpath(dpkgidx, argv[1])) {
			rv = EINVAL;
			goto out;
		}

		if ((rv = xbps_unregister_repository(dpkgidx)) != 0) {
			if (rv == ENOENT)
				printf("Repository '%s' not actually "
				    "registered.\n", dpkgidx);
			else
				printf("ERROR: couldn't unregister "
				    "repository (%s)\n", strerror(rv));
		}

	} else if (strcasecmp(argv[0], "search") == 0) {
		/*
		 * Search for a package by looking at pkgname/short_desc
		 * by using shell style match patterns (fnmatch(3)).
		 */
		if (argc != 2)
			usage();

		SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
			printf("From %s repository ...\n", rdata->rd_uri);
			(void)xbps_callback_array_iter_in_dict(rdata->rd_repod,
			    "packages", show_pkg_namedesc, argv[1]);
		}

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about a binary package. */
		if (argc != 2)
			usage();

		SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
			pkgd = xbps_find_pkg_in_dict(rdata->rd_repod,
			    "packages", argv[1]);
			if (pkgd == NULL) {
				errno = ENOENT;
				continue;
			}
			printf("Repository: %s\n", rdata->rd_uri);
			show_pkg_info(pkgd);
			break;
		}
		if (rv == 0 && errno == ENOENT) {
			printf("Unable to locate package '%s' from "
			    "repository pool.\n", argv[1]);
			rv = EINVAL;
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-deps") == 0) {
		/* Shows the required run dependencies for a package. */
		if (argc != 2)
			usage();

		SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
			pkgd = xbps_find_pkg_in_dict(rdata->rd_repod,
			    "packages", argv[1]);
			if (pkgd == NULL) {
				errno = ENOENT;
				continue;
			}
			if (!prop_dictionary_get_cstring_nocopy(pkgd,
			    "version", &pkgver)) {
				rv = errno;
				goto out;
			}
			printf("Repository %s [pkgver: %s]\n",
			    rdata->rd_uri, pkgver);
			(void)xbps_callback_array_iter_in_dict(pkgd,
			    "run_depends", list_strings_sep_in_array, NULL);
		}
		if (rv == 0 && errno == ENOENT) {
			printf("Unable to locate package '%s' from "
			    "repository pool.\n", argv[1]);
			rv = EINVAL;
			goto out;
		}

	} else if (strcasecmp(argv[0], "genindex") == 0) {
		/* Generates a package repository index plist file. */
		if (argc != 2)
			usage();

		rv = xbps_repo_genindex(argv[1]);

	} else if (strcasecmp(argv[0], "sync") == 0) {
		/* Syncs the pkg index for all registered remote repos */
		if (argc != 1)
			usage();

		/*
		 * Iterate over repository pool.
		 */
		SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
			const char *uri = rdata->rd_uri;
			if (xbps_check_is_repo_string_remote(uri)) {
				printf("Syncing package index from: %s\n", uri);
				rv = xbps_sync_repository_pkg_index(uri);
				if (rv == -1) {
					printf("Failed! returned: %s\n",
					    xbps_fetch_error_string());
					goto out;
				} else if (rv == 0) {
					printf("Package index file is already "
					    "up to date.\n");
					continue;
				}
				plist = xbps_get_pkg_index_plist(uri);
				if (plist == NULL) {
					rv = EINVAL;
					goto out;
				}
				(void)pkgindex_verify(plist, uri, true);
				free(plist);
			}
		}

	} else {
		usage();
	}

out:
	xbps_release_repolist_data();
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
