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
#include <signal.h>
#include <assert.h>
#include <unistd.h>

#include <xbps_api.h>
#include "compat.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

static void __attribute__((noreturn))
usage(struct xbps_handle *xhp)
{
	if (xhp != NULL)
		xbps_end(xhp);

	fprintf(stderr,
	    "Usage: xbps-bin [options] [target] [arguments]\n"
	    "See xbps-bin(8) for more information.\n");
	exit(EXIT_FAILURE);
}

static void __attribute__((noreturn))
cleanup(int signum)
{
	struct xbps_handle *xhp = xbps_handle_get();

	xbps_end(xhp);
	exit(signum);
}

int
main(int argc, char **argv)
{
	struct xbps_handle *xhp;
	struct xferstat xfer;
	struct list_pkgver_cb lpc;
	struct sigaction sa;
	const char *rootdir, *cachedir, *conffile, *option;
	int i, c, flags, rv;
	bool yes, purge, debug, reqby_force, force_rm_with_deps, recursive_rm;
	bool install_auto, install_manual, show_download_pkglist_url;
	bool reinstall;

	rootdir = cachedir = conffile = option = NULL;
	flags = rv = 0;
	reqby_force = yes = purge = force_rm_with_deps = false;
	recursive_rm = debug = reinstall = false;
	install_auto = install_manual = show_download_pkglist_url = false;

	while ((c = getopt(argc, argv, "AC:c:dDFfMo:pRr:Vvy")) != -1) {
		switch (c) {
		case 'A':
			install_auto = true;
			break;
		case 'C':
			conffile = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			debug = true;
			break;
		case 'D':
			show_download_pkglist_url = true;
			break;
		case 'F':
			force_rm_with_deps = true;
			break;
		case 'f':
			reinstall = true;
			flags |= XBPS_FLAG_FORCE_CONFIGURE;
			flags |= XBPS_FLAG_FORCE_REMOVE_FILES;
			break;
		case 'M':
			install_manual = true;
			break;
		case 'o':
			option = optarg;
			break;
		case 'p':
			purge = true;
			break;
		case 'R':
			recursive_rm = true;
			break;
		case 'r':
			/* To specify the root directory */
			rootdir = optarg;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'y':
			yes = true;
			break;
		case '?':
		default:
			usage(NULL);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(NULL);

	/* Specifying -A and -M is illegal */
	if (install_manual && install_auto) {
		xbps_error_printf("xbps-bin: -A and -M options cannot be "
		    "used together!\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * Register a signal handler to clean up resources used by libxbps.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = cleanup;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	/*
	 * Initialize libxbps.
	 */
	xhp = xbps_handle_alloc();
	if (xhp == NULL) {
		xbps_error_printf("xbps-bin: failed to allocate resources.\n");
		exit(EXIT_FAILURE);
	}
	xhp->debug = debug;
	xhp->state_cb = state_cb;
	xhp->fetch_cb = fetch_file_progress_cb;
	xhp->fetch_cb_data = &xfer;
	xhp->rootdir = rootdir;
	xhp->cachedir = cachedir;
	xhp->conffile = conffile;
	xhp->flags = flags;
	xhp->install_reason_manual = install_manual;
	xhp->install_reason_auto = install_auto;
	if (flags & XBPS_FLAG_VERBOSE)
		xhp->unpack_cb = unpack_progress_cb_verbose;
	else
		xhp->unpack_cb = unpack_progress_cb;

	if ((rv = xbps_init(xhp)) != 0) {
		xbps_error_printf("xbps-bin: couldn't initialize library: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (strcasecmp(argv[0], "list") == 0) {
		/* Lists packages currently registered in database. */
		if (argc < 1 || argc > 2)
			usage(xhp);

		if (xhp->regpkgdb_dictionary == NULL) {
			printf("No packages currently installed.\n");
			goto out;
		}

		lpc.check_state = true;
		lpc.state = 0;
		if (argv[1]) {
			if (strcmp(argv[1], "installed") == 0)
				lpc.state = XBPS_PKG_STATE_INSTALLED;
			if (strcmp(argv[1], "half-unpacked") == 0)
				lpc.state = XBPS_PKG_STATE_HALF_UNPACKED;
			else if (strcmp(argv[1], "unpacked") == 0)
				lpc.state = XBPS_PKG_STATE_UNPACKED;
			else if (strcmp(argv[1], "config-files") == 0)
				lpc.state = XBPS_PKG_STATE_CONFIG_FILES;
			else {
				fprintf(stderr,
				    "E: invalid state `%s'. Accepted values: "
				    "config-files, unpacked, "
				    "installed [default]\n", argv[1]);
				rv = -1;
				goto out;
			}

		}
		/*
		 * Find the longest pkgver string to pretty print the output.
		 */
		lpc.pkgver_len = find_longest_pkgver(xhp->regpkgdb_dictionary);
		rv = xbps_callback_array_iter_in_dict(xhp->regpkgdb_dictionary,
		    "packages", list_pkgs_in_dict, &lpc);

	} else if (strcasecmp(argv[0], "install") == 0) {
		/* Installs a binary package and required deps. */
		if (argc < 2)
			usage(xhp);

		for (i = 1; i < argc; i++)
			if ((rv = install_new_pkg(argv[i], reinstall)) != 0)
				goto out;

		rv = exec_transaction(yes, show_download_pkglist_url);

	} else if (strcasecmp(argv[0], "update") == 0) {
		/* Update an installed package. */
		if (argc < 2)
			usage(xhp);

		for (i = 1; i < argc; i++)
			if ((rv = update_pkg(argv[i])) != 0)
				goto out;

		rv = exec_transaction(yes, show_download_pkglist_url);

	} else if (strcasecmp(argv[0], "remove") == 0) {
		/* Removes a package. */
		if (argc < 2)
			usage(xhp);

		for (i = 1; i < argc; i++) {
			rv = remove_pkg(argv[i], purge, recursive_rm);
			if (rv == 0)
				continue;
			else if (rv != EEXIST)
				goto out;
			else
				reqby_force = true;
		}
		if (reqby_force && !force_rm_with_deps) {
			rv = EINVAL;
			goto out;
		}
		rv = exec_transaction(yes, false);

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about an installed binary package. */
		if (argc != 2)
			usage(xhp);

		rv = show_pkg_info_from_metadir(argv[1], option);
		if (rv != 0) {
			printf("Package %s not installed.\n", argv[1]);
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-files") == 0) {
		/* Shows files installed by a binary package. */
		if (argc != 2)
			usage(xhp);

		rv = show_pkg_files_from_metadir(argv[1]);
		if (rv != 0) {
			printf("Package %s not installed.\n", argv[1]);
			goto out;
		}

	} else if (strcasecmp(argv[0], "check") == 0) {
		/* Checks the integrity of an installed package. */
		if (argc != 2)
			usage(xhp);

		if (strcasecmp(argv[1], "all") == 0)
			rv = check_pkg_integrity_all();
		else
			rv = check_pkg_integrity(NULL, argv[1]);

	} else if (strcasecmp(argv[0], "autoupdate") == 0) {
		/*
		 * To update all packages currently installed.
		 */
		if (argc != 1)
			usage(xhp);

		rv = autoupdate_pkgs(yes, show_download_pkglist_url);

	} else if (strcasecmp(argv[0], "show-orphans") == 0) {
		/*
		 * Only show the package name of all currently package
		 * orphans.
		 */
		if (argc != 1)
			usage(xhp);

		rv = show_orphans();

	} else if (strcasecmp(argv[0], "autoremove") == 0) {
		/*
		 * Removes orphan pkgs. These packages were installed
		 * as dependency and any installed package does not depend
		 * on it currently.
		 */
		if (argc != 1)
			usage(xhp);

		rv = autoremove_pkgs(yes, purge);

	} else if (strcasecmp(argv[0], "purge") == 0) {
		/*
		 * Purge a package completely.
		 */
		if (argc != 2)
			usage(xhp);

		if (strcasecmp(argv[1], "all") == 0)
			rv = xbps_purge_packages();
		else
			rv = xbps_purge_pkg(argv[1], true);

	} else if (strcasecmp(argv[0], "reconfigure") == 0) {
		/*
		 * Reconfigure a package.
		 */
		if (argc != 2)
			usage(xhp);

		if (strcasecmp(argv[1], "all") == 0)
			rv = xbps_configure_packages();
		else
			rv = xbps_configure_pkg(argv[1], NULL, true, false);

	} else if (strcasecmp(argv[0], "show-deps") == 0) {
		/*
		 * Show dependencies for a package.
		 */
		if (argc != 2)
			usage(xhp);

		rv = show_pkg_deps(argv[1]);

	} else if (strcasecmp(argv[0], "list-manual") == 0) {
		/*
		 * List packages that were installed manually, not as
		 * dependencies.
		 */
		if (argc != 1)
			usage(xhp);

		rv = xbps_callback_array_iter_in_dict(xhp->regpkgdb_dictionary,
		    "packages", list_manual_pkgs, NULL);

	} else if (strcasecmp(argv[0], "show-revdeps") == 0) {
		/*
		 * Show reverse dependencies for a package.
		 */
		if (argc != 2)
			usage(xhp);

		rv = show_pkg_reverse_deps(argv[1]);

	} else if (strcasecmp(argv[0], "find-files") == 0) {
		/*
		 * Find files matched by a pattern from installed
		 * packages.
		 */
		if (argc < 2)
			usage(xhp);

		rv = find_files_in_packages(argc, argv);

	} else {
		usage(xhp);
	}

out:
	xbps_end(xhp);
	exit(rv);
}
