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
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>

#include <xbps.h>
#include "defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-query [OPTIONS] MODE [ARGUMENTS]\n"
	    "\nOPTIONS\n"
	    " -C, --config <dir>        Path to confdir (xbps.d)\n"
	    " -c, --cachedir <dir>      Path to cachedir\n"
	    " -d, --debug               Debug mode shown to stderr\n"
	    " -h, --help                Show usage\n"
	    " -i, --ignore-conf-repos   Ignore repositories defined in xbps.d\n"
	    " -M, --memory-sync         Remote repository data is fetched and stored\n"
	    "                           in memory, ignoring on-disk repodata archives\n"
	    " -p, --property PROP[,...] Specify properties for --show or --search modes\n"
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

int
main(int argc, char **argv)
{
	const char *shortopts = "C:c:df:hHiLlMmOo:p:Rr:s:S:VvX:x:";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "ignore-conf-repos", no_argument, NULL, 'i' },
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
		{ "deps", required_argument, NULL, 'x' },
		{ "revdeps", required_argument, NULL, 'X' },
		{ "regex", no_argument, NULL, 0 },
		{ "fulldeptree", no_argument, NULL, 1 },
		{ "cat", required_argument, NULL, 2 },
		{ NULL, 0, NULL, 0 },
	};
	struct xbps_handle xh;
	const char *pkg, *rootdir, *cachedir, *confdir, *props, *catfile;
	int c, flags, rv, ops;
	bool list_pkgs, list_repos, orphans, own, list_repolock, list_manual;
	bool list_hold, show_prop, show_files, show_deps, show_rdeps, show;
	bool cat, pkg_search, regex, repo_mode, opmode, fulldeptree;
	bool not_compatible_with_repo_mode;
	enum search_mode search_mode;

	pkg = rootdir = cachedir = confdir = props = catfile = NULL;
	c = flags = rv = ops = 0;
	list_pkgs = list_repos = orphans = own = list_repolock = list_manual= false;
	list_hold = show_prop = show_files = show_deps = show_rdeps = show = false;
	cat = pkg_search = regex = repo_mode = opmode = fulldeptree = false;
	not_compatible_with_repo_mode = false;
	search_mode = IN_INSTALLED;

	memset(&xh, 0, sizeof(xh));

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
		case 'f':
			pkg = optarg;
			show_files = opmode = true;
			break;
		case 'H':
			list_hold = opmode = true;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'i':
			flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
			break;
		case 'L':
			list_repos = opmode = true;
			break;
		case 'l':
			list_pkgs = opmode = true;
			break;
		case 'M':
			flags |= XBPS_FLAG_REPOS_MEMSYNC;
			break;
		case 'm':
			list_manual = opmode = true;
			break;
		case 'O':
			orphans = opmode = true;
			break;
		case 'o':
			pkg = optarg;
			own = opmode = true;
			break;
		case 'p':
			props = optarg;
			show_prop = true;
			break;
		case 'R':
			if (optarg != NULL) {
				xbps_repo_store(&xh, optarg);
			}
			repo_mode = true;
			break;
		case 'r':
			rootdir = optarg;
			break;
		case 'S':
			pkg = optarg;
			show = opmode = true;
			break;
		case 's':
			pkg = optarg;
			pkg_search = opmode = true;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'x':
			pkg = optarg;
			show_deps = opmode = true;
			break;
		case 'X':
			pkg = optarg;
			show_rdeps = opmode = true;
			break;
		case 0:
			regex = true;
			break;
		case 1:
			fulldeptree = true;
			break;
		case 2:
			catfile = optarg;
			cat = opmode = true;
			break;
		case 3:
			list_repolock = opmode = true;
			break;
		case '?':
		default:
			usage(true);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (!argc && !opmode) {
		usage(true);
		/* NOTREACHED */
	} else if (!opmode) {
		/* show mode by default */
		show = opmode = true;
		pkg = *(argv++);
		argc--;
	}
	if (argc) {
		/* trailing parameters */
		usage(true);
		/* NOTREACHED */
	}
	/*
	 * Initialize libxbps.
	 */
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

	not_compatible_with_repo_mode = false
		|| list_repos
		|| list_hold
		|| list_repolock
		|| list_manual
		|| list_pkgs
		|| orphans;

	if (repo_mode && not_compatible_with_repo_mode) {
		xbps_error_printf("Repository mode (-R, --repository) conflicts with one of:\n"
			" -l, --list-pkgs\n"
			" -L, --list-repos\n"
			" -H, --list-hold-pkgs\n"
			"     --list-repolock-pkgs\n"
			" -m, --list-manual-pkgs\n"
			" -O, --list-orphans\n");
		exit(EXIT_FAILURE);
	}

	if (pkg_search) {
		if (list_manual) {
			list_manual = false;
			search_mode = IN_MANUAL;
		}

		/* 'props' are always passed to search */
		if (show_prop)
			show_prop = false;
	}

	ops = 0
		+ (int)list_repos
		+ (int)list_hold
		+ (int)list_repolock
		+ (int)list_manual
		+ (int)list_pkgs
		+ (int)orphans
		+ (int)own
		+ (int)pkg_search
		+ (int)cat
		+ (int)(show || show_prop)
		+ (int)show_files
		+ (int)show_deps
		+ (int)show_rdeps;

	if (ops != 1) {
		xbps_error_printf("Conflicting MODE flags, please consult the man page.\n");
		exit(EXIT_FAILURE);
	}

	if (list_repos) {
		/* list repositories */
		rv = repo_list(&xh);

	} else if (list_hold) {
		/* list on hold pkgs */
		rv = xbps_pkgdb_foreach_cb(&xh, list_hold_pkgs, NULL);

	} else if (list_repolock) {
		/* list repolocked packages */
		rv = xbps_pkgdb_foreach_cb(&xh, list_repolock_pkgs, NULL);

	} else if (list_manual) {
		/* list manual pkgs */
		rv = xbps_pkgdb_foreach_cb(&xh, list_manual_pkgs, NULL);

	} else if (list_pkgs) {
		/* list available pkgs */
		rv = list_pkgs_pkgdb(&xh);

	} else if (orphans) {
		/* list pkg orphans */
		rv = list_orphans(&xh);

	} else if (own) {
		/* ownedby mode */
		rv = ownedby(&xh, pkg, repo_mode, regex);

	} else if (pkg_search) {
		/* search mode */
		if (repo_mode)
			search_mode = IN_REPO;
		rv = search(&xh, regex, pkg, props, search_mode);

	} else if (catfile) {
		/* repo cat file mode */
		if (repo_mode)
			rv =  repo_cat_file(&xh, pkg, catfile);
		else
			rv =  cat_file(&xh, pkg, catfile);
	} else if (show || show_prop) {
		/* show mode */
		if (repo_mode)
			rv = repo_show_pkg_info(&xh, pkg, props);
		else
			rv = show_pkg_info_from_metadir(&xh, pkg, props);

	} else if (show_files) {
		/* show-files mode */
		if (repo_mode)
			rv =  repo_show_pkg_files(&xh, pkg);
		else
			rv = show_pkg_files_from_metadir(&xh, pkg);

	} else if (show_deps) {
		/* show-deps mode */
		rv = show_pkg_deps(&xh, pkg, repo_mode, fulldeptree);

	} else if (show_rdeps) {
		/* show-rdeps mode */
		rv = show_pkg_revdeps(&xh, pkg, repo_mode);
	}

	xbps_end(&xh);
	exit(rv);
}
