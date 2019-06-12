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
#include "../xbps-install/defs.h"

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"Usage: xbps-fetch [options] <url> <url+N>\n\n"
	"OPTIONS:\n"
	" -d\t\tEnable debug messages to stderr\n"
	" -h\t\tShow usage()\n"
	" -o <file>\tRename downloaded file to <file>\n"
	" -v\t\tEnable verbose output\n"
	" -V\t\tPrints the xbps release version\n");
	exit(EXIT_FAILURE);
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

int
main(int argc, char **argv)
{
	int flags = 0, c = 0, rv = 0;
	bool verbose = false;
	struct xbps_handle xh = {};
	struct xferstat xfer = {};
	const char *filename = NULL, *progname = argv[0];
	const struct option longopts[] = {
		{ NULL, 0, NULL, 0 }
	};

	while ((c = getopt_long(argc, argv, "o:dhVv", longopts, NULL)) != -1) {
		switch (c) {
		case 'o':
			filename = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'v':
			verbose = true;
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

	if (!argc)
		usage();

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
		if ((argc == 1) && !filename)
			filename = fname(argv[i]);

		rv = xbps_fetch_file_dest(&xh, argv[i], filename, verbose ? "v" : "");

		if (rv == -1) {
			fprintf(stderr, "%s: %s\n", argv[i], xbps_fetch_error_string());
		} else if (rv == 0) {
			printf("%s: file is identical with remote.\n", argv[i]);
		} else {
			rv = 0;
		}
	}

	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
