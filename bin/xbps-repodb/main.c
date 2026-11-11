/*-
 * Copyright (c) 2020 Duncan Overbruck <mail@duncano.de>
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
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include <xbps.h>

#include "defs.h"


static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"Usage: xbps-repodb [OPTIONS] MODE <repository>...\n\n"
	"OPTIONS:\n"
	" -d, --debug    Enable debug messages to stderr\n"
	" -n, --dry-run  Dry-run mode\n"
	" -v, --verbose  Enable verbose output\n"
	" -V, --version  Prints the xbps release version\n"
	"MODE:\n"
	" -p, --purge    Remove obsolete binary packages from repositories\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	int c = 0, rv = 0;
	struct xbps_handle xh;
	const struct option longopts[] = {
		{ "debug", no_argument, NULL, 'd' },
		{ "dry-run", no_argument, NULL, 'n' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	bool dry = false;
	enum {
		MODE_NIL,
		MODE_PURGE,
	} mode = MODE_NIL;

	memset(&xh, 0, sizeof xh);

	while ((c = getopt_long(argc, argv, "dhnpVv", longopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			xh.flags |= XBPS_FLAG_DEBUG;
			break;
		case 'p':
			mode = MODE_PURGE;
			break;
		case 'n':
			dry = true;
			break;
		case 'v':
			xh.flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		case 'h':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0 || mode == MODE_NIL)
		usage();

	/*
	* Initialize libxbps.
	*/
	xh.flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	switch (mode) {
	case MODE_PURGE:
		rv = purge_repos(&xh, argc, argv, dry);
	case MODE_NIL:
		break;
	}

	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
