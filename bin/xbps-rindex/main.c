/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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
	    " -f --force                        Force mode to overwrite entry in add mode\n"
	    " -h --help                         Show help usage\n"
	    " -V --version                      Show XBPS version\n"
	    "    --privkey <key>                Path to the private key for signing\n"
	    "    --signedby <string>            Signature details, i.e \"name <email>\"\n\n"
	    "MODE\n"
	    " -a --add <repodir/pkg> ...        Add package(s) to repository index\n"
	    " -r --remove-obsoletes <repodir>   Removes obsolete packages from repository\n"
	    " -s --sign <repodir>               Sign repository index\n\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	const char *shortopts = "afhrV";
	struct option longopts[] = {
		{ "add", no_argument, NULL, 'a' },
		{ "force", no_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "remove-obsoletes", no_argument, NULL, 'r' },
		{ "version", no_argument, NULL, 'V' },
		{ "privkey", required_argument, NULL, 0},
		{ "signedby", required_argument, NULL, 1},
		{ "sign", no_argument, NULL, 's'},
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	const char *privkey = NULL, *signedby = NULL;
	int rv, c;
	bool add_mode, rm_mode, sign_mode, force;

	add_mode = rm_mode = sign_mode = force = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 0:
			privkey = optarg;
			break;
		case 1:
			signedby = optarg;
			break;
		case 'a':
			add_mode = true;
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
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		}
	}
	if ((argc == optind) || (!add_mode && !rm_mode && !sign_mode)) {
		usage(true);
	} else if ((add_mode && (rm_mode || sign_mode)) ||
		   (rm_mode && (add_mode || sign_mode)) ||
		   (sign_mode && (add_mode || rm_mode))) {
		fprintf(stderr, "Only one mode can be specified: add, "
		    "remove-obsoletes or sign.\n");
		exit(EXIT_FAILURE);
	}

	/* initialize libxbps */
	memset(&xh, 0, sizeof(xh));
	if ((rv = xbps_init(&xh)) != 0) {
		fprintf(stderr, "failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (add_mode)
		rv = index_add(&xh, argc - optind, argv + optind, force);
	else if (rm_mode)
		rv = remove_obsoletes(&xh, argv[optind]);
	else if (sign_mode)
		rv = sign_repo(&xh, argv[optind], privkey, signedby);

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
