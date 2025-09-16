/*-
 * Copyright (c) 2008-2019 Juan Romero Pardines.
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

#include <xbps.h>
#include "defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-install [OPTIONS] [PKGNAME...]\n\n"
	    "OPTIONS\n"
	    " -A, --automatic             Set automatic installation mode\n"
	    " -C, --config <dir>          Path to confdir (xbps.d)\n"
	    " -c, --cachedir <dir>        Path to cachedir\n"
	    " -d, --debug                 Debug mode shown to stderr\n"
	    " -D, --download-only         Download packages and check integrity, nothing else\n"
	    " -f, --force                 Force package re-installation\n"
	    "                             If specified twice, all files will be overwritten.\n"
	    " -h, --help                  Show usage\n"
	    " -i, --ignore-conf-repos     Ignore repositories defined in xbps.d\n"
	    " -I, --ignore-file-conflicts Ignore detected file conflicts\n"
	    " -U, --unpack-only           Unpack packages in transaction, do not configure them\n"
	    " -M, --memory-sync           Remote repository data is fetched and stored\n"
	    "                             in memory, ignoring on-disk repodata archives\n"
	    " -n, --dry-run               Dry-run mode\n"
	    " -R, --repository <url>      Add repository to the top of the list\n"
	    "                             This option can be specified multiple times\n"
	    " -r, --rootdir <dir>         Full path to rootdir\n"
	    "     --reproducible          Enable reproducible mode in pkgdb\n"
	    "     --staging               Enable use of staged packages\n"
	    " -S, --sync                  Sync remote repository index\n"
	    " -u, --update                Update target package(s)\n"
	    " -v, --verbose               Verbose messages\n"
	    " -y, --yes                   Assume yes to all questions\n"
	    " -V, --version               Show XBPS version\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
unpack_progress_cb(const struct xbps_unpack_cb_data *xpd, void *cbdata UNUSED)
{
	if (xpd->entry == NULL || xpd->entry_total_count <= 0)
		return;

	printf("%s: unpacked %sfile `%s' (%" PRIi64 " bytes)\n",
	    xpd->pkgver,
	    xpd->entry_is_conf ? "configuration " : "", xpd->entry,
	    xpd->entry_size);
}

static int
repo_import_key_cb(struct xbps_repo *repo, void *arg UNUSED, bool *done UNUSED)
{
	int rv;

	if ((rv = xbps_repo_key_import(repo)) != 0)
		xbps_error_printf("Failed to import pubkey from %s: %s\n",
		    repo->uri, strerror(rv));

	return rv;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "AC:c:DdfhIiMnR:r:SuUVvy";
	const struct option longopts[] = {
		{ "automatic", no_argument, NULL, 'A' },
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "download-only", no_argument, NULL, 'D' },
		{ "force", no_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "ignore-conf-repos", no_argument, NULL, 'i' },
		{ "ignore-file-conflicts", no_argument, NULL, 'I' },
		{ "memory-sync", no_argument, NULL, 'M' },
		{ "dry-run", no_argument, NULL, 'n' },
		{ "repository", required_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "sync", no_argument, NULL, 'S' },
		{ "unpack-only", no_argument, NULL, 'U' },
		{ "update", no_argument, NULL, 'u' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ "yes", no_argument, NULL, 'y' },
		{ "reproducible", no_argument, NULL, 1 },
		{ "staging", no_argument, NULL, 2 },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	struct xferstat xfer;
	const char *rootdir, *cachedir, *confdir;
	int i, c, flags, rv, fflag = 0;
	bool syncf, yes, force, drun, update;
	int maxcols, eexist = 0;

	rootdir = cachedir = confdir = NULL;
	flags = rv = 0;
	syncf = yes = force = drun = update = false;

	memset(&xh, 0, sizeof(xh));

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 1:
			flags |= XBPS_FLAG_INSTALL_REPRO;
			break;
		case 2:
			flags |= XBPS_FLAG_USE_STAGE;
			break;
		case 'A':
			flags |= XBPS_FLAG_INSTALL_AUTO;
			break;
		case 'C':
			confdir = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'D':
			flags |= XBPS_FLAG_DOWNLOAD_ONLY;
			break;
		case 'f':
			fflag++;
			if (fflag > 1)
				flags |= XBPS_FLAG_FORCE_UNPACK;
			force = true;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'I':
			flags |= XBPS_FLAG_IGNORE_FILE_CONFLICTS;
			break;
		case 'i':
			flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
			break;
		case 'M':
			flags |= XBPS_FLAG_REPOS_MEMSYNC;
			break;
		case 'n':
			drun = true;
			break;
		case 'R':
			xbps_repo_store(&xh, optarg);
			break;
		case 'r':
			rootdir = optarg;
			break;
		case 'S':
			syncf = true;
			break;
		case 'U':
			flags |= XBPS_FLAG_UNPACK_ONLY;
			break;
		case 'u':
			update = true;
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
	if ((!update && !syncf) && (argc == optind))
		usage(true);

	/*
	 * Initialize libxbps.
	 */
	xh.state_cb = state_cb;
	xh.fetch_cb = fetch_file_progress_cb;
	xh.fetch_cb_data = &xfer;
	if (rootdir)
		xbps_strlcpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	if (cachedir)
		xbps_strlcpy(xh.cachedir, cachedir, sizeof(xh.cachedir));
	if (confdir)
		xbps_strlcpy(xh.confdir, confdir, sizeof(xh.confdir));
	xh.flags = flags;
	if (flags & XBPS_FLAG_VERBOSE)
		xh.unpack_cb = unpack_progress_cb;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	maxcols = get_maxcols();

	/* Sync remote repository data and import keys from remote repos */
	if (syncf && !drun) {
		if ((rv = xbps_rpool_sync(&xh, NULL)) != 0) {
			xbps_end(&xh);
			exit(EXIT_FAILURE);
		}
		rv = xbps_rpool_foreach(&xh, repo_import_key_cb, NULL);
		if (rv != 0) {
			xbps_end(&xh);
			exit(EXIT_FAILURE);
		}
	}

	if (syncf && !update && (argc == optind))
		exit(EXIT_SUCCESS);

	if (!(xh.flags & XBPS_FLAG_DOWNLOAD_ONLY) && !drun) {
		if (xbps_pkgdb_lock(&xh) < 0) {
			xbps_end(&xh);
			exit(EXIT_FAILURE);
		}
	}

	eexist = optind;

	if (update && (argc == optind)) {
		/* Update all installed packages */
		rv = dist_upgrade(&xh, maxcols, yes, drun);
	} else if (update) {
		/* Update target packages */
		for (i = optind; i < argc; i++) {
			rv = update_pkg(&xh, argv[i], force);
			if (rv == EEXIST) {
				/* pkg already updated, ignore */
				rv = 0;
				eexist++;
			} else if (rv != 0) {
				xbps_end(&xh);
				exit(rv);
			}
		}
		if (eexist == argc)
			goto out;

		rv = exec_transaction(&xh, maxcols, yes, drun);
	} else if (!update) {
		/* Install target packages */
		for (i = optind; i < argc; i++) {
			rv = install_new_pkg(&xh, argv[i], force);
			if (rv == EEXIST) {
				/* pkg already installed, ignore */
				rv = 0;
				eexist++;
			} else if (rv != 0) {
				xbps_end(&xh);
				exit(rv);
			}
		}
		if (eexist == argc)
			goto out;

		rv = exec_transaction(&xh, maxcols, yes, drun);
	}

out:
	xbps_end(&xh);
	exit(rv);
}
