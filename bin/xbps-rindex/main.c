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

static void __attribute__((noreturn))
usage(bool fail)
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
	    "MODE\n"
	    " -a, --add <repodir/file.xbps> ...  Add package(s) to repository index\n"
	    " -c, --clean <repodir>              Clean repository index\n"
	    " -r, --remove-obsoletes <repodir>   Removes obsolete packages from repository\n"
	    " -s, --sign <repodir>               Initialize repository metadata signature\n"
	    " -S, --sign-pkg <file.xbps> ...     Sign binary package archive\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	const char *shortopts = "acdfhrsCSVv";
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
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	const char *compression = NULL;
	const char *privkey = NULL, *signedby = NULL;
	int rv, c, flags = 0;
	bool add_mode, clean_mode, rm_mode, sign_mode, sign_pkg_mode, force,
			 hashcheck;

	add_mode = clean_mode = rm_mode = sign_mode = sign_pkg_mode = force =
		hashcheck = false;

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
			add_mode = true;
			break;
		case 'c':
			clean_mode = true;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'f':
			force = true;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'r':
			rm_mode = true;
			break;
		case 's':
			sign_mode = true;
			break;
		case 'C':
			hashcheck = true;
			break;
		case 'S':
			sign_pkg_mode = true;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage(true);
			/* NOTREACHED */
		}
	}
	if ((argc == optind) ||
	    (!add_mode && !clean_mode && !rm_mode && !sign_mode && !sign_pkg_mode)) {
		usage(true);
		/* NOTREACHED */
	} else if ((add_mode && (clean_mode || rm_mode || sign_mode || sign_pkg_mode)) ||
		   (clean_mode && (add_mode || rm_mode || sign_mode || sign_pkg_mode)) ||
		   (rm_mode && (add_mode || clean_mode || sign_mode || sign_pkg_mode)) ||
		   (sign_mode && (add_mode || clean_mode || rm_mode || sign_pkg_mode)) ||
		   (sign_pkg_mode && (add_mode || clean_mode || rm_mode || sign_mode))) {
		xbps_error_printf("Only one mode can be specified: add, clean, "
		    "remove-obsoletes, sign or sign-pkg.\n");
		exit(EXIT_FAILURE);
	}

	/* initialize libxbps */
	memset(&xh, 0, sizeof(xh));
	xh.flags = flags;
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (add_mode)
		rv = index_add(&xh, optind, argc, argv, force, compression);
	else if (clean_mode)
		rv = index_clean(&xh, argv[optind], hashcheck, compression);
	else if (rm_mode)
		rv = remove_obsoletes(&xh, argv[optind]);
	else if (sign_mode)
		rv = sign_repo(&xh, argv[optind], privkey, signedby, compression);
	else if (sign_pkg_mode)
		rv = sign_pkgs(&xh, optind, argc, argv, privkey, force);

	exit(rv);
}
