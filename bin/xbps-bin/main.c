/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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
#include <errno.h>
#include <signal.h>

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-repo/defs.h"

static void
usage(void)
{
	fprintf(stderr,
	"Usage: xbps-bin [options] [target] [arguments]\n\n"
	" Available targets:\n"
	"    autoremove\n"
	"    autoupdate\n"
	"    check\t\t[<pkgname>|<all>]\n"
	"    install\t\t[<pkgname(s)>|<pkgpattern(s)>]\n"
	"    list\n"
	"    list-manual\n"
	"    purge\t\t[<pkgname>|<all>]\n"
	"    reconfigure\t\t[<pkgname>|<all>]\n"
	"    remove\t\t<pkgname(s)>\n"
	"    show\t\t<pkgname>\n"
	"    show-deps\t\t<pkgname>\n"
	"    show-files\t\t<pkgname>\n"
	"    show-revdeps\t<pkgname>\n"
	"    update\t\t<pkgname(s)>\n"
	" Options shared by all targets:\n"
	"    -c\t\t<cachedir>\n"
	"    -r\t\t<rootdir>\n"
	"    -v\t\tShows verbose messages\n"
	"    -V\t\tPrints the xbps release version\n"
	" Options used by the install/(auto)remove/update targets:\n"
	"    -y\t\tAssume \"yes\" for all questions.\n"
	"    -p\t\tAlso purge package(s) in the (auto)remove targets.\n"
	" Options used by the purge/reconfigure/remove targets:\n"
	"    -f\t\tForce reconfiguration or removal of files.\n"
	"\n");
	exit(EXIT_FAILURE);
}

static int
list_pkgs_in_dict(prop_object_t obj, void *arg, bool *loop_done)
{
	const char *pkgver, *short_desc;

	(void)arg;
	(void)loop_done;

	assert(prop_object_type(obj) == PROP_TYPE_DICTIONARY);

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "short_desc", &short_desc);
	if (pkgver && short_desc) {
		printf("%s\t%s\n", pkgver, short_desc);
		return 0;
	}

	return EINVAL;
}

static int
list_manual_packages(prop_object_t obj, void *arg, bool *loop_done)
{
	const char *pkgver;
	bool automatic = false;

	(void)arg;
	(void)loop_done;

	prop_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (automatic == false) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}


static void
cleanup(int signum)
{
	xbps_regpkgs_dictionary_release();
	exit(signum);
}

int
main(int argc, char **argv)
{
	prop_dictionary_t dict;
	struct sigaction sa;
	int i = 0, c, flags = 0, rv = 0;
	bool yes, verbose, purge;

	yes = verbose = purge = false;

	while ((c = getopt(argc, argv, "Vcfpr:vy")) != -1) {
		switch (c) {
		case 'c':
			xbps_set_cachedir(optarg);
			break;
		case 'f':
			flags |= XBPS_FLAG_FORCE;
			break;
		case 'p':
			purge = true;
			break;
		case 'r':
			/* To specify the root directory */
			xbps_set_rootdir(optarg);
			break;
		case 'v':
			verbose = true;
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
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if (flags != 0)
		xbps_set_flags(flags);

	/*
	 * Register a signal handler to clean up resources used by libxbps.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = cleanup;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	if ((dict = xbps_regpkgs_dictionary_init()) == NULL) {
		if (errno != ENOENT) {
			rv = errno;
			fprintf(stderr,
			    "E: couldn't initialize regpkgdb dict: %s\n",
			    strerror(errno));
			goto out;
		}
	}

	if (strcasecmp(argv[0], "list") == 0) {
		/* Lists packages currently registered in database. */
		if (argc != 1)
			usage();

		if (dict == NULL) {
			printf("No packages currently installed.\n");
			goto out;
		}

		if (!xbps_callback_array_iter_in_dict(dict, "packages",
		    list_pkgs_in_dict, NULL)) {
			rv = errno;
			goto out;
		}

	} else if (strcasecmp(argv[0], "install") == 0) {
		/* Installs a binary package and required deps. */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++)
			if ((rv = xbps_install_new_pkg(argv[i])) != 0)
				goto out;

		rv = xbps_exec_transaction(yes);

	} else if (strcasecmp(argv[0], "update") == 0) {
		/* Update an installed package. */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++)
			if ((rv = xbps_update_pkg(argv[i])) != 0)
				goto out;

		rv = xbps_exec_transaction(yes);

	} else if (strcasecmp(argv[0], "remove") == 0) {
		/* Removes a binary package. */
		if (argc < 2)
			usage();

		rv = xbps_remove_installed_pkgs(argc, argv, yes, purge);

	} else if (strcasecmp(argv[0], "show") == 0) {
		/* Shows info about an installed binary package. */
		if (argc != 2)
			usage();

		rv = show_pkg_info_from_metadir(argv[1]);
		if (rv != 0) {
			printf("Package %s not installed.\n", argv[1]);
			goto out;
		}

	} else if (strcasecmp(argv[0], "show-files") == 0) {
		/* Shows files installed by a binary package. */
		if (argc != 2)
			usage();

		rv = show_pkg_files_from_metadir(argv[1]);
		if (rv != 0) {
			printf("Package %s not installed.\n", argv[1]);
			goto out;
		}

	} else if (strcasecmp(argv[0], "check") == 0) {
		/* Checks the integrity of an installed package. */
		if (argc != 2)
			usage();

		if (strcasecmp(argv[1], "all") == 0)
			rv = xbps_check_pkg_integrity_all();
		else
			rv = xbps_check_pkg_integrity(argv[1]);

	} else if (strcasecmp(argv[0], "autoupdate") == 0) {
		/*
		 * To update all packages currently installed.
		 */
		if (argc != 1)
			usage();

		rv = xbps_autoupdate_pkgs(yes);

	} else if (strcasecmp(argv[0], "autoremove") == 0) {
		/*
		 * Removes orphan pkgs. These packages were installed
		 * as dependency and any installed package does not depend
		 * on it currently.
		 */
		if (argc != 1)
			usage();

		rv = xbps_autoremove_pkgs(yes, purge);

	} else if (strcasecmp(argv[0], "purge") == 0) {
		/*
		 * Purge a package completely.
		 */
		if (argc != 2)
			usage();

		if (strcasecmp(argv[1], "all") == 0)
			rv = xbps_purge_all_pkgs();
		else
			rv = xbps_purge_pkg(argv[1], true);

	} else if (strcasecmp(argv[0], "reconfigure") == 0) {
		/*
		 * Reconfigure a package.
		 */
		if (argc != 2)
			usage();

		if (strcasecmp(argv[1], "all") == 0)
			rv = xbps_configure_all_pkgs();
		else
			rv = xbps_configure_pkg(argv[1], NULL, true, false);

	} else if (strcasecmp(argv[0], "show-deps") == 0) {
		/*
		 * Show dependencies for a package.
		 */
		if (argc != 2)
			usage();

		rv = xbps_show_pkg_deps(argv[1]);

	} else if (strcasecmp(argv[0], "list-manual") == 0) {
		/*
		 * List packages that were installed manually, not as
		 * dependencies.
		 */
		if (argc != 1)
			usage();

		rv = xbps_callback_array_iter_in_dict(dict, "packages",
		    list_manual_packages, NULL);
		
	} else if (strcasecmp(argv[0], "show-revdeps") == 0) {
		/*
		 * Show reverse dependencies for a package.
		 */
		if (argc != 2)
			usage();

		rv = xbps_show_pkg_reverse_deps(argv[1]);

	} else {
		usage();
	}

out:
	cleanup(rv);
}
