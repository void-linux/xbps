/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
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
#include "../xbps-install/defs.h"

static void
write_plist_file(xbps_dictionary_t dict, const char *file)
{
	assert(dict != NULL || file != NULL);

	if (!xbps_dictionary_externalize_to_zfile(dict, file)) {
		xbps_object_release(dict);
		xbps_error_printf("xbps-uhelper: couldn't write to %s: %s\n",
		    file, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"usage: xbps-uhelper [options] [action] [args]\n"
	"\n"
	"  Available actions:\n"
	"    cmpver, digest, fetch, getpkgdepname, getpkgname, getpkgrevision,\n"
	"    getpkgversion, pkgmatch, sanitize-plist, version.\n"
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
	"    sanitize-plist\t<plist>\n"
	"    version\t\t<pkgname>\n"
	"\n"
	"  Options shared by all actions:\n"
	"    -C\t\tPath to xbps.conf file.\n"
	"    -d\t\tDebugging messages to stderr.\n"
	"    -r\t\t<rootdir>\n"
	"    -V\t\tPrints the xbps release version\n"
	"\n"
	"  Examples:\n"
	"    $ xbps-uhelper cmpver 'foo-1.0_1' 'foo-2.1_1'\n"
	"    $ xbps-uhelper digest file ...\n"
	"    $ xbps-uhelper fetch http://www.foo.org/file.blob ...\n"
	"    $ xbps-uhelper getpkgdepname 'foo>=0'\n"
	"    $ xbps-uhelper getpkgdepversion 'foo>=0'\n"
	"    $ xbps-uhelper getpkgname foo-2.0_1\n"
	"    $ xbps-uhelper getpkgrevision foo-2.0_1\n"
	"    $ xbps-uhelper getpkgversion foo-2.0_1\n"
	"    $ xbps-uhelper pkgmatch foo-1.0_1 'foo>=1.0'\n"
	"    $ xbps-uhelper sanitize-plist foo.plist\n"
	"    $ xbps-uhelper version pkgname\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	xbps_dictionary_t dict;
	struct xbps_handle xh;
	struct xferstat xfer;
	const char *version, *rootdir = NULL, *confdir = NULL;
	char *pkgname, *hash;
	int flags = 0, i, c, rv = 0;

	while ((c = getopt(argc, argv, "C:dr:V")) != -1) {
		switch (c) {
		case 'C':
			confdir = optarg;
			break;
		case 'r':
			/* To specify the root directory */
			rootdir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
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

	memset(&xh, 0, sizeof(xh));

	if ((strcasecmp(argv[0], "version") == 0) ||
	    (strcasecmp(argv[0], "fetch") == 0)) {
		/*
		* Initialize libxbps.
		*/
		xh.flags = flags;
		xh.fetch_cb = fetch_file_progress_cb;
		xh.fetch_cb_data = &xfer;
		xh.rootdir = rootdir;
		xh.conffile = confdir;
		if ((rv = xbps_init(&xh)) != 0) {
			xbps_error_printf("xbps-uhelper: failed to "
			    "initialize libxbps: %s.\n", strerror(rv));
			exit(EXIT_FAILURE);
		}
	}

	if (strcasecmp(argv[0], "version") == 0) {
		/* Prints version of an installed package */
		if (argc != 2)
			usage();

		if ((((dict = xbps_pkgdb_get_pkg(&xh, argv[1])) == NULL)) &&
		    (((dict = xbps_pkgdb_get_virtualpkg(&xh, argv[1])) == NULL)))
			exit(EXIT_FAILURE);

		xbps_dictionary_get_cstring_nocopy(dict, "pkgver", &version);
		printf("%s\n", xbps_pkg_version(version));
	} else if (strcasecmp(argv[0], "sanitize-plist") == 0) {
		/* Sanitize a plist file (properly indent the file) */
		if (argc != 2)
			usage();

		dict = xbps_dictionary_internalize_from_zfile(argv[1]);
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

		version = xbps_pkg_version(argv[1]);
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

		pkgname = xbps_pkg_name(argv[1]);
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

		version = xbps_pkg_revision(argv[1]);
		if (version == NULL)
			exit(EXIT_SUCCESS);

		printf("%s\n", version);
	} else if (strcasecmp(argv[0], "getpkgdepname") == 0) {
		/* Returns the pkgname of a dependency */
		if (argc != 2)
			usage();

		pkgname = xbps_pkgpattern_name(argv[1]);
		if (pkgname == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", pkgname);
		free(pkgname);
	} else if (strcasecmp(argv[0], "getpkgdepversion") == 0) {
		/* returns the version of a package pattern dependency */
		if (argc != 2)
			usage();

		version = xbps_pkgpattern_version(argv[1]);
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
			hash = xbps_file_hash(argv[i]);
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
			rv = xbps_fetch_file(&xh, argv[i], "v");
			if (rv == -1) {
				printf("%s: %s\n", argv[1],
				    xbps_fetch_error_string());
			} else if (rv == 0) {
				printf("%s: file is identical than remote.\n",
				    argv[1]);
			} else
				rv = 0;
		}
	} else {
		usage();
	}

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
