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
usage(void)
{
	fprintf(stdout,
	"Usage: xbps-digest [options] [file] [file+N]\n"
	"\n"
	"OPTIONS:\n"
	" -h\t\tShow usage()\n"
	" -m <sha256>\tSelects the digest mode, sha256 (default)\n"
	" -V\t\tPrints the xbps release version\n"
	"\n"
	"NOTES\n"
	" If [file] not set, reads from stdin.\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	int c;
	char *hash = NULL;
	const char *mode = NULL, *progname = argv[0];
	const struct option longopts[] = {
		{ NULL, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "m:hV", longopts, NULL)) != -1) {
		switch (c) {
		case 'm':
			mode = optarg;
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

	if (mode && strcmp(mode, "sha256")) {
		/* sha256 is the only supported mode currently */
		fprintf(stderr, "%s: unsupported digest mode\n", progname);
		exit(EXIT_FAILURE);
	}

	if (argc < 1) {
		hash = xbps_file_hash("/dev/stdin");
		if (hash == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", hash);
		free(hash);
	} else {
		for (int i = 0; i < argc; i++) {
			hash = xbps_file_hash(argv[i]);
			if (hash == NULL) {
				fprintf(stderr,
				    "%s: couldn't get hash for %s (%s)\n",
				progname, argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", hash);
			free(hash);
		}
	}
	exit(EXIT_SUCCESS);
}
