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

enum {
	MODE_DEFAULT,
	MODE_SHA256,
	MODE_BLAKE3
};

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	"Usage: xbps-digest [options] [file] [file+N]\n"
	"\n"
	"OPTIONS\n"
	" -h, --help                  Show usage\n"
	" -m, --mode <blake3|sha256>  Digest mode (sha256 default)\n"
	" -V, --version               Show XBPS version\n"
	"\nNOTES\n"
	" If [file] not set, reads from stdin\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	int c, dmode = MODE_DEFAULT;
	char digest[XBPS_DIGEST_SIZE];
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

	if (mode) {
		if (strcmp(mode, "blake3") == 0) {
			dmode = MODE_BLAKE3;
		} else if (strcmp(mode, "sha256") == 0) {
			dmode = MODE_SHA256;
		} else {
			fprintf(stderr, "%s: unsupported digest mode %s\n", progname, mode);
			exit(EXIT_FAILURE);
		}
	}

	if (argc < 1) {
		if (dmode == MODE_BLAKE3) {
			if (!xbps_file_blake3(digest, sizeof digest, "/dev/stdin"))
				exit(EXIT_FAILURE);

		} else {
			if (!xbps_file_sha256(digest, sizeof digest, "/dev/stdin"))
				exit(EXIT_FAILURE);

		}
		printf("%s\n", digest);

	} else {
		for (int i = 0; i < argc; i++) {
			if (dmode == MODE_BLAKE3) {
				if (!xbps_file_blake3(digest, sizeof digest, argv[i])) {
					fprintf(stderr,
					    "%s: couldn't get hash for %s (%s)\n",
					progname, argv[i], strerror(errno));
					exit(EXIT_FAILURE);
				}
			} else {
				if (!xbps_file_sha256(digest, sizeof digest, argv[i])) {
					fprintf(stderr,
				            "%s: couldn't get hash for %s (%s)\n",
					progname, argv[i], strerror(errno));
					exit(EXIT_FAILURE);
				}
			}
			printf("%s\n", digest);
		}
	}
	exit(EXIT_SUCCESS);
}
