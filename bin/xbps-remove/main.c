/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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
#include <getopt.h>
#include <syslog.h>

#include <xbps.h>
#include "../xbps-install/defs.h"
#include "defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-remove [OPTIONS] [PKGNAME...]\n\n"
	    "OPTIONS\n"
	    " -C --config <dir>        Path to confdir (xbps.d)\n"
	    " -c --cachedir <dir>      Path to cachedir\n"
	    " -d --debug               Debug mode shown to stderr\n"
	    " -F --force-revdeps       Force package removal even with revdeps or\n"
	    "                          unresolved shared libraries\n"
	    " -f --force               Force package files removal\n"
	    " -h --help                Print help usage\n"
	    " -n --dry-run             Dry-run mode\n"
	    " -O --clean-cache         Remove obsolete packages in cachedir\n"
	    " -o --remove-orphans      Remove package orphans\n"
	    " -R --recursive           Recursively remove dependencies\n"
	    " -r --rootdir <dir>       Full path to rootdir\n"
	    " -v --verbose             Verbose messages\n"
	    " -y --yes                 Assume yes to all questions\n"
	    " -V --version             Show XBPS version\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
state_cb_rm(const struct xbps_state_cb_data *xscd, void *cbdata UNUSED)
{
	bool slog = false;

	if ((xscd->xhp->flags & XBPS_FLAG_DISABLE_SYSLOG) == 0) {
		slog = true;
		openlog("xbps-remove", 0, LOG_USER);
	}

	switch (xscd->state) {
	/* notifications */
	case XBPS_STATE_REMOVE:
		printf("Removing `%s' ...\n", xscd->arg);
		break;
	/* success */
	case XBPS_STATE_REMOVE_FILE:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE:
		if (xscd->xhp->flags & XBPS_FLAG_VERBOSE)
			printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_REMOVE_DONE:
		printf("Removed `%s' successfully.\n", xscd->arg);
		if (slog) {
			syslog(LOG_NOTICE, "Removed `%s' successfully "
			    "(rootdir: %s).", xscd->arg,
			    xscd->xhp->rootdir);
		}
		break;
	case XBPS_STATE_SHOW_REMOVE_MSG:
                printf("%s: pre-remove message:\n", xscd->arg);
		printf("========================================================================\n");
		printf("%s", xscd->desc);
		printf("========================================================================\n");
		break;
	/* errors */
	case XBPS_STATE_REMOVE_FAIL:
		xbps_error_printf("%s\n", xscd->desc);
		if (slog) {
			syslog(LOG_ERR, "%s", xscd->desc);
		}
		break;
	case XBPS_STATE_REMOVE_FILE_FAIL:
	case XBPS_STATE_REMOVE_FILE_HASH_FAIL:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL:
		/* Ignore errors due to not empty directories */
		if (xscd->err == ENOTEMPTY)
			return 0;

		xbps_error_printf("%s\n", xscd->desc);
		if (slog) {
			syslog(LOG_ERR, "%s", xscd->desc);
		}
		break;
	case XBPS_STATE_ALTGROUP_ADDED:
	case XBPS_STATE_ALTGROUP_REMOVED:
	case XBPS_STATE_ALTGROUP_SWITCHED:
	case XBPS_STATE_ALTGROUP_LINK_ADDED:
	case XBPS_STATE_ALTGROUP_LINK_REMOVED:
		if (xscd->desc) {
			printf("%s\n", xscd->desc);
			if (slog)
				syslog(LOG_NOTICE, "%s", xscd->desc);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int
remove_pkg(struct xbps_handle *xhp, const char *pkgname, bool recursive)
{
	int rv;

	rv = xbps_transaction_remove_pkg(xhp, pkgname, recursive);
	if (rv == EEXIST) {
		return rv;
	} else if (rv == ENOENT) {
		printf("Package `%s' is not currently installed.\n", pkgname);
		return 0;
	} else if (rv != 0) {
		xbps_error_printf("Failed to queue `%s' for removing: %s\n",
		    pkgname, strerror(rv));
		return rv;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "C:c:dFfhnOoRr:vVy";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "force-revdeps", no_argument, NULL, 'F' },
		{ "force", no_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "dry-run", no_argument, NULL, 'n' },
		{ "clean-cache", no_argument, NULL, 'O' },
		{ "remove-orphans", no_argument, NULL, 'o' },
		{ "recursive", no_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ "yes", no_argument, NULL, 'y' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	const char *rootdir, *cachedir, *confdir;
	int c, flags, rv;
	bool yes, drun, recursive, clean_cache, orphans;
	int maxcols;

	rootdir = cachedir = confdir = NULL;
	flags = rv = 0;
	drun = recursive = clean_cache = yes = orphans = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'C':
			confdir = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'F':
			flags |= XBPS_FLAG_FORCE_REMOVE_REVDEPS;
			break;
		case 'f':
			flags |= XBPS_FLAG_FORCE_REMOVE_FILES;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'n':
			drun = true;
			break;
		case 'O':
			clean_cache = true;
			break;
		case 'o':
			orphans = true;
			break;
		case 'R':
			recursive = true;
			break;
		case 'r':
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
			usage(true);
			/* NOTREACHED */
		}
	}
	if (!clean_cache && !orphans && (argc == optind))
		usage(true);

	/*
	 * Initialize libxbps.
	 */
	memset(&xh, 0, sizeof(xh));
	xh.state_cb = state_cb_rm;
	if (rootdir)
		xbps_strlcpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	if (cachedir)
		xbps_strlcpy(xh.cachedir, cachedir, sizeof(xh.cachedir));
	if (confdir)
		xbps_strlcpy(xh.confdir, confdir, sizeof(xh.confdir));

	xh.flags = flags;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	maxcols = get_maxcols();

	if (clean_cache) {
		rv = clean_cachedir(&xh, drun);
		if (!orphans || rv)
			exit(rv);;
	}

	if (!drun && (rv = xbps_pkgdb_lock(&xh)) != 0) {
		fprintf(stderr, "failed to lock pkgdb: %s\n", strerror(rv));
		exit(rv);
	}

	if (orphans) {
		if ((rv = xbps_transaction_autoremove_pkgs(&xh)) != 0) {
			xbps_end(&xh);
			if (rv != ENOENT) {
				fprintf(stderr, "Failed to queue package "
				    "orphans: %s\n", strerror(rv));
				exit(EXIT_FAILURE);
			}
			exit(EXIT_SUCCESS);
		}
	}

	for (int i = optind; i < argc; i++) {
		rv = remove_pkg(&xh, argv[i], recursive);
		if (rv != 0) {
			xbps_end(&xh);
			exit(rv);
		}
	}
	if (orphans || (argc > optind)) {
		rv = exec_transaction(&xh, maxcols, yes, drun);
	}
	xbps_end(&xh);
	exit(rv);
}
