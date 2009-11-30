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

static void usage(void);

static void
usage(void)
{
	printf("Usage: xbps-repo [options] [action] [arguments]\n\n"
	" Available actions:\n"
        "    add, genindex, list, remove, search, show, show-deps,\n"
	"    show-files, sync\n"
	" Actions with arguments:\n"
	"    add\t\t<URI>\n"
	"    genindex\t<path>\n"
	"    remove\t<URI>\n"
	"    search\t<string>\n"
	"    show\t<pkgname>\n"
	"    show-deps\t<pkgname>\n"
	"    show-files\t<pkgname>\n"
	" Options shared by all actions:\n"
	"    -c\t\t<cachedir>\n"
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

int
main(int argc, char **argv)
{
	prop_dictionary_t pkgd;
	struct repository_pool *rpool;
	char *root;
	int c, rv = 0;

	while ((c = getopt(argc, argv, "Vcr:")) != -1) {
		switch (c) {
		case 'c':
			xbps_set_cachedir(optarg);
			break;
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

	if ((rv = xbps_repository_pool_init()) != 0) {
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

		rv = register_repository(argv[1]);

	} else if (strcasecmp(argv[0], "list") == 0) {
		/* Lists all repositories registered in pool. */
		if (argc != 1)
			usage();

		SIMPLEQ_FOREACH(rpool, &repopool_queue, chain)
			printf("%s\n", rpool->rp_uri);

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

		SIMPLEQ_FOREACH(rpool, &repopool_queue, chain) {
			printf("From %s repository ...\n", rpool->rp_uri);
			(void)xbps_callback_array_iter_in_dict(rpool->rp_repod,
			    "packages", show_pkg_namedesc, argv[1]);
		}

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about a binary package. */
		if (argc != 2)
			usage();

		rv = show_pkg_info_from_repolist(argv[1]);
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

		rv = show_pkg_deps_from_repolist(argv[1]);
		if (rv == 0 && errno == ENOENT) {
			printf("Unable to locate package '%s' from "
			    "repository pool.\n", argv[1]);
			rv = EINVAL;
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-files") == 0) {
		/* Shows the package files in a binary package */
		if (argc != 2)
			usage();

		pkgd = xbps_get_pkg_plist_dict_from_repo(argv[1],
		    XBPS_PKGFILES);
		if (pkgd == NULL) {
			printf("E: couldn't read %s: %s.\n", XBPS_PKGFILES,
			    strerror(errno));
			rv = errno;
			goto out;
		}
		rv = show_pkg_files(pkgd);
		prop_object_release(pkgd);

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
	xbps_repository_pool_release();
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
