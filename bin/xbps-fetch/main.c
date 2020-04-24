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

#include <openssl/sha.h>

#include <xbps.h>
#include "../xbps-install/defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	"Usage: xbps-fetch [options] <url> <url+N>\n\n"
	"OPTIONS\n"
	" -d, --debug       Enable debug messages to stderr\n"
	" -h, --help        Show usage\n"
	" -o, --out <file>  Rename downloaded file to <file>\n"
	" -s, --sha256      Output sha256sums of the files\n"
	" -v, --verbose     Enable verbose output\n"
	" -V, --version     Show XBPS version\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
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

static void
print_digest(const uint8_t *digest, size_t len)
{
	while (len--) {
		if (*digest / 16 < 10)
			putc('0' + *digest / 16, stdout);
		else
			putc('a' + *digest / 16 - 10, stdout);
		if (*digest % 16 < 10)
			putc('0' + *digest % 16, stdout);
		else
			putc('a' + *digest % 16 - 10, stdout);
		++digest;
	}
}

int
main(int argc, char **argv)
{
	int flags = 0, c = 0, rv = 0;
	bool verbose = false;
	bool shasum = false;
	struct xbps_handle xh = {};
	struct xferstat xfer = {};
	const char *filename = NULL, *progname = argv[0];
	const struct option longopts[] = {
		{ "out", required_argument, NULL, 'o' },
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "sha256", no_argument, NULL, 's' },
		{ "version", no_argument, NULL, 'V' },
		{ "verbose", no_argument, NULL, 'v' },
		{ NULL, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "o:dhsVv", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'o':
			filename = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 's':
			shasum = true;
			break;
		case 'v':
			verbose = true;
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

	if (!argc) {
		usage(true);
		/* NOTREACHED */
	}

	/*
	* Initialize libxbps.
	*/
	xh.flags = flags;
	xh.fetch_cb = fetch_file_progress_cb;
	xh.fetch_cb_data = &xfer;
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("%s: failed to initialize libxbps: %s\n",
		    progname, strerror(rv));
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < argc; i++) {
		unsigned char digest[XBPS_SHA256_DIGEST_SIZE];

		if (i > 0 || !filename)
			filename = fname(argv[i]);

		if (shasum) {
			rv = xbps_fetch_file_dest_sha256(&xh, argv[i], filename, verbose ? "v" : "", digest, sizeof digest);
		} else {
			rv = xbps_fetch_file_dest(&xh, argv[i], filename, verbose ? "v" : "");
		}

		if (rv == -1) {
			fprintf(stderr, "%s: %s\n", argv[i], xbps_fetch_error_string());
		} else if (rv == 0) {
			fprintf(stderr, "%s: file is identical with remote.\n", argv[i]);
			if (shasum) {
				if (!xbps_file_sha256_raw(digest, sizeof digest, filename)) {
					xbps_error_printf("%s: failed to hash libxbps: %s: %s\n",
						progname, filename, strerror(rv));
					*digest = '\0';
				}
			}
		} else {
			rv = 0;
		}
		if (shasum) {
			print_digest(digest, SHA256_DIGEST_LENGTH);
			printf("  %s\n", filename);
		}
	}

	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
