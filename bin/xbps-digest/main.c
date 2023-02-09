/*-
 * Copyright (c) 2019 Juan Romero Pardines.
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

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	"Usage: xbps-digest [options] [file] [file+N]\n"
	"\n"
	"OPTIONS\n"
	" -h, --help           Show usage\n"
	" -m, --mode <sha256>  Selects the digest mode, sha256 (default)\n"
	" -V, --version        Show XBPS version\n"
	"\nNOTES\n"
	" If [file] not set, reads from stdin\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	int c;
	char sha256[XBPS_SHA256_SIZE];
	const char *mode = NULL, *progname = argv[0];
	const struct option longopts[] = {
		{ "mode", required_argument, NULL, 'm' },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "m:hV", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'm':
			mode = optarg;
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

	argc -= optind;
	argv += optind;

	if (mode && strcmp(mode, "sha256")) {
		/* sha256 is the only supported mode currently */
		xbps_error_printf("%s: unsupported digest mode\n", progname);
		exit(EXIT_FAILURE);
	}

	if (argc < 1) {
		if (!xbps_file_sha256(sha256, sizeof sha256, "/dev/stdin"))
			exit(EXIT_FAILURE);

		printf("%s\n", sha256);
	} else {
		for (int i = 0; i < argc; i++) {
			if (!xbps_file_sha256(sha256, sizeof sha256, argv[i])) {
				xbps_error_printf(
				    "%s: couldn't get hash for %s (%s)\n",
				progname, argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", sha256);
		}
	}
	exit(EXIT_SUCCESS);
}
