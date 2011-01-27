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
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

#include <xbps_api.h>
#include "../xbps-bin/defs.h"

/* error messages in bold/red */
#define MSG_ERROR	"\033[1m\033[31m"
/* warn messages in bold/yellow */
#define MSG_WARN	"\033[1m\033[33m"
/* normal messages in bold */
#define MSG_NORMAL	"\033[1m"
#define MSG_RESET	"\033[m"

static void
write_plist_file(prop_dictionary_t dict, const char *file)
{
	assert(dict != NULL || file != NULL);

	if (!prop_dictionary_externalize_to_zfile(dict, file)) {
		prop_object_release(dict);
		printf("=> ERROR: couldn't write to %s (%s)",
		    file, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr,
	"usage: xbps-uhelper [options] [action] [args]\n"
	"\n"
	"  Available actions:\n"
	"    cmpver, digest, fetch, getpkgdepname, getpkgname, getpkgrevision,\n"
	"    getpkgversion, pkgmatch, register, sanitize-plist, unregister,\n"
	"    version\n"
	"\n"
	"  Action arguments:\n"
	"    cmpver\t\t<instver> <reqver>\n"
	"    digest\t\t<file> <file1+N>\n"
	"    fetch\t\t<URL> <URL1+N>\n"
	"    getpkgdepname\t<string>\n"
	"    getpkgdepversion\t<string>\n"
	"    getpkgname\t\t<string>\n"
	"    getpkgrevision\t<string>\n"
	"    getpkgversion\t<string>\n"
	"    pkgmatch\t\t<pkg-version> <pkg-pattern>\n"
	"    register\t\t<pkgname> <version> <shortdesc>\n"
	"    sanitize-plist\t<plist>\n"
	"    unregister\t\t<pkgname> <version>\n"
	"    version\t\t<pkgname>\n"
	"\n"
	"  Options shared by all actions:\n"
	"    -d\t\tDebugging messages to stderr.\n"
	"    -r\t\t\t<rootdir>\n"
	"    -V\t\tPrints the xbps release version\n"
	"\n"
	"  Examples:\n"
	"    $ xbps-uhelper cmpver 'foo-1.0' 'foo-2.1'\n"
	"    $ xbps-uhelper digest /foo/blah.txt ...\n"
	"    $ xbps-uhelper fetch http://www.foo.org/file.blob ...\n"
	"    $ xbps-uhelper getpkgname foo-2.0\n"
	"    $ xbps-uhelper getpkgrevision foo-2.0_1\n"
	"    $ xbps-uhelper getpkgversion foo-2.0\n"
	"    $ xbps-uhelper register pkgname 2.0 \"A short description\"\n"
	"    $ xbps-uhelper sanitize-plist /blah/foo.plist\n"
	"    $ xbps-uhelper unregister pkgname 2.0\n"
	"    $ xbps-uhelper version pkgname\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	struct xbps_fetch_progress_data xfpd;
	prop_dictionary_t dict;
	const char *version;
	char *plist, *pkgname, *pkgver, *in_chroot_env, *hash;
	bool debug = false, in_chroot = false;
	int i, c, rv = 0;

	while ((c = getopt(argc, argv, "Vdr:")) != -1) {
		switch (c) {
		case 'r':
			/* To specify the root directory */
			xbps_set_rootdir(optarg);
			break;
		case 'd':
			debug = true;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	xbps_init(debug);

	plist = xbps_xasprintf("%s/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL) {
		fprintf(stderr,
		    "%s=> ERROR: couldn't find regpkdb file (%s)%s\n",
		    MSG_ERROR, strerror(errno), MSG_RESET);
		exit(EXIT_FAILURE);
	}

	in_chroot_env = getenv("in_chroot");
	if (in_chroot_env != NULL)
		in_chroot = true;

	if (strcasecmp(argv[0], "register") == 0) {
		/* Registers a package into the database */
		if (argc != 4)
			usage();

		dict = prop_dictionary_create();
		if (dict == NULL)
			exit(EXIT_FAILURE);
		prop_dictionary_set_cstring_nocopy(dict, "pkgname", argv[1]);
		prop_dictionary_set_cstring_nocopy(dict, "version", argv[2]);
		prop_dictionary_set_cstring_nocopy(dict, "short_desc", argv[3]);
		pkgver = xbps_xasprintf("%s-%s", argv[1], argv[2]);
		if (pkgver == NULL)
			exit(EXIT_FAILURE);
		prop_dictionary_set_cstring(dict, "pkgver", pkgver);
		free(pkgver);

		rv = xbps_set_pkg_state_installed(argv[1],
		    XBPS_PKG_STATE_INSTALLED);
		if (rv != 0)
			exit(EXIT_FAILURE);

		rv = xbps_register_pkg(dict, false);
		if (rv == EEXIST) {
			printf("%s%s=> %s-%s already registered.%s\n", MSG_WARN,
			    in_chroot ? "[chroot] " : "", argv[1], argv[2],
			    MSG_RESET);
		} else if (rv != 0) {
			fprintf(stderr, "%s%s=> couldn't register %s-%s "
			    "(%s).%s\n", MSG_ERROR,
			    in_chroot ? "[chroot] " : "" , argv[1], argv[2],
			    strerror(rv), MSG_RESET);
		} else {
			printf("%s%s=> %s-%s registered successfully.%s\n",
			    MSG_NORMAL, in_chroot ? "[chroot] " : "",
			    argv[1], argv[2], MSG_RESET);
		}

	} else if (strcasecmp(argv[0], "unregister") == 0) {
		/* Unregisters a package from the database */
		if (argc != 3)
			usage();

		if (!xbps_remove_pkg_dict_from_file(argv[1], plist)) {
			if (errno == ENOENT)
				fprintf(stderr, "%s=> ERROR: %s not registered "
				    "in database.%s\n", MSG_WARN, argv[1], MSG_RESET);
			else
				fprintf(stderr, "%s=> ERROR: couldn't unregister %s "
			    	    "from database (%s)%s\n", MSG_ERROR,
				    argv[1], strerror(errno), MSG_RESET);

			exit(EXIT_FAILURE);
		}

		printf("%s%s=> %s-%s unregistered successfully.%s\n",
		    MSG_NORMAL, in_chroot ? "[chroot] " : "", argv[1],
		    argv[2], MSG_RESET);

	} else if (strcasecmp(argv[0], "version") == 0) {
		/* Prints version of an installed package */
		if (argc != 2)
			usage();

		dict = xbps_find_pkg_dict_from_plist_by_name(plist, argv[1]);
		if (dict == NULL)
			exit(EXIT_FAILURE);

		prop_dictionary_get_cstring_nocopy(dict, "version", &version);
		printf("%s\n", version);
		prop_object_release(dict);

	} else if (strcasecmp(argv[0], "sanitize-plist") == 0) {
		/* Sanitize a plist file (properly indent the file) */
		if (argc != 2)
			usage();

		dict = prop_dictionary_internalize_from_zfile(argv[1]);
		if (dict == NULL) {
			fprintf(stderr,
			    "=> ERROR: couldn't sanitize %s plist file "
			    "(%s)\n", argv[1], strerror(errno));
			exit(EXIT_FAILURE);
		}
		write_plist_file(dict, argv[1]);

	} else if (strcasecmp(argv[0], "getpkgversion") == 0) {
		/* Returns the version of a pkg string */
		if (argc != 2)
			usage();

		version = xbps_get_pkg_version(argv[1]);
		if (version == NULL) {
			fprintf(stderr,
			    "Invalid string, expected <string>-<version>\n");
			exit(EXIT_FAILURE);
		}
		printf("%s\n", version);

	} else if (strcasecmp(argv[0], "getpkgname") == 0) {
		/* Returns the name of a pkg string */
		if (argc != 2)
			usage();

		pkgname = xbps_get_pkg_name(argv[1]);
		if (pkgname == NULL) {
			fprintf(stderr,
			    "Invalid string, expected <string>-<version>\n");
			exit(EXIT_FAILURE);
		}
		printf("%s\n", pkgname);
		free(pkgname);

	} else if (strcasecmp(argv[0], "getpkgrevision") == 0) {
		/* Returns the revision of a pkg string */
		if (argc != 2)
			usage();

		version = xbps_get_pkg_revision(argv[1]);
		if (version == NULL)
			exit(EXIT_SUCCESS);

		printf("%s\n", version);

	} else if (strcasecmp(argv[0], "getpkgdepname") == 0) {
		/* Returns the pkgname of a dependency */
		if (argc != 2)
			usage();

		pkgname = xbps_get_pkgpattern_name(argv[1]);
		if (pkgname == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", pkgname);
		free(pkgname);
	} else if (strcasecmp(argv[0], "getpkgdepversion") == 0) {
		/* returns the version of a package pattern dependency */
		if (argc != 2)
			usage();

		version = xbps_get_pkgpattern_version(argv[1]);
		if (version == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", version);

	} else if (strcasecmp(argv[0], "pkgmatch") == 0) {
		/* Matches a pkg with a pattern */
		if (argc != 3)
			usage();

		exit(xbps_pkgpattern_match(argv[1], argv[2]));

	} else if (strcasecmp(argv[0], "cmpver") == 0) {
		/* Compare two version strings, installed vs required */
		if (argc != 3)
			usage();

		exit(xbps_cmpver(argv[1], argv[2]));

	} else if (strcasecmp(argv[0], "digest") == 0) {
		/* Prints SHA256 hashes for specified files */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			hash = xbps_get_file_hash(argv[i]);
			if (hash == NULL) {
				fprintf(stderr,
				    "E: couldn't get hash for %s (%s)\n",
				    argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", hash);
			free(hash);
		}

	} else if (strcasecmp(argv[0], "fetch") == 0) {
		/* Fetch a file from specified URL */
		if (argc != 2)
			usage();

		for (i = 1; i < argc; i++) {
			rv = xbps_fetch_file(argv[i], ".", false, "v",
			    fetch_file_progress_cb, &xfpd);
			if (rv == -1) {
				printf("%s: %s\n", argv[1],
				    xbps_fetch_error_string());
				exit(EXIT_FAILURE);
			} else if (rv == 0) {
				printf("%s: file is identical than remote.\n",
				    argv[1]);
			}
		}

	} else {
		usage();
	}

	xbps_end();

	exit(EXIT_SUCCESS);
}
