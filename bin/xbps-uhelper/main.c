/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
 * Copyright (c) 2014 Enno Boland.
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

#include <xbps.h>
#include "../xbps-install/defs.h"

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"usage: xbps-uhelper [options] [action] [args]\n"
	"\n"
	"  Available actions:\n"
	"    cmpver, digest, fetch, getpkgdepname, getpkgname, getpkgrevision,\n"
	"    getpkgversion, pkgmatch, version, real-version.\n"
	"\n"
	"  Action arguments:\n"
	"    cmpver\t\t<instver> <reqver>\n"
	"    digest\t\t<file> <file1+N>\n"
	"    fetch\t\t<URL[>filename]> <URL1+N[>filename]>\n"
	"    getpkgdepname\t<string>\n"
	"    getpkgdepversion\t<string>\n"
	"    getpkgname\t\t<string>\n"
	"    getpkgrevision\t<string>\n"
	"    getpkgversion\t<string>\n"
	"    pkgmatch\t\t<pkg-version> <pkg-pattern>\n"
	"    version\t\t<pkgname>\n"
	"    real-version\t<pkgname>\n"
	"    xfetch\t\t<oldfile> <URL[>filename]>\n"
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
	"    $ xbps-uhelper version pkgname\n");

	exit(EXIT_FAILURE);
}

static char*
fname(char *url) {
	char *filename;

	if( (filename = strrchr(url, '>')) ) {
		*filename = '\0';
	} else {
		filename = strrchr(url, '/');
	}
	if(filename == NULL)
		return NULL;
	return filename + 1;
}

int
main(int argc, char **argv)
{
	xbps_dictionary_t dict;
	struct xbps_handle xh;
	struct xferstat xfer;
	const char *version, *rootdir = NULL, *conffile = NULL;
	char *pkgname, *hash, *filename;
	int flags = 0, c, rv = 0;

	while ((c = getopt(argc, argv, "C:dr:V")) != -1) {
		switch (c) {
		case 'C':
			conffile = optarg;
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

	if ((strcmp(argv[0], "version") == 0) ||
	    (strcmp(argv[0], "real-version") == 0) ||
	    (strcmp(argv[0], "fetch") == 0)) {
		/*
		* Initialize libxbps.
		*/
		xh.flags = flags;
		xh.fetch_cb = fetch_file_progress_cb;
		xh.fetch_cb_data = &xfer;
		if (rootdir)
			strncpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
		if (conffile)
			strncpy(xh.conffile, conffile, sizeof(xh.conffile));
		if ((rv = xbps_init(&xh)) != 0) {
			xbps_error_printf("xbps-uhelper: failed to "
			    "initialize libxbps: %s.\n", strerror(rv));
			exit(EXIT_FAILURE);
		}
	}

	if (strcmp(argv[0], "version") == 0) {
		/* Prints version of an installed package */
		if (argc != 2)
			usage();

		if ((((dict = xbps_pkgdb_get_pkg(&xh, argv[1])) == NULL)) &&
		    (((dict = xbps_pkgdb_get_virtualpkg(&xh, argv[1])) == NULL)))
			exit(EXIT_FAILURE);

		xbps_dictionary_get_cstring_nocopy(dict, "pkgver", &version);
		printf("%s\n", xbps_pkg_version(version));
	} else if (strcmp(argv[0], "real-version") == 0) {
		/* Prints version of an installed real package, not virtual */
		if (argc != 2)
			usage();

		if ((dict = xbps_pkgdb_get_pkg(&xh, argv[1])) == NULL)
			exit(EXIT_FAILURE);

		xbps_dictionary_get_cstring_nocopy(dict, "pkgver", &version);
		printf("%s\n", xbps_pkg_version(version));
	} else if (strcmp(argv[0], "getpkgversion") == 0) {
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
	} else if (strcmp(argv[0], "getpkgname") == 0) {
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
	} else if (strcmp(argv[0], "getpkgrevision") == 0) {
		/* Returns the revision of a pkg string */
		if (argc != 2)
			usage();

		version = xbps_pkg_revision(argv[1]);
		if (version == NULL)
			exit(EXIT_SUCCESS);

		printf("%s\n", version);
	} else if (strcmp(argv[0], "getpkgdepname") == 0) {
		/* Returns the pkgname of a dependency */
		if (argc != 2)
			usage();

		pkgname = xbps_pkgpattern_name(argv[1]);
		if (pkgname == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", pkgname);
		free(pkgname);
	} else if (strcmp(argv[0], "getpkgdepversion") == 0) {
		/* returns the version of a package pattern dependency */
		if (argc != 2)
			usage();

		version = xbps_pkgpattern_version(argv[1]);
		if (version == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", version);
	} else if (strcmp(argv[0], "pkgmatch") == 0) {
		/* Matches a pkg with a pattern */
		if (argc != 3)
			usage();

		exit(xbps_pkgpattern_match(argv[1], argv[2]));
	} else if (strcmp(argv[0], "cmpver") == 0) {
		/* Compare two version strings, installed vs required */
		if (argc != 3)
			usage();

		exit(xbps_cmpver(argv[1], argv[2]));
	} else if (strcmp(argv[0], "digest") == 0) {
		/* Prints SHA256 hashes for specified files */
		if (argc < 2)
			usage();

		for (int i = 1; i < argc; i++) {
			hash = xbps_file_hash(argv[i]);
			if (hash == NULL) {
				fprintf(stderr,
				    "E: couldn't get hash for %s (%s)\n",
				    argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", hash);
		}
	} else if (strcmp(argv[0], "xfetch") == 0) {
		/* apply a delta from specified URL */
		if (argc != 3)
			usage();

		filename = fname(argv[2]);
		rv = xbps_fetch_delta(&xh, argv[1], argv[2], filename, "v");

		if (rv == -1) {
			printf("%s: %s\n", argv[2],
				xbps_fetch_error_string());
		} else if (rv == 0) {
			printf("%s: file is identical than remote.\n",
				argv[3]);
		} else
			rv = 0;
	} else if (strcmp(argv[0], "fetch") == 0) {
		/* Fetch a file from specified URL */
		if (argc != 2)
			usage();

		for (int i = 1; i < argc; i++) {
			filename = fname(argv[i]);
			rv = xbps_fetch_file_dest(&xh, argv[i], filename, "v");

			if (rv == -1) {
				printf("%s: %s\n", argv[i],
				    xbps_fetch_error_string());
			} else if (rv == 0) {
				printf("%s: file is identical than remote.\n",
				    argv[i]);
			} else
				rv = 0;
		}
	} else {
		usage();
	}

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
