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
#include <signal.h>
#include <assert.h>
#include <unistd.h>

#include <xbps_api.h>
#include "compat.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

static struct xbps_handle xh;

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stderr,
	    "Usage: xbps-bin [options] target [arguments]\n\n"
	    "[options]\n"
	    " -A           Enable Automatic installation (shown as orphan)\n"
	    " -C file      Full path to configuration file\n"
	    " -c cachedir  Full path to cachedir, to store downloaded binpkgs\n"
	    " -d           Debug mode shown to stderr\n"
	    " -D           Print URLs when packages need to be downloaded\n"
	    " -F           Force package removal even if there are reverse dependencies\n"
	    " -f           Force package installation, configuration or removal\n"
	    " -h           Print usage help\n"
	    " -M           Enable Manual installation\n"
	    " -n           Dry-run mode\n"
	    " -o key[,key] Print package metadata keys in show target\n"
	    " -R           Remove recursively packages\n"
	    " -r rootdir   Full path to rootdir\n"
	    " -S           Sync repository index\n"
	    " -v           Verbose messages\n"
	    " -y           Assume yes to all questions\n"
	    " -V           Show XBPS version\n\n"
	    "[targets]\n"
	    " check <pkgname|all>\n"
	    "   Package integrity check for `pkgname' or `all' packages.\n"
	    " dist-upgrade\n"
	    "   Update all currently installed packages to newest versions.\n"
	    " find-files <pattern> [patterns]\n"
	    "   Print package name/version for any pattern matched.\n"
	    " install <pattern> [patterns]\n"
	    "   Install package by specifying pkgnames or package patterns.\n"
	    " list [state]\n"
	    "   List installed packages, and optionally matching `state'.\n"
	    "   Possible states: half-removed, half-unpacked, installed, unpacked.\n"
	    " reconfigure <pkgname|all>\n"
	    "   Reconfigure `pkgname' or `all' packages.\n"
	    " remove <pkgname> [pkgnames]\n"
	    "   Remove a list of packages.\n"
	    " remove-orphans\n"
	    "   Remove all package orphans from system.\n"
	    " show <pkgname>\n"
	    "   Print package information for `pkgname'.\n"
	    " show-deps <pkgname>\n"
	    "   Print package's required dependencies for `pkgname'.\n"
	    " show-files <pkgname>\n"
	    "   Print package's files list for `pkgname'.\n"
	    " show-orphans\n"
	    "   List all package orphans currently installed.\n"
	    " show-revdeps <pkgname>\n"
	    "   Print package's reverse dependencies for `pkgname'.\n"
	    " update <pkgname> [pkgnames]\n"
	    "   Update a list of packages by specifing its names.\n\n"
	    "Refer to xbps-bin(8) for a more detailed description.\n");

	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((noreturn))
cleanup(int signum)
{
	xbps_end(&xh);
	exit(signum);
}

int
main(int argc, char **argv)
{
	struct xferstat xfer;
	struct list_pkgver_cb lpc;
	struct sigaction sa;
	const char *rootdir, *cachedir, *conffile, *option;
	int i, c, flags, rv;
	bool rsync, yes, reqby_force, force_rm_with_deps, recursive_rm;
	bool reinstall, show_download_pkglist_url, dry_run;

	rootdir = cachedir = conffile = option = NULL;
	flags = rv = 0;
	reqby_force = rsync = yes = dry_run = force_rm_with_deps = false;
	recursive_rm = reinstall = show_download_pkglist_url = false;

	while ((c = getopt(argc, argv, "AC:c:dDFfhMno:Rr:SVvy")) != -1) {
		switch (c) {
		case 'A':
			flags |= XBPS_FLAG_INSTALL_AUTO;
			break;
		case 'C':
			conffile = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
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
		case 'h':
			usage(false);
			break;
		case 'M':
			flags |= XBPS_FLAG_INSTALL_MANUAL;
			break;
		case 'n':
			dry_run = true;
			break;
		case 'o':
			option = optarg;
			break;
		case 'R':
			recursive_rm = true;
			break;
		case 'r':
			/* To specify the root directory */
			rootdir = optarg;
			break;
		case 'S':
			rsync = true;
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
			usage(true);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage(true);

	/* Specifying -A and -M is illegal */
	if ((flags & XBPS_FLAG_INSTALL_AUTO) &&
	    (flags & XBPS_FLAG_INSTALL_MANUAL)) {
		xbps_error_printf("xbps-bin: -A and -M options cannot be "
		    "used together!\n");
		exit(EXIT_FAILURE);
	}

	/*
	 * Initialize libxbps.
	 */
	memset(&xh, 0, sizeof(xh));
	xh.state_cb = state_cb;
	xh.fetch_cb = fetch_file_progress_cb;
	xh.fetch_cb_data = &xfer;
	xh.rootdir = rootdir;
	xh.cachedir = cachedir;
	xh.conffile = conffile;
	xh.flags = flags;
	if (flags & XBPS_FLAG_VERBOSE)
		xh.unpack_cb = unpack_progress_cb_verbose;
	else
		xh.unpack_cb = unpack_progress_cb;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("xbps-bin: couldn't initialize library: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	/*
	 * Register a signal handler to clean up resources used by libxbps.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = cleanup;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	if (strcasecmp(argv[0], "list") == 0) {
		/* Lists packages currently registered in database. */
		if (argc < 1 || argc > 2)
			usage(true);

		lpc.check_state = true;
		lpc.state = 0;
		if (argv[1]) {
			if (strcmp(argv[1], "installed") == 0)
				lpc.state = XBPS_PKG_STATE_INSTALLED;
			if (strcmp(argv[1], "half-unpacked") == 0)
				lpc.state = XBPS_PKG_STATE_HALF_UNPACKED;
			else if (strcmp(argv[1], "unpacked") == 0)
				lpc.state = XBPS_PKG_STATE_UNPACKED;
			else if (strcmp(argv[1], "half-removed") == 0)
				lpc.state = XBPS_PKG_STATE_HALF_REMOVED;
			else {
				xbps_error_printf(
				    "invalid state `%s'. Accepted values: "
				    "half-removed, unpacked, half-unpacked, "
				    "installed [default]\n", argv[1]);
				rv = -1;
				goto out;
			}

		}
		/*
		 * Find the longest pkgver string to pretty print the output.
		 */
		lpc.pkgver_len = find_longest_pkgver(&xh, NULL);
		rv = xbps_pkgdb_foreach_cb(&xh, list_pkgs_in_dict, &lpc);
		if (rv == ENOENT) {
			printf("No packages currently registered.\n");
			rv = 0;
		}

	} else if (strcasecmp(argv[0], "install") == 0) {
		/* Installs a binary package and required deps. */
		if (argc < 2)
			usage(true);

		if (rsync && ((rv = xbps_rpool_sync(&xh, NULL)) != 0))
			goto out;

		for (i = 1; i < argc; i++)
			if ((rv = install_new_pkg(&xh, argv[i], reinstall)) != 0)
				goto out;

		rv = exec_transaction(&xh, yes, dry_run, show_download_pkglist_url);

	} else if (strcasecmp(argv[0], "update") == 0) {
		/* Update an installed package. */
		if (argc < 2)
			usage(true);

		if (rsync && ((rv = xbps_rpool_sync(&xh, NULL)) != 0))
			goto out;

		for (i = 1; i < argc; i++)
			if ((rv = update_pkg(&xh, argv[i])) != 0)
				goto out;

		rv = exec_transaction(&xh, yes, dry_run, show_download_pkglist_url);

	} else if (strcasecmp(argv[0], "remove") == 0) {
		/* Removes a package. */
		if (argc < 2)
			usage(true);

		for (i = 1; i < argc; i++) {
			rv = remove_pkg(&xh, argv[i], recursive_rm);
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
		rv = exec_transaction(&xh, yes, dry_run, false);

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about an installed binary package. */
		if (argc != 2)
			usage(true);

		rv = show_pkg_info_from_metadir(&xh, argv[1], option);
		if (rv != 0) {
			printf("Package %s not installed.\n", argv[1]);
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-files") == 0) {
		/* Shows files installed by a binary package. */
		if (argc != 2)
			usage(true);

		rv = show_pkg_files_from_metadir(&xh, argv[1]);
		if (rv != 0) {
			printf("Package %s not installed.\n", argv[1]);
			goto out;
		}

	} else if (strcasecmp(argv[0], "check") == 0) {
		/* Checks the integrity of an installed package. */
		if (argc != 2)
			usage(true);

		if (strcasecmp(argv[1], "all") == 0)
			rv = check_pkg_integrity_all(&xh);
		else
			rv = check_pkg_integrity(&xh, NULL, argv[1], true, NULL);

	} else if ((strcasecmp(argv[0], "dist-upgrade") == 0) ||
		   (strcasecmp(argv[0], "autoupdate") == 0)) {
		/*
		 * To update all packages currently installed.
		 */
		if (argc != 1)
			usage(true);

		if (rsync && ((rv = xbps_rpool_sync(&xh, NULL)) != 0))
			goto out;

		rv = dist_upgrade(&xh, yes, dry_run, show_download_pkglist_url);

	} else if (strcasecmp(argv[0], "show-orphans") == 0) {
		/*
		 * Only show the package name of all currently package
		 * orphans.
		 */
		if (argc != 1)
			usage(true);

		rv = show_orphans(&xh);

	} else if ((strcasecmp(argv[0], "remove-orphans") == 0) ||
		   (strcasecmp(argv[0], "autoremove") == 0)) {
		/*
		 * Removes orphan pkgs. These packages were installed
		 * as dependency and any installed package does not depend
		 * on it currently.
		 */
		if (argc != 1)
			usage(true);

		rv = remove_pkg_orphans(&xh, yes, dry_run);

	} else if (strcasecmp(argv[0], "reconfigure") == 0) {
		/*
		 * Reconfigure a package.
		 */
		if (argc != 2)
			usage(true);

		if (strcasecmp(argv[1], "all") == 0)
			rv = xbps_configure_packages(&xh, true);
		else
			rv = xbps_configure_pkg(&xh, argv[1], true, false, true);

	} else if (strcasecmp(argv[0], "show-deps") == 0) {
		/*
		 * Show dependencies for a package.
		 */
		if (argc != 2)
			usage(true);

		rv = show_pkg_deps(&xh, argv[1]);

	} else if (strcasecmp(argv[0], "list-manual") == 0) {
		/*
		 * List packages that were installed manually, not as
		 * dependencies.
		 */
		if (argc != 1)
			usage(true);

		rv = xbps_pkgdb_foreach_cb(&xh, list_manual_pkgs, NULL);

	} else if (strcasecmp(argv[0], "show-revdeps") == 0) {
		/*
		 * Show reverse dependencies for a package.
		 */
		if (argc != 2)
			usage(true);

		rv = show_pkg_reverse_deps(&xh, argv[1]);

	} else if (strcasecmp(argv[0], "find-files") == 0) {
		/*
		 * Find files matched by a pattern from installed
		 * packages.
		 */
		if (argc < 2)
			usage(true);

		rv = find_files_in_packages(&xh, argc, argv);

	} else {
		usage(true);
	}

out:
	xbps_end(&xh);
	exit(rv);
}
