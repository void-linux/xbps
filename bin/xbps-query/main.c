/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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
	    " -C --config <file>       Full path to configuration file\n"
	    " -c --cachedir <dir>      Full path to cachedir\n"
	    " -d --debug               Debug mode shown to stderr\n"
	    " -h --help                Print help usage\n"
	    " -p --property PROP[,...] Show properties for PKGNAME\n"
	    " -R --repository          Enable repository mode. This mode explicitly\n"
	    "                          looks for packages in repositories.\n"
	    "    --repository=<url>    Enable repository mode and add repository\n"
	    "                          to the top of the list. This option can be\n"
	    "                          specified multiple times.\n"
	    "    --regex               Use Extended Regular Expressions to match\n"
	    " -r --rootdir <dir>       Full path to rootdir\n"
	    " -V --version             Show XBPS version\n"
	    " -v --verbose             Verbose messages\n"
	    "\nMODE\n"
	    " -l --list-pkgs           List installed packages\n"
	    " -L --list-repos          List registered repositories\n"
	    " -H --list-hold-pkgs      List packages on hold state\n"
	    " -m --list-manual-pkgs    List packages installed explicitly\n"
	    " -O --list-orphans        List package orphans\n"
	    " -o --ownedby FILE        Search for package files by matching STRING or REGEX\n"
	    " -S --show PKG            Show information for PKG [default mode]\n"
	    " -s --search PKG          Search for packages by matching PKG, STRING or REGEX\n"
	    " -f --files PKG           Show package files for PKG\n"
	    " -x --deps PKG            Show dependencies for PKG (set it twice for a full dependency tree)\n"
	    " -X --revdeps PKG         Show reverse dependencies for PKG\n");

	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	const char *shortopts = "C:c:df:hHLlmOo:p:Rr:s:S:VvX:x:";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "list-repos", no_argument, NULL, 'L' },
		{ "list-pkgs", no_argument, NULL, 'l' },
		{ "list-hold-pkgs", no_argument, NULL, 'H' },
		{ "list-manual-pkgs", no_argument, NULL, 'm' },
		{ "list-orphans", no_argument, NULL, 'O' },
		{ "ownedby", required_argument, NULL, 'o' },
		{ "property", required_argument, NULL, 'p' },
		{ "repository", optional_argument, NULL, 'R' },
		{ "regex", no_argument, NULL, 0 },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "show", required_argument, NULL, 'S' },
		{ "search", required_argument, NULL, 's' },
		{ "version", no_argument, NULL, 'V' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "files", required_argument, NULL, 'f' },
		{ "deps", required_argument, NULL, 'x' },
		{ "revdeps", required_argument, NULL, 'X' },
		{ NULL, 0, NULL, 0 },
	};
	struct xbps_handle xh;
	const char *pkg, *rootdir, *cachedir, *conffile, *props;
	int c, flags, rv, show_deps = 0;
	bool list_pkgs, list_repos, orphans, own;
	bool list_manual, list_hold, show_prop, show_files, show_rdeps;
	bool show, search, regex, repo_mode, opmode, fulldeptree;

	rootdir = cachedir = conffile = props = pkg = NULL;
	flags = rv = c = 0;
	list_pkgs = list_repos = list_hold = orphans = search = own = false;
	list_manual = show_prop = show_files = false;
	regex = show = show_rdeps = fulldeptree = false;
	repo_mode = opmode = false;

	memset(&xh, 0, sizeof(xh));

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
			pkg = optarg;
			show_files = opmode = true;
			break;
		case 'H':
			list_hold = opmode = true;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'L':
			list_repos = opmode = true;
			break;
		case 'l':
			list_pkgs = opmode = true;
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
				if (xh.repositories == NULL)
					xh.repositories = xbps_array_create();

				xbps_array_add_cstring_nocopy(xh.repositories, optarg);
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
			search = opmode = true;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'x':
			pkg = optarg;
			show_deps++;
			opmode = true;
			break;
		case 'X':
			pkg = optarg;
			show_rdeps = opmode = true;
			break;
		case 0:
			regex = true;
			break;
		case '?':
			usage(true);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == optind || (!argc && !opmode)) {
		usage(true);
	} else if (!opmode) {
		/* show mode by default */
		show = opmode = true;
		pkg = *argv;
	}
	/*
	 * Initialize libxbps.
	 */
	if (rootdir)
		strncpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	if (cachedir)
		strncpy(xh.cachedir, cachedir, sizeof(xh.cachedir));
	xh.conffile = conffile;
	xh.flags = flags;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (list_repos) {
		/* list repositories */
		rv = repo_list(&xh);

	} else if (list_hold) {
		/* list on hold pkgs */
		rv = xbps_pkgdb_foreach_cb(&xh, list_hold_pkgs, NULL);

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

	} else if (search) {
		/* search mode */
		rv = repo_search(&xh, pkg, props, regex);

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
		if (show_deps > 1)
			fulldeptree = true;

		if (repo_mode)
			rv = repo_show_pkg_deps(&xh, pkg, fulldeptree);
		else
			rv = show_pkg_deps(&xh, pkg, fulldeptree);

	} else if (show_rdeps) {
		/* show-rdeps mode */
		if (repo_mode)
			rv = repo_show_pkg_revdeps(&xh, pkg);
		else
			rv = show_pkg_revdeps(&xh, pkg);
	}

	exit(rv);
}
