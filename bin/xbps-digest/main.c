/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
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
		fprintf(stderr, "%s: unsupported digest mode\n", progname);
		exit(EXIT_FAILURE);
	}

	if (argc < 1) {
		if (!xbps_file_sha256(sha256, sizeof sha256, "/dev/stdin"))
			exit(EXIT_FAILURE);

		printf("%s\n", sha256);
	} else {
		for (int i = 0; i < argc; i++) {
			if (!xbps_file_sha256(sha256, sizeof sha256, argv[i])) {
				fprintf(stderr,
				    "%s: couldn't get hash for %s (%s)\n",
				progname, argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", sha256);
		}
	}
	exit(EXIT_SUCCESS);
}
