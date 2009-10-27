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
#include "index.h"
#include "util.h"

typedef struct repository_info {
	const char *index_version;
	uint64_t total_pkgs;
} repo_info_t;

static bool sanitize_localpath(char *, const char *);
static bool pkgindex_getinfo(prop_dictionary_t, repo_info_t *);
static void usage(void);

static void
usage(void)
{
	printf("Usage: xbps-repo [options] [action] [arguments]\n\n"
	" Available actions:\n"
        "    add, add-pkgidx, genindex, list, remove, search, show\n"
	" Actions with arguments:\n"
	"    add\t\t<URI>\n"
	"    add-pkgidx\t<repo> <binpkg>\n"
	"    genindex\t<path>\n"
	"    remove\t<URI>\n"
	"    search\t<string>\n"
	"    show\t<pkgname>\n"
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
	"    $ xbps-repo add-pkgidx /pkgdir /pkgdir/noarch/foo.xbps\n"
	"    $ xbps-repo genindex /pkgdir\n");
	exit(EXIT_FAILURE);
}

static bool
pkgindex_getinfo(prop_dictionary_t dict, repo_info_t *ri)
{
	assert(dict != NULL || ri != NULL);

	if (!prop_dictionary_get_cstring_nocopy(dict,
	    "pkgindex-version", &ri->index_version))
		return false;

	if (!prop_dictionary_get_uint64(dict, "total-pkgs",
	    &ri->total_pkgs))
		return false;

	/* Reject empty repositories, how could this happen? :-) */
	if (ri->total_pkgs <= 0)
		return false;

	return true;
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
add_repository(const char *uri, bool remote)
{
	prop_dictionary_t dict;
	repo_info_t *rinfo;
	char *plist, idxstr[PATH_MAX];
	int rv = 0;

	if (remote) {
		rv = xbps_sync_repository_pkg_index(uri);
		if (rv != 0)
			return rv;
		plist = xbps_get_pkg_index_plist(uri);
	} else {
		if (!sanitize_localpath(idxstr, uri))
			return errno;
		plist = xbps_get_pkg_index_plist(idxstr);
	}

	if (plist == NULL)
		return errno;

	dict = prop_dictionary_internalize_from_file(plist);
	if (dict == NULL) {
		printf("Repository %s does not contain any "
		    "xbps pkgindex file.\n", idxstr);
		rv = errno;
		goto out;
	}

	rinfo = malloc(sizeof(*rinfo));
	if (rinfo == NULL) {
		rv = errno;
		goto out;
	}

	if (!pkgindex_getinfo(dict, rinfo)) {
		printf("'%s' is incomplete.\n", plist);
		rv = EINVAL;
		goto out;
	}

	if (remote)
		rv = xbps_register_repository(uri);
	else
		rv = xbps_register_repository(idxstr);

	if (rv != 0) {
		printf("ERROR: couldn't register repository (%s)\n",
		    strerror(rv));
		goto out;
	}
	
	printf("Added repository at %s (%s) with %ju packages.\n",
	       uri, rinfo->index_version, rinfo->total_pkgs);

out:
	if (dict != NULL)
		prop_object_release(dict);
	if (rinfo != NULL)
		free(rinfo);
	if (plist != NULL)
		free(plist);

	return rv;
}

int
main(int argc, char **argv)
{
	char dpkgidx[PATH_MAX], *root = NULL;
	int c, rv = 0;
	bool remote_repo = false;

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

	if (strcasecmp(argv[0], "add") == 0) {
		/* Adds a new repository to the pool. */
		if (argc != 2)
			usage();

		if ((strncmp(argv[1], "http://", 7) == 0) ||
		    (strncmp(argv[1], "ftp://", 6) == 0))
			remote_repo = true;

		rv = add_repository(argv[1], remote_repo);

	} else if (strcasecmp(argv[0], "list") == 0) {
		/* Lists all repositories registered in pool. */
		if (argc != 1)
			usage();

		(void)xbps_callback_array_iter_in_repolist(
		    list_strings_sep_in_array, NULL);

	} else if ((strcasecmp(argv[0], "rm") == 0) ||
		   (strcasecmp(argv[0], "remove") == 0)) {
		/* Remove a repository from the pool. */
		if (argc != 2)
			usage();

		if (!sanitize_localpath(dpkgidx, argv[1]))
			exit(EXIT_FAILURE);

		if ((rv = xbps_unregister_repository(dpkgidx)) != 0) {
			if (rv == ENOENT)
				printf("Repository '%s' not actually "
				    "registered.\n", dpkgidx);
			else
				printf("ERROR: couldn't unregister "
				    "repository (%s)\n", strerror(rv));
			exit(EXIT_FAILURE);
		}

	} else if (strcasecmp(argv[0], "search") == 0) {
		/*
		 * Search for a package by looking at pkgname/short_desc
		 * by using shell style match patterns (fnmatch(3)).
		 */
		if (argc != 2)
			usage();

		(void)xbps_callback_array_iter_in_repolist(
		    search_string_in_pkgs, argv[1]);

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about a binary package. */
		if (argc != 2)
			usage();

		rv = xbps_callback_array_iter_in_repolist(
			show_pkg_info_from_repolist, argv[1]);
		if (rv == 0 && errno == ENOENT) {
			printf("Unable to locate package '%s' from "
			    "repository pool.\n", argv[1]);
			exit(EXIT_FAILURE);
		}

	} else if (strcasecmp(argv[0], "genindex") == 0) {
		/* Generates a package repository index plist file. */
		if (argc != 2)
			usage();

		rv = xbps_repo_genindex(argv[1]);
		exit(rv);

	} else if (strcasecmp(argv[0], "add-pkgidx") == 0) {
		/* Add a binary package into a package repository. */
		if (argc != 3)
			usage();

		rv = xbps_repo_addpkg_index(argv[1], argv[2]);
		if (rv != 0)
			exit(rv);

	} else {
		usage();
	}

	exit(EXIT_SUCCESS);
}
