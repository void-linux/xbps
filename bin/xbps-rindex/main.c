/*-
 * Copyright (c) 2012-2019 Juan Romero Pardines.
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
#include <getopt.h>

#include "defs.h"
#include "xbps/xbps_array.h"

static void __attribute__((noreturn))
usage(int status)
{
	fprintf(stdout,
	    "Usage: xbps-rindex [OPTIONS] MODE ARGUMENTS\n\n"
	    "OPTIONS\n"
	    " -d, --debug                        Debug mode shown to stderr\n"
	    " -f, --force                        Force mode to overwrite entry in add mode\n"
	    " -h, --help                         Show usage\n"
	    " -v, --verbose                      Verbose messages\n"
	    " -V, --version                      Show XBPS version\n"
	    " -C, --hashcheck                    Consider file hashes for cleaning up packages\n"
	    "     --compression <fmt>            Compression format: none, gzip, bzip2, lz4, xz, zstd (default)\n"
	    "     --privkey <key>                Path to the private key for signing\n"
	    "     --signedby <string>            Signature details, i.e \"name <email>\"\n\n"
	    " -R, --repository <dir>             Add a local repository\n"
	    "MODE\n"
	    " -a, --add <repodir/file.xbps> ...  Add package(s) to repository index\n"
	    " -c, --clean <repodir>              Clean repository index\n"
	    " -r, --remove-obsoletes <repodir>   Removes obsolete packages from repository\n"
	    " -s, --sign <repodir>               Initialize repository metadata signature\n"
	    " -S, --sign-pkg <file.xbps> ...     Sign binary package archive\n");
	exit(status);
}

static void __attribute__((noreturn))
multiple_mode_error(void)
{
	xbps_error_printf("only one mode can be specified: add, clean, "
	    "remove-obsoletes, sign or sign-pkg.\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	const char *shortopts = "acdfhrR:sCSVv";
	struct option longopts[] = {
		{ "add", no_argument, NULL, 'a' },
		{ "clean", no_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "force", no_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "remove-obsoletes", no_argument, NULL, 'r' },
		{ "version", no_argument, NULL, 'V' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "privkey", required_argument, NULL, 0},
		{ "signedby", required_argument, NULL, 1},
		{ "sign", no_argument, NULL, 's'},
		{ "sign-pkg", no_argument, NULL, 'S'},
		{ "hashcheck", no_argument, NULL, 'C' },
		{ "compression", required_argument, NULL, 2},
		{ "repository", required_argument, NULL, 'R'},
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh = {0};
	const char *compression = NULL;
	const char *privkey = NULL, *signedby = NULL;
	int rv, c;
	xbps_array_t repos = NULL;
	enum {
		INDEX_ADD = 1,
		CLEAN_INDEX,
		REMOVE_OBSOLETES,
		SIGN_REPO,
		SIGN_PACKAGE,
	} mode = 0;
	bool force = false, hashcheck = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 0:
			privkey = optarg;
			break;
		case 1:
			signedby = optarg;
			break;
		case 2:
			compression = optarg;
			break;
		case 'a':
			if (mode != 0)
				multiple_mode_error();
			mode = INDEX_ADD;
			break;
		case 'c':
			if (mode != 0)
				multiple_mode_error();
			mode = CLEAN_INDEX;
			break;
		case 'd':
			xh.flags |= XBPS_FLAG_DEBUG;
			break;
		case 'f':
			force = true;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			/* NOTREACHED */
		case 'r':
			if (mode != 0)
				multiple_mode_error();
			mode = REMOVE_OBSOLETES;
			break;
		case 's':
			if (mode != 0)
				multiple_mode_error();
			mode = SIGN_REPO;
			break;
		case 'C':
			hashcheck = true;
			break;
		case 'R':
			if (!repos)
				repos = xbps_array_create();
			if (!repos || !xbps_array_add_cstring(repos, optarg)) {
				xbps_error_oom();
				exit(1);
			}
			break;
		case 'S':
			if (mode != 0)
				multiple_mode_error();
			mode = SIGN_PACKAGE;
			break;
		case 'v':
			xh.flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if ((argc == optind) || mode == 0) {
		usage(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* initialize libxbps */
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	switch (mode) {
	case INDEX_ADD:
		rv = index_add(&xh, argc - optind, argv + optind, force,
		    compression, repos);
		break;
	case CLEAN_INDEX:
		rv = index_clean(&xh, argv[optind], hashcheck, compression);
		break;
	case REMOVE_OBSOLETES:
		rv = remove_obsoletes(&xh, argv[optind]);
		break;
	case SIGN_REPO:
		rv = sign_repo(&xh, argv[optind], privkey, signedby, compression);
		break;
	case SIGN_PACKAGE:
		rv = sign_pkgs(&xh, optind, argc, argv, privkey, force);
		break;
	}

	exit(rv);
}
