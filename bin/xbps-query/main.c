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

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xbps.h>

#include "defs.h"
#include "xbps.h"
#include "xbps/json.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-query [OPTIONS] MODE [ARGUMENTS]\n"
	    "\nOPTIONS\n"
	    " -C, --config <dir>        Path to confdir (xbps.d)\n"
	    " -c, --cachedir <dir>      Path to cachedir\n"
	    " -d, --debug               Debug mode shown to stderr\n"
	    " -F, --format <format>     Format for list output\n"
	    " -h, --help                Show usage\n"
	    " -i, --ignore-conf-repos   Ignore repositories defined in xbps.d\n"
	    " -J, --json                Print output as json\n"
	    " -M, --memory-sync         Remote repository data is fetched and stored\n"
	    "                           in memory, ignoring on-disk repodata archives\n"
	    " -p, --property PROP[,...] Show properties for PKGNAME\n"
	    " -R, --repository          Enable repository mode. This mode explicitly\n"
	    "                           looks for packages in repositories\n"
	    "     --repository=<url>    Enable repository mode and add repository\n"
	    "                           to the top of the list. This option can be\n"
	    "                           specified multiple times\n"
	    "     --regex               Use Extended Regular Expressions to match\n"
	    "     --fulldeptree         Full dependency tree for -x/--deps\n"
	    " -r, --rootdir <dir>       Full path to rootdir\n"
	    " -V, --version             Show XBPS version\n"
	    " -v, --verbose             Verbose messages\n"
	    "\nMODE\n"
	    " -l, --list-pkgs           List installed packages\n"
	    " -L, --list-repos          List registered repositories\n"
	    " -H, --list-hold-pkgs      List packages on hold state\n"
	    "     --list-repolock-pkgs  List repolocked packages\n"
	    " -m, --list-manual-pkgs    List packages installed explicitly\n"
	    " -O, --list-orphans        List package orphans\n"
	    " -o, --ownedby FILE        Search for package files by matching STRING or REGEX\n"
	    " -S, --show PKG            Show information for PKG [default mode]\n"
	    " -s, --search PKG          Search for packages by matching PKG, STRING or REGEX\n"
	    "     --cat=FILE PKG        Print FILE from PKG binpkg to stdout\n"
	    " -f, --files PKG           Show package files for PKG\n"
	    " -x, --deps PKG            Show dependencies for PKG\n"
	    " -X, --revdeps PKG         Show reverse dependencies for PKG\n");

	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
show_repo(struct xbps_repo *repo)
{
	xbps_data_t pubkey;
	const char *signedby = NULL;
	char *hexfp = NULL;
	uint16_t pubkeysize = 0;

	printf("%5zd %s",
	    repo->idx ? (ssize_t)xbps_dictionary_count(repo->idx) : -1,
	    repo->uri);
	printf(" (RSA %s)\n", repo->is_signed ? "signed" : "unsigned");
	if (!(repo->xhp->flags & XBPS_FLAG_VERBOSE))
		return;

	xbps_dictionary_get_cstring_nocopy(repo->idxmeta, "signature-by", &signedby);
	xbps_dictionary_get_uint16(repo->idxmeta, "public-key-size", &pubkeysize);
	pubkey = xbps_dictionary_get(repo->idxmeta, "public-key");
	if (pubkey)
		hexfp = xbps_pubkey2fp(pubkey);
	if (signedby)
		printf("      Signed-by: %s\n", signedby);
	if (pubkeysize && hexfp)
		printf("      %u %s\n", pubkeysize, hexfp);
	free(hexfp);
}

static int
show_repos(struct xbps_handle *xhp)
{
	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		const char *repouri = NULL;
		struct xbps_repo *repo;
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		repo = xbps_repo_open(xhp, repouri);
		if (!repo) {
			printf("%5zd %s (RSA maybe-signed)\n", (ssize_t)-1, repouri);
			continue;
		}
		show_repo(repo);
		xbps_repo_release(repo);
	}
	return 0;
}

static int
filter_hold(xbps_object_t obj)
{
	return xbps_dictionary_get(obj, "hold") != NULL;
}

static int
filter_manual(xbps_object_t obj)
{
	bool automatic = false;
	xbps_dictionary_get_bool(obj, "automatic-install", &automatic);
	return !automatic;
}

static int
filter_repolock(xbps_object_t obj)
{
	return xbps_dictionary_get(obj, "repolock") != NULL;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "C:c:dF:f:hHiJLlMmOo:p:Rr:s:S:VvX:x:";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "ignore-conf-repos", no_argument, NULL, 'i' },
		{ "json", no_argument, NULL, 'J' },
		{ "list-repos", no_argument, NULL, 'L' },
		{ "list-pkgs", no_argument, NULL, 'l' },
		{ "list-hold-pkgs", no_argument, NULL, 'H' },
		{ "list-repolock-pkgs", no_argument, NULL, 3 },
		{ "memory-sync", no_argument, NULL, 'M' },
		{ "list-manual-pkgs", no_argument, NULL, 'm' },
		{ "list-orphans", no_argument, NULL, 'O' },
		{ "ownedby", required_argument, NULL, 'o' },
		{ "property", required_argument, NULL, 'p' },
		{ "repository", optional_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "show", required_argument, NULL, 'S' },
		{ "search", required_argument, NULL, 's' },
		{ "version", no_argument, NULL, 'V' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "files", required_argument, NULL, 'f' },
		{ "format", required_argument, NULL, 'F' },
		{ "deps", required_argument, NULL, 'x' },
		{ "revdeps", required_argument, NULL, 'X' },
		{ "regex", no_argument, NULL, 0 },
		{ "fulldeptree", no_argument, NULL, 1 },
		{ "cat", required_argument, NULL, 2 },
		{ NULL, 0, NULL, 0 },
	};
	struct xbps_handle xh = {0};
	const char *pkg = NULL, *props = NULL, *catfile, *format;
	int c, rv;
	bool regex = false, repo_mode = false, fulldeptree = false;
	int json = 0;
	enum {
		CAT_FILE = 1,
		LIST_HOLD,
		LIST_INSTALLED,
		LIST_MANUAL,
		LIST_ORPHANS,
		LIST_REPOLOCK,
		SHOW_REPOS,
		SEARCH_FILE,
		SEARCH_PKG,
		SHOW_DEPS,
		SHOW_FILES,
		SHOW_PKG,
		SHOW_REVDEPS,
	} mode = 0;

	props = pkg = catfile = format = NULL;
	rv = c = 0;
	repo_mode = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'C':
			xbps_strlcpy(xh.confdir, optarg, sizeof(xh.confdir));
			break;
		case 'c':
			xbps_strlcpy(xh.cachedir, optarg, sizeof(xh.cachedir));
			break;
		case 'd':
			xh.flags |= XBPS_FLAG_DEBUG;
			break;
		case 'f':
			pkg = optarg;
			mode = SHOW_FILES;
			break;
		case 'F':
			format = optarg;
			break;
		case 'J':
			json++;
			break;
		case 'H':
			mode = LIST_HOLD;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'i':
			xh.flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
			break;
		case 'L':
			mode = SHOW_REPOS;
			break;
		case 'l':
			mode = LIST_INSTALLED;
			break;
		case 'M':
			xh.flags |= XBPS_FLAG_REPOS_MEMSYNC;
			break;
		case 'm':
			mode = LIST_MANUAL;
			break;
		case 'O':
			mode = LIST_ORPHANS;
			break;
		case 'o':
			pkg = optarg;
			mode = SEARCH_FILE;
			break;
		case 'p':
			props = optarg;
			break;
		case 'R':
			if (optarg != NULL) {
				xbps_repo_store(&xh, optarg);
			}
			repo_mode = true;
			break;
		case 'r':
			xbps_strlcpy(xh.rootdir, optarg, sizeof(xh.rootdir));
			break;
		case 'S':
			pkg = optarg;
			mode = SHOW_PKG;
			break;
		case 's':
			pkg = optarg;
			mode = SEARCH_PKG;
			break;
		case 'v':
			xh.flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'x':
			pkg = optarg;
			mode = SHOW_DEPS;
			break;
		case 'X':
			pkg = optarg;
			mode = SHOW_REVDEPS;
			break;
		case 0:
			regex = true;
			break;
		case 1:
			fulldeptree = true;
			break;
		case 2:
			mode = CAT_FILE;
			catfile = optarg;
			break;
		case 3:
			mode = LIST_REPOLOCK;
			break;
		case '?':
		default:
			usage(true);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	/* no mode (defaults to show) and cat mode take a trailing argv */
	if (mode == 0 || mode == CAT_FILE) {
		if (argc == 0)
			usage(true);
		if (mode == 0)
			mode = SHOW_PKG;
		pkg = *(argv++);
		argc--;
	}

	/* trailing parameters */
	if (argc != 0)
		usage(true);

	/*
	 * Initialize libxbps.
	 */
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	switch (mode) {
        case LIST_HOLD:
		rv = list_pkgdb(&xh, filter_hold, format ? format : "{pkgver}\n", json) < 0;
		break;
        case LIST_INSTALLED:
		if (format || json > 0) {
			rv = list_pkgdb(&xh, NULL, format, json);
		} else {
			rv = list_pkgs_pkgdb(&xh);
		}
		break;
        case LIST_MANUAL:
		rv = list_pkgdb(&xh, filter_manual, format ? format : "{pkgver}\n", json) < 0;
		break;
        case LIST_ORPHANS:
		rv = list_orphans(&xh, format ? format : "{pkgver}\n") < 0;
		break;
        case LIST_REPOLOCK:
		rv = list_pkgdb(&xh, filter_repolock, format ? format : "{pkgver}\n", json) < 0;
		break;
        case SHOW_REPOS:
		rv = show_repos(&xh);
		break;
        case SEARCH_FILE:
		rv = ownedby(&xh, pkg, repo_mode, regex);
		break;
        case SEARCH_PKG:
		rv = search(&xh, repo_mode, pkg, props, regex);
		break;
        case SHOW_DEPS:
		rv = show_pkg_deps(&xh, pkg, repo_mode, fulldeptree);
		break;
        case SHOW_FILES:
		if (repo_mode)
			rv = repo_show_pkg_files(&xh, pkg);
		else
			rv = show_pkg_files_from_metadir(&xh, pkg);
		break;
	case CAT_FILE:
		if (repo_mode)
			rv = repo_cat_file(&xh, pkg, catfile);
		else
			rv = cat_file(&xh, pkg, catfile);
		break;
        case SHOW_PKG:
		if (repo_mode)
			rv = repo_show_pkg_info(&xh, pkg, props);
		else
			rv = show_pkg_info_from_metadir(&xh, pkg, props);
		break;
        case SHOW_REVDEPS:
		rv = show_pkg_revdeps(&xh, pkg, repo_mode);
                break;
        }

	xbps_end(&xh);
	exit(rv);
}
