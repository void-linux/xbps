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
#include <libintl.h>
#include <locale.h>

#define _(x) gettext(x)

#include <openssl/sha.h>

#include <xbps.h>
#include "../xbps-install/defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,_(
	"Usage: xbps-fetch [options] <url> <url+N>\n\n"
	"OPTIONS\n"
	" -d, --debug       Enable debug messages to stderr\n"
	" -h, --help        Show usage\n"
	" -o, --out <file>  Rename downloaded file to <file>\n"
	" -s, --sha256      Output sha256sums of the files\n"
	" -v, --verbose     Enable verbose output\n"
	" -V, --version     Show XBPS version\n"));
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

	setlocale(LC_MESSAGES, "");
	bindtextdomain("xbps", LOCALEDIR);
	textdomain("xbps");

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
