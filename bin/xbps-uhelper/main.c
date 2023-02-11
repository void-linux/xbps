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
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>

#include <xbps.h>
#include "../xbps-install/defs.h"

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"usage: xbps-uhelper [options] [action] [args]\n"
	"\n"
	"  Available actions:\n"
	"    binpkgarch, binpkgver, cmpver, getpkgdepname,\n"
	"    getpkgname, getpkgrevision, getpkgversion, pkgmatch, version,\n"
	"    real-version, arch, getsystemdir, getname, getversion\n"
	"\n"
	"  Action arguments:\n"
	"    binpkgarch          <binpkg> ...\n"
	"    binpkgver           <binpkg> ...\n"
	"    cmpver              <instver> <reqver>\n"
	"    getpkgdepname       <string> ...\n"
	"    getpkgdepversion    <string> ...\n"
	"    getpkgname          <string> ...\n"
	"    getpkgrevision      <string> ...\n"
	"    getpkgversion       <string> ...\n"
	"    getname             <string> ...\n"
	"    getversion          <string> ...\n"
	"    pkgmatch            <pkg-version> <pkg-pattern>\n"
	"    version             <pkgname> ...\n"
	"    real-version        <pkgname> ...\n"
	"\n"
	"  Options shared by all actions:\n"
	"    -C, --config     Path to xbps.conf file.\n"
	"    -d, --debug      Debugging messages to stderr.\n"
	"    -r, --rootdir    <rootdir>\n"
	"    -V, --version    Prints the xbps release version\n");

	exit(EXIT_FAILURE);
}

static char *
fname(char *url)
{
	char *filename;

	if ((filename = strrchr(url, '>'))) {
		*filename = '\0';
	} else {
		filename = strrchr(url, '/');
	}
	if (filename == NULL)
		return NULL;
	return filename + 1;
}

int
main(int argc, char **argv)
{
	xbps_dictionary_t dict;
	struct xbps_handle xh;
	struct xferstat xfer;
	const char *version, *rootdir = NULL, *confdir = NULL;
	char pkgname[XBPS_NAME_SIZE], *filename;
	int flags = 0, c, rv = 0, i = 0;
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "debug", no_argument, NULL, 'd' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "C:dr:V", longopts, NULL)) != -1) {
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

	if ((strcmp(argv[0], "version") == 0) ||
	    (strcmp(argv[0], "real-version") == 0) ||
	    (strcmp(argv[0], "arch") == 0) ||
	    (strcmp(argv[0], "fetch") == 0) ||
	    (strcmp(argv[0], "getsystemdir") == 0)) {
		/*
		* Initialize libxbps.
		*/
		xh.fetch_cb = fetch_file_progress_cb;
		xh.fetch_cb_data = &xfer;
		xh.flags = flags;
		if (rootdir)
			xbps_strlcpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
		if (confdir)
			xbps_strlcpy(xh.confdir, confdir, sizeof(xh.confdir));
		if ((rv = xbps_init(&xh)) != 0) {
			xbps_error_printf("xbps-uhelper: failed to "
			    "initialize libxbps: %s.\n", strerror(rv));
			exit(EXIT_FAILURE);
		}
	}

	if (strcmp(argv[0], "version") == 0) {
		/* Prints version of installed packages */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			if ((((dict = xbps_pkgdb_get_pkg(&xh, argv[i])) == NULL)) &&
				(((dict = xbps_pkgdb_get_virtualpkg(&xh, argv[i])) == NULL))) {
				xbps_error_printf("Could not find package '%s'\n", argv[i]);
				rv = 1;
			} else {
				xbps_dictionary_get_cstring_nocopy(dict, "pkgver", &version);
				printf("%s\n", xbps_pkg_version(version));
			}
		}
	} else if (strcmp(argv[0], "real-version") == 0) {
		/* Prints version of installed real packages, not virtual */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			if ((dict = xbps_pkgdb_get_pkg(&xh, argv[i])) == NULL) {
				xbps_error_printf("Could not find package '%s'\n", argv[i]);
				rv = 1;
			} else {
				xbps_dictionary_get_cstring_nocopy(dict, "pkgver", &version);
				printf("%s\n", xbps_pkg_version(version));
			}
		}
	} else if (strcmp(argv[0], "getpkgversion") == 0) {
		/* Returns the version of pkg strings */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			version = xbps_pkg_version(argv[i]);
			if (version == NULL) {
				xbps_error_printf(
					"Invalid string '%s', expected <string>-<version>_<revision>\n", argv[i]);
				rv = 1;
			} else {
				printf("%s\n", version);
			}
		}
	} else if (strcmp(argv[0], "getpkgname") == 0) {
		/* Returns the name of pkg strings */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			if (!xbps_pkg_name(pkgname, sizeof(pkgname), argv[i])) {
				xbps_error_printf(
					"Invalid string '%s', expected <string>-<version>_<revision>\n", argv[i]);
				rv = 1;
			} else {
				printf("%s\n", pkgname);
			}
		}
	} else if (strcmp(argv[0], "getpkgrevision") == 0) {
		/* Returns the revision of pkg strings */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			version = xbps_pkg_revision(argv[1]);
			if (version == NULL) {
				rv = 1;
			} else {
				printf("%s\n", version);
			}
		}
	} else if (strcmp(argv[0], "getpkgdepname") == 0) {
		/* Returns the pkgname of dependencies */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			if (!xbps_pkgpattern_name(pkgname, sizeof(pkgname), argv[i])) {
				xbps_error_printf("Invalid string '%s', expected <string><comparator><version>\n", argv[i]);
				rv = 1;
			} else {
				printf("%s\n", pkgname);
			}
		}
	} else if (strcmp(argv[0], "getpkgdepversion") == 0) {
		/* returns the version of package pattern dependencies */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			version = xbps_pkgpattern_version(argv[i]);
			if (version == NULL) {
				xbps_error_printf("Invalid string '%s', expected <string><comparator><version>\n", argv[i]);
				rv = 1;
			} else {
				printf("%s\n", version);
			}
		}
	} else if (strcmp(argv[0], "getname") == 0) {
		/* returns the name of a pkg strings or pkg patterns */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			if (xbps_pkgpattern_name(pkgname, sizeof(pkgname), argv[i]) ||
				xbps_pkg_name(pkgname, sizeof(pkgname), argv[i])) {
				printf("%s\n", pkgname);
			} else {
				xbps_error_printf(
					"Invalid string '%s', expected <string><comparator><version> "
					"or <string>-<version>_<revision>\n", argv[i]);
				rv = 1;
			}
		}
	} else if (strcmp(argv[0], "getversion") == 0) {
		/* returns the version of a pkg strings or pkg patterns */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			version = xbps_pkgpattern_version(argv[i]);
			if (version == NULL) {
				version = xbps_pkg_version(argv[i]);
				if (version == NULL) {
					xbps_error_printf(
						"Invalid string '%s', expected <string><comparator><version> "
						"or <string>-<version>_<revision>\n", argv[i]);
					rv = 1;
					continue;
				}
			}
			printf("%s\n", version);
		}
	} else if (strcmp(argv[0], "binpkgver") == 0) {
		/* Returns the pkgver of binpkg strings */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			version = xbps_binpkg_pkgver(argv[i]);
			if (version == NULL) {
				xbps_error_printf(
					"Invalid string '%s', expected <pkgname>-<version>_<revision>.<arch>.xbps\n", argv[i]);
				rv = 1;
			} else {
				printf("%s\n", version);
			}
		}
	} else if (strcmp(argv[0], "binpkgarch") == 0) {
		/* Returns the arch of binpkg strings */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			version = xbps_binpkg_arch(argv[i]);
			if (version == NULL) {
				xbps_error_printf(
					"Invalid string '%s', expected <pkgname>-<version>_<revision>.<arch>.xbps\n", argv[i]);
				rv = 1;
			} else {
				printf("%s\n", version);
			}
		}
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
	} else if (strcmp(argv[0], "arch") == 0) {
		/* returns the xbps native arch */
		if (argc != 1)
			usage();

		if (xh.native_arch[0] && xh.target_arch && strcmp(xh.native_arch, xh.target_arch)) {
			printf("%s\n", xh.target_arch);
		} else {
			printf("%s\n", xh.native_arch);
		}
	} else if (strcmp(argv[0], "getsystemdir") == 0) {
		/* returns the xbps system directory (<sharedir>/xbps.d) */
		if (argc != 1)
			usage();

		printf("%s\n", XBPS_SYSDEFCONF_PATH);
	} else if (strcmp(argv[0], "digest") == 0) {
		char sha256[XBPS_SHA256_SIZE];

		/* Prints SHA256 hashes for specified files */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			if (!xbps_file_sha256(sha256, sizeof sha256, argv[i])) {
				xbps_error_printf(
				    "couldn't get hash for %s (%s)\n",
				    argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", sha256);
		}
	} else if (strcmp(argv[0], "fetch") == 0) {
		/* Fetch a file from specified URL */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			filename = fname(argv[i]);
			rv = xbps_fetch_file_dest(&xh, argv[i], filename, "v");

			if (rv == -1) {
				xbps_error_printf("%s: %s\n", argv[i],
				    xbps_fetch_error_string());
			} else if (rv == 0) {
				printf("%s: file is identical with remote.\n", argv[i]);
			} else {
				rv = 0;
			}
		}
	} else {
		usage();
	}

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
