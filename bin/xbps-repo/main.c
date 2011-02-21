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
#include <strings.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-bin/defs.h"

static void __attribute__((noreturn))
usage(void)
{
	xbps_end();
	fprintf(stderr,
	    "Usage: xbps-repo [options] [action] [arguments]\n"
	    "See xbps-repo(8) for more information.\n");
	exit(EXIT_FAILURE);
}

static int
repo_list_uri_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	(void)arg;
	(void)done;

	printf("%s\n", rpi->rpi_uri);

	return 0;
}

static int
repo_search_pkgs_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repo_search_data rsd;
	(void)done;

	rsd.pattern = arg;
	rsd.pkgver_len = find_longest_pkgver(rpi->rpi_repod);

	printf("From %s repository ...\n", rpi->rpi_uri);
	(void)xbps_callback_array_iter_in_dict(rpi->rpi_repod,
	    "packages", show_pkg_namedesc, &rsd);

	return 0;
}

int
main(int argc, char **argv)
{
	struct xbps_handle xh;
	struct xbps_fetch_progress_data xfpd;
	prop_dictionary_t pkgd;
	const char *rootdir, *cachedir;
	int c, rv = 0;
	bool with_debug = false;

	rootdir = cachedir = NULL;

	while ((c = getopt(argc, argv, "Vc:dr:")) != -1) {
		switch (c) {
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			with_debug = true;
			break;
		case 'r':
			/* To specify the root directory */
			rootdir = optarg;
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

	/*
	 * Initialize the function callbacks and debug in libxbps.
	 */
	memset(&xh, 0, sizeof(xh));
	xh.with_debug = with_debug;
	xh.xbps_fetch_cb = fetch_file_progress_cb;
	xh.xfpd = &xfpd;
	xh.rootdir = rootdir;
	xh.cachedir = cachedir;
	xbps_init(&xh);

	if ((rv = xbps_repository_pool_init()) != 0) {
		if (rv != ENOENT) {
			xbps_error_printf("xbps-repo: failed to initialize "
			    "repository pool: %s\n", strerror(rv));
			exit(EXIT_FAILURE);
		}
	}

	if (strcasecmp(argv[0], "add") == 0) {
		/* Adds a new repository to the pool. */
		if (argc != 2)
			usage();

		rv = register_repository(argv[1]);

	} else if (strcasecmp(argv[0], "list") == 0) {
		/* Lists all repositories registered in pool. */
		if (argc != 1)
			usage();

		xbps_repository_pool_foreach(repo_list_uri_cb, NULL);

	} else if ((strcasecmp(argv[0], "rm") == 0) ||
		   (strcasecmp(argv[0], "remove") == 0)) {
		/* Remove a repository from the pool. */
		if (argc != 2)
			usage();

		rv = unregister_repository(argv[1]);

	} else if (strcasecmp(argv[0], "search") == 0) {
		/*
		 * Search for a package by looking at pkgname/short_desc
		 * by using shell style match patterns (fnmatch(3)).
		 */
		if (argc != 2)
			usage();

		xbps_repository_pool_foreach(repo_search_pkgs_cb, argv[1]);

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about a binary package. */
		if (argc != 2)
			usage();

		rv = show_pkg_info_from_repolist(argv[1]);
		if (rv == 0 && errno == ENOENT) {
			xbps_error_printf("xbps-repo: unable to locate "
			    "`%s' from repository pool: %s\n", argv[1],
			    strerror(rv));
			rv = EINVAL;
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-deps") == 0) {
		/* Shows the required run dependencies for a package. */
		if (argc != 2)
			usage();

		rv = show_pkg_deps_from_repolist(argv[1]);
		if (rv == 0 && errno == ENOENT) {
			xbps_error_printf("xbps-repo: unable to locate "
			    "`%s' from repository pool: %s\n", argv[1],
			    strerror(rv));
			rv = EINVAL;
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-files") == 0) {
		/* Shows the package files in a binary package */
		if (argc != 2)
			usage();

		pkgd = xbps_repository_plist_find_pkg_dict(argv[1],
		    XBPS_PKGFILES);
		if (pkgd == NULL) {
			if (errno != ENOENT) {
				xbps_error_printf("xbps-repo: unexpected "
				    "error '%s' searching for '%s'\n",
				    strerror(errno), argv[1]);
			} else {
				xbps_error_printf("xbps-repo: `%s' not found "
				    "in repository pool.\n", argv[1]);
			}
			rv = errno;
			goto out;
		}
		rv = show_pkg_files(pkgd);
		prop_object_release(pkgd);

	} else if (strcasecmp(argv[0], "find-files") == 0) {
		/* Finds files by patterns, exact matches and components. */
		if (argc != 2)
			usage();

		rv = repo_find_files_in_packages(argv[1]);

	} else if (strcasecmp(argv[0], "genindex") == 0) {
		/* Generates a package repository index plist file. */
		if (argc != 2)
			usage();

		rv = xbps_repo_genindex(argv[1]);

	} else if (strcasecmp(argv[0], "sync") == 0) {
		/* Syncs the pkg index for all registered remote repos */
		if (argc != 1)
			usage();

		rv = repository_sync();

	} else {
		usage();
	}

out:
	xbps_end();
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
