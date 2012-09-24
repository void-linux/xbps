/*-
 * Copyright (c) 2008-2012 Juan Romero Pardines.
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
#include <inttypes.h>

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-bin/defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-repo [options] target [arguments]\n\n"
	    "[options]\n"
	    " -C file      Full path to configuration file\n"
	    " -c cachedir  Full path to cachedir to store downloaded binpkgs\n"
	    " -d           Debug mode shown to stderr\n"
	    " -h           Print usage help\n"
	    " -o key[,key] Print package metadata keys in show target\n"
	    " -r rootdir   Full path to rootdir\n"
	    " -V           Show XBPS version\n\n"
	    "[targets]\n"
	    " clean\n"
	    "   Removes obsolete binary packages from cachedir.\n"
	    " find-files <pattern> [patterns]\n"
	    "   Print package name/version for any pattern matched.\n"
	    " index-add <repository>/foo-1.0.xbps ...\n"
	    "   Registers specified package(s) to the local repository's index.\n"
	    "   Multiple packages can be specified. An absolute path is expected.\n"
	    " index-clean <repository>\n"
	    "   Removes obsolete entries from repository's index files.\n"
	    " list\n"
	    "   List registered repositories.\n"
	    " pkg-list [repo]\n"
	    "   Print packages in repository matching `repo' URI.\n"
	    "   If `repo' not specified, all registered repositories will be used.\n"
	    " remove-obsoletes <repository>\n"
	    "   Removes obsolete packages (not registered in index any longer) from\n"
	    "   local repository \"<repository>\".\n"
	    " search <pattern> [patterns]\n"
	    "   Search for packages in repositories matching the patterns.\n"
	    " show <pkgname|pkgpattern>\n"
	    "   Print package information for `pkgname' or `pkgpattern'.\n"
	    " show-deps <pkgname|pkgpattern>\n"
	    "   Print package's required dependencies for `pkgname' or `pkgpattern'.\n"
	    " show-files <pkgname|pkgpattern>\n"
	    "   Print package's files list for `pkgname' or `pkgpattern'.\n"
	    " sync [repo]\n"
	    "   Synchronize package index file for `repo'.\n"
	    "   If `repo' not specified, all remote repositories will be used. \n\n"
	    "Refer to xbps-repo(8) for a more detailed description.\n");

	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	struct xbps_handle xh;
	struct xferstat xfer;
	struct repo_search_data rsd;
	prop_dictionary_t pkgd;
	const char *rootdir, *cachedir, *conffile, *option;
	int flags = 0, c, rv = 0;

	rootdir = cachedir = conffile = option = NULL;

	while ((c = getopt(argc, argv, "C:c:dho:r:V")) != -1) {
		switch (c) {
		case 'C':
			conffile = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'h':
			usage(false);
			break;
		case 'o':
			option = optarg;
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
			usage(true);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(true);

	/*
	 * Initialize XBPS subsystems.
	 */
	memset(&xh, 0, sizeof(xh));
	xh.flags = flags;
	xh.state_cb = state_cb;
	xh.fetch_cb = fetch_file_progress_cb;
	xh.fetch_cb_data = &xfer;
	xh.rootdir = rootdir;
	xh.cachedir = cachedir;
	xh.conffile = conffile;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("xbps-repo: couldn't initialize library: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	/*
	 * Check that we have write permission on cachedir/metadir.
	 */
	if ((strcasecmp(argv[0], "sync") == 0) ||
	    (strcasecmp(argv[0], "clean") == 0)) {
		if ((access(xh.metadir, W_OK) == -1) ||
		    (access(xh.cachedir, W_OK) == -1)) {
			if (errno != ENOENT) {
				xbps_error_printf("xbps-repo: cannot write to "
				    "cachedir/metadir: %s\n", strerror(errno));
				xbps_end(&xh);
				exit(EXIT_FAILURE);
			}
		}
	}

	if (strcasecmp(argv[0], "list") == 0) {
		/* Lists all repositories registered in pool. */
		if (argc != 1)
			usage(true);

		rv = xbps_rpool_foreach(&xh, repo_list_uri_cb, NULL);
		if (rv == ENOTSUP)
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		else if (rv != 0 && rv != ENOTSUP)
			xbps_error_printf("xbps-repo: failed to initialize "
			    "rpool: %s\n", strerror(rv));
	} else if (strcasecmp(argv[0], "pkg-list") == 0) {
		/*
		 * Only list packages for the target repository.
		 */
		if (argc < 1 || argc > 2)
			usage(true);

		rsd.arg = argv[1];
		rsd.pkgver_len = repo_find_longest_pkgver(&xh);
		rsd.maxcols = get_maxcols();

		rv = xbps_rpool_foreach(&xh, repo_pkg_list_cb, &rsd);
		if (rv == ENOTSUP)
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		else if (rv != 0)
			xbps_error_printf("xbps-repo: failed to initialize "
			    "rpool: %s\n", strerror(rv));
	} else if (strcasecmp(argv[0], "search") == 0) {
		/*
		 * Search for a package by looking at pkgname/short_desc
		 * by using shell style match patterns (fnmatch(3)).
		 */
		if (argc < 2)
			usage(true);

		rsd.npatterns = argc;
		rsd.patterns = argv;
		rsd.pkgver_len = repo_find_longest_pkgver(&xh);
		rsd.maxcols = get_maxcols();

		rv = xbps_rpool_foreach(&xh, repo_search_pkgs_cb, &rsd);
		if (rv == ENOTSUP)
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		else if (rv != 0 && rv != ENOTSUP)
			xbps_error_printf("xbps-repo: failed to initialize "
			    "rpool: %s\n", strerror(rv));
	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about a binary package. */
		if (argc != 2)
			usage(true);

		rv = show_pkg_info_from_repolist(&xh, argv[1], option);
		if (rv == ENOENT) {
			xbps_error_printf("Unable to locate package "
			    "`%s' in repository pool.\n", argv[1]);
		} else if (rv == ENOTSUP) {
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		} else if (rv != 0 && rv != ENOENT) {
			xbps_error_printf("xbps-repo: unexpected error '%s' ",
			    "searching for '%s'\n", strerror(rv), argv[1]);
		}
	} else if (strcasecmp(argv[0], "show-deps") == 0) {
		/* Shows the required run dependencies for a package. */
		if (argc != 2)
			usage(true);

		rv = show_pkg_deps_from_repolist(&xh, argv[1]);
		if (rv == ENOENT) {
			xbps_error_printf("Unable to locate package "
			    "`%s' in repository pool.\n", argv[1]);
		} else if (rv == ENOTSUP) {
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		} else if (rv != 0 && rv != ENOENT) {
			xbps_error_printf("xbps-repo: unexpected error '%s' "
			    "searching for '%s'\n", strerror(errno), argv[1]);
		}
	} else if (strcasecmp(argv[0], "show-files") == 0) {
		/* Shows the package files in a binary package */
		if (argc != 2)
			usage(true);

		pkgd = xbps_rpool_dictionary_metadata_plist(&xh, argv[1],
		    "./files.plist");
		if (pkgd == NULL) {
			if (errno == ENOTSUP) {
				xbps_error_printf("xbps-repo: no repositories "
				    "currently registered!\n");
			} else if (errno == ENOENT) {
				xbps_error_printf("Unable to locate package `%s' "
				    "in repository pool.\n", argv[1]);
			} else {
				xbps_error_printf("xbps-repo: unexpected "
				    "error '%s' searching for '%s'\n",
				    strerror(errno), argv[1]);
			}
			rv = errno;
			goto out;
		}
		rv = show_pkg_files(pkgd);
		prop_object_release(pkgd);
	} else if (strcasecmp(argv[0], "find-files") == 0) {
		/* Finds files by patterns, exact matches and components. */
		if (argc < 2)
			usage(true);

		rv = repo_find_files_in_packages(&xh, argc, argv);
		if (rv == ENOTSUP) {
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		}
	} else if (strcasecmp(argv[0], "remove-obsoletes") == 0) {
		if (argc < 2)
			usage(true);

		if ((rv = repo_remove_obsoletes(&xh, argv[1])) != 0)
			goto out;

	} else if (strcasecmp(argv[0], "index-add") == 0) {
		/* Registers a binary package into the repository's index. */
		if (argc < 2)
			usage(true);

		if ((rv = repo_index_add(&xh, argc, argv)) != 0)
			goto out;
		if ((rv = repo_index_files_add(&xh, argc, argv)) != 0)
			goto out;

	} else if (strcasecmp(argv[0], "index-clean") == 0) {
		/* Removes obsolete pkg entries from index in a repository */
		if (argc != 2)
			usage(true);

		if ((rv = repo_index_clean(&xh, argv[1])) != 0)
			goto out;

		rv = repo_index_files_clean(&xh, argv[1]);

	} else if (strcasecmp(argv[0], "sync") == 0) {
		/* Syncs the pkg index for all registered remote repos */
		if (argc < 1 || argc > 2)
			usage(true);

		rv = xbps_rpool_sync(&xh, argv[1]);
		if (rv == ENOTSUP) {
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		}
	} else if (strcasecmp(argv[0], "clean") == 0) {
		/* Cleans up cache directory */
		if (argc != 1)
			usage(true);

		rv = cachedir_clean(&xh);
	} else {
		usage(true);
	}

out:
	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
