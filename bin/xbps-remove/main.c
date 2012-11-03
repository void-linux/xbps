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
#include <getopt.h>
#include <dirent.h>
#include <syslog.h>

#include <xbps_api.h>
#include "../xbps-install/defs.h"

static struct xbps_handle xh;

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-remove [OPTIONS] [PKGNAME...]\n\n"
	    "OPTIONS\n"
	    " -C --config <file>       Full path to configuration file\n"
	    " -c --cachedir <dir>      Full path to cachedir\n"
	    " -d --debug               Debug mode shown to stderr\n"
	    " -f --force               Force package files removal\n"
	    " -h --help                Print help usage\n"
	    " -i --ignore-revdeps      Ignore reverse deps\n"
	    " -n --dry-run             Dry-run mode\n"
	    " -O --clean-cache         Remove obsolete packages in cachedir\n"
	    " -o --remove-orphans      Remove package orphans\n"
	    " -p --print-format <fmt>  Print format for dry-run mode\n"
	    " -R --recursive           Recursively remove dependencies\n"
	    " -r --rootdir <dir>       Full path to rootdir\n"
	    " -v --verbose             Verbose messages\n"
	    " -y --yes                 Assume yes to all questions\n"
	    " -V --version             Show XBPS version\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void __attribute__((noreturn))
cleanup_sighandler(int signum)
{
	xbps_end(&xh);
	_exit(signum);
}

static void
state_cb_rm(struct xbps_handle *xhp,
	    struct xbps_state_cb_data *xscd,
	    void *cbdata)
{
	bool syslog_enabled = false;

	(void)cbdata;

	if (xhp->flags & XBPS_FLAG_SYSLOG) {
		syslog_enabled = true;
		openlog("xbps-remove", LOG_CONS, LOG_USER);
	}

	switch (xscd->state) {
	/* notifications */
	case XBPS_STATE_REMOVE:
		printf("Removing `%s-%s' ...\n", xscd->pkgname, xscd->version);
		break;
	/* success */
	case XBPS_STATE_REMOVE_FILE:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE:
		if (xhp->flags & XBPS_FLAG_VERBOSE)
			printf("%s\n", xscd->desc);
		else {
			printf("%s\n", xscd->desc);
			printf("\033[1A\033[K");
		}
		break;
	case XBPS_STATE_REMOVE_DONE:
		printf("Removed `%s-%s' successfully.\n",
		    xscd->pkgname, xscd->version);
		if (syslog_enabled)
			syslog(LOG_NOTICE, "Removed `%s-%s' successfully "
			    "(rootdir: %s).", xscd->pkgname, xscd->version,
			    xhp->rootdir);
		break;
	/* errors */
	case XBPS_STATE_UNREGISTER_FAIL:
	case XBPS_STATE_REMOVE_FAIL:
		xbps_error_printf("%s\n", xscd->desc);
		if (syslog_enabled)
			syslog(LOG_ERR, "%s", xscd->desc);
		break;
	case XBPS_STATE_REMOVE_FILE_FAIL:
	case XBPS_STATE_REMOVE_FILE_HASH_FAIL:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL:
		/* Ignore errors due to not empty directories */
		if (xscd->err == ENOTEMPTY)
			return;

		xbps_error_printf("%s\n", xscd->desc);
		if (syslog_enabled)
			syslog(LOG_ERR, "%s", xscd->desc);
		break;
	default:
		xbps_dbg_printf(xhp,
		    "unknown state %d\n", xscd->state);
		break;
	}
}

static int
cachedir_clean(struct xbps_handle *xhp)
{
	prop_dictionary_t pkg_propsd, repo_pkgd;
	DIR *dirp;
	struct dirent *dp;
	const char *pkgver, *rsha256;
	char *binpkg, *ext;
	int rv = 0;

	if ((dirp = opendir(xhp->cachedir)) == NULL)
		return 0;

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		/* only process xbps binary packages, ignore something else */
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".xbps")) {
			printf("ignoring unknown file: %s\n", dp->d_name);
			continue;
		}
		/* Internalize props.plist dictionary from binary pkg */
		binpkg = xbps_xasprintf("%s/%s", xhp->cachedir, dp->d_name);
		assert(binpkg != NULL);
		pkg_propsd = xbps_dictionary_metadata_plist_by_url(binpkg,
		    "./props.plist");
		if (pkg_propsd == NULL) {
			xbps_error_printf("Failed to read from %s: %s\n",
			    dp->d_name, strerror(errno));
			free(binpkg);
			rv = errno;
			break;
		}
		prop_dictionary_get_cstring_nocopy(pkg_propsd, "pkgver", &pkgver);
		/*
		 * Remove binary pkg if it's not registered in any repository
		 * or if hash doesn't match.
		 */
		repo_pkgd = xbps_rpool_find_pkg_exact(xhp, pkgver);
		if (repo_pkgd) {
			prop_dictionary_get_cstring_nocopy(repo_pkgd,
			    "filename-sha256", &rsha256);
			if (xbps_file_hash_check(binpkg, rsha256) == ERANGE) {
				printf("Removed %s from cachedir (sha256 mismatch)\n",
				    dp->d_name);
				if (unlink(binpkg) == -1)
					fprintf(stderr, "Failed to remove "
					    "`%s': %s\n", binpkg,
					    strerror(errno));
			}
			free(binpkg);
			continue;
		}
		printf("Removed %s from cachedir (obsolete)\n", dp->d_name);
		if (unlink(binpkg) == -1)
			fprintf(stderr, "Failed to remove `%s': %s\n",
			    binpkg, strerror(errno));
		free(binpkg);
	}
	closedir(dirp);
	return rv;
}

static int
remove_pkg(struct xbps_handle *xhp, const char *pkgname, size_t cols,
	   bool recursive)
{
	prop_dictionary_t pkgd;
	prop_array_t reqby;
	const char *pkgver;
	size_t x;
	int rv;

	rv = xbps_transaction_remove_pkg(xhp, pkgname, recursive);
	if (rv == EEXIST) {
		/* pkg has revdeps */
		pkgd = xbps_find_pkg_dict_installed(xhp, pkgname, false);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		reqby = prop_dictionary_get(pkgd, "requiredby");
		printf("WARNING: %s IS REQUIRED BY %u PACKAGE%s:\n\n",
		    pkgver, prop_array_count(reqby),
		    prop_array_count(reqby) > 1 ? "S" : "");
		for (x = 0; x < prop_array_count(reqby); x++) {
			prop_array_get_cstring_nocopy(reqby, x, &pkgver);
			print_package_line(pkgver, cols, false);
		}
		printf("\n\n");
		print_package_line(NULL, cols, true);
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
	const char *shortopts = "C:c:dfhinOop:Rr:vVy";
	struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "force", no_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "ignore-revdeps", no_argument, NULL, 'i' },
		{ "dry-run", no_argument, NULL, 'n' },
		{ "clean-cache", no_argument, NULL, 'O' },
		{ "remove-orphans", no_argument, NULL, 'o' },
		{ "print-format", required_argument, NULL, 'p' },
		{ "recursive", no_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ "yes", no_argument, NULL, 'y' },
		{ NULL, 0, NULL, 0 }
	};
	struct sigaction sa;
	const char *rootdir, *cachedir, *conffile, *pformat;
	int i, c, flags, rv;
	bool yes, drun, recursive, ignore_revdeps, clean_cache;
	bool orphans, reqby_force;
	size_t maxcols;

	rootdir = cachedir = conffile = pformat = NULL;
	flags = rv = 0;
	drun = recursive = ignore_revdeps = clean_cache = false;
	reqby_force = yes = orphans = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
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
		case 'f':
			flags |= XBPS_FLAG_FORCE_REMOVE_FILES;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'i':
			ignore_revdeps = true;
			break;
		case 'n':
			drun = true;
			break;
		case 'O':
			clean_cache = true;
			break;
		case 'o':
			orphans = true;
			break;
		case 'p':
			pformat = optarg;
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
	xh.rootdir = rootdir;
	xh.cachedir = cachedir;
	xh.conffile = conffile;
	xh.flags = flags;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	/*
	 * Register a signal handler to clean up resources used by libxbps.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = cleanup_sighandler;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	maxcols = get_maxcols();
	/*
	 * Check that we have write permission on rootdir, metadir
	 * and cachedir.
	 */
	if ((!drun && ((access(xh.rootdir, W_OK) == -1) ||
	    (access(xh.metadir, W_OK) == -1) ||
	    (access(xh.cachedir, W_OK) == -1)))) {
		if (errno != ENOENT) {
			fprintf(stderr, "Not enough permissions on "
			    "rootdir/cachedir/metadir: %s\n",
			    strerror(errno));
			rv = errno;
			goto out;
		}
	}

	if (clean_cache) {
		rv = cachedir_clean(&xh);
		if (rv != 0)
			goto out;
	}

	if (orphans) {
		if ((rv = xbps_transaction_autoremove_pkgs(&xh)) != 0) {
			if (rv != ENOENT) {
				fprintf(stderr, "Failed to remove package "
				    "orphans: %s\n", strerror(rv));
				goto out;
			}
		}
	}

	for (i = optind; i < argc; i++) {
		rv = remove_pkg(&xh, argv[i], maxcols, recursive);
		if (rv == 0)
			continue;
		else if (rv != EEXIST)
			goto out;
		else
			reqby_force = true;
	}
	if (reqby_force && !ignore_revdeps) {
		rv = EINVAL;
		goto out;
	}

	if (orphans || argc)
		rv = exec_transaction(&xh, maxcols, yes, drun);

out:
	xbps_end(&xh);
	exit(rv);
}
