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
#include <inttypes.h>

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-bin/defs.h"

static void __attribute__((noreturn))
usage(struct xbps_handle *xhp)
{
	if (xhp != NULL)
		xbps_end(xhp);

	fprintf(stderr,
	    "Usage: xbps-repo [options] [action] [arguments]\n"
	    "See xbps-repo(8) for more information.\n");
	exit(EXIT_FAILURE);
}

static int
repo_list_uri_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	const char *pkgidx;
	uint64_t npkgs;

	(void)arg;
	(void)done;

	prop_dictionary_get_cstring_nocopy(rpi->rpi_repod,
	    "pkgindex-version", &pkgidx);
	prop_dictionary_get_uint64(rpi->rpi_repod, "total-pkgs", &npkgs);
	printf("%s (index %s, " "%" PRIu64 " packages)\n",
	    rpi->rpi_uri, pkgidx, npkgs);

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
	struct xbps_handle *xhp;
	struct xferstat xfer;
	prop_dictionary_t pkgd;
	const char *rootdir, *cachedir, *conffile;
	int c, rv = 0;
	bool debug = false;

	rootdir = cachedir = conffile = NULL;

	while ((c = getopt(argc, argv, "C:c:dr:V")) != -1) {
		switch (c) {
		case 'C':
			conffile = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			debug = true;
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
			usage(NULL);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(NULL);

	/*
	 * Initialize XBPS subsystems.
	 */
	xhp = xbps_handle_alloc();
	if (xhp == NULL) {
		xbps_error_printf("xbps-repo: failed to allocate resources.\n");
		exit(EXIT_FAILURE);
	}
	xhp->debug = debug;
	xhp->xbps_transaction_cb = transaction_cb;
	xhp->xbps_transaction_err_cb = transaction_err_cb;
	xhp->xbps_fetch_cb = fetch_file_progress_cb;
	xhp->xfcd->cookie = &xfer;
	xhp->rootdir = rootdir;
	xhp->cachedir = cachedir;
	xhp->conffile = conffile;

	if ((rv = xbps_init(xhp)) != 0) {
		xbps_error_printf("xbps-repo: couldn't initialize library: %s\n",
		    strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (strcasecmp(argv[0], "list") == 0) {
		/* Lists all repositories registered in pool. */
		if (argc != 1)
			usage(xhp);

		rv = xbps_repository_pool_foreach(repo_list_uri_cb, NULL);
		if (rv == ENOTSUP)
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		else if (rv != 0 && rv != ENOTSUP)
			xbps_error_printf("xbps-repo: failed to initialize "
			    "rpool: %s\n", strerror(rv));

	} else if (strcasecmp(argv[0], "search") == 0) {
		/*
		 * Search for a package by looking at pkgname/short_desc
		 * by using shell style match patterns (fnmatch(3)).
		 */
		if (argc != 2)
			usage(xhp);

		rv = xbps_repository_pool_foreach(repo_search_pkgs_cb, argv[1]);
		if (rv == ENOTSUP)
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		else if (rv != 0 && rv != ENOTSUP)
			xbps_error_printf("xbps-repo: failed to initialize "
			    "rpool: %s\n", strerror(rv));

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about a binary package. */
		if (argc != 2)
			usage(xhp);

		rv = show_pkg_info_from_repolist(argv[1]);
		if (rv == ENOENT) {
			xbps_printf("Unable to locate package "
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
			usage(xhp);

		rv = show_pkg_deps_from_repolist(argv[1]);
		if (rv == ENOENT) {
			xbps_printf("Unable to locate package "
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
			usage(xhp);

		pkgd = xbps_repository_pool_dictionary_metadata_plist(argv[1],
		    XBPS_PKGFILES);
		if (pkgd == NULL) {
			if (errno == ENOTSUP) {
				xbps_error_printf("xbps-repo: no repositories "
				    "currently registered!\n");
			} else if (errno == ENOENT) {
				xbps_printf("Unable to locate package `%s' "
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
		if (argc != 2)
			usage(xhp);

		rv = repo_find_files_in_packages(argv[1]);
		if (rv == ENOTSUP) {
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		}

	} else if (strcasecmp(argv[0], "genindex") == 0) {
		/* Generates a package repository index plist file. */
		if (argc != 2)
			usage(xhp);

		rv = repo_genindex(argv[1]);

	} else if (strcasecmp(argv[0], "sync") == 0) {
		/* Syncs the pkg index for all registered remote repos */
		if (argc != 1)
			usage(xhp);

		rv = repository_sync();
		if (rv == ENOTSUP) {
			xbps_error_printf("xbps-repo: no repositories "
			    "currently registered!\n");
		}

	} else {
		usage(xhp);
	}

out:
	xbps_end(xhp);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
