/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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
	    " -h --help                         Show help usage\n"
	    " -V --version                      Show XBPS version\n\n"
	    "MODE\n"
	    " -a --add <repodir/pkg> ...        Add package(s) to repository index\n"
	    " -c --clean <repodir>              Cleans obsolete entries in repository index\n"
	    " -r --remove-obsoletes <repodir>   Removes obsolete packages from repository\n\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

int
main(int argc, char **argv)
{
	const char *shortopts = "achrV";
	struct option longopts[] = {
		{ "add", no_argument, NULL, 'a' },
		{ "clean", no_argument, NULL, 'c' },
		{ "help", no_argument, NULL, 'h' },
		{ "remove-obsoletes", no_argument, NULL, 'r' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	int rv, c;
	bool clean_mode = false, add_mode = false, rm_mode = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'a':
			add_mode = true;
			break;
		case 'c':
			clean_mode = true;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'r':
			rm_mode = true;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		}
	}
	if ((argc == optind) || (!add_mode && !clean_mode && !rm_mode)) {
		usage(true);
	} else if ((add_mode && (clean_mode || rm_mode)) ||
		   (clean_mode && (add_mode || rm_mode)) ||
		   (rm_mode && (add_mode || clean_mode))) {
		fprintf(stderr, "Only one mode can be specified: add, clean "
		    "or remove-obsoletes.\n");
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
		rv = index_add(&xh, argc - optind, argv + optind);
	else if (clean_mode)
		rv = index_clean(&xh, argv[optind]);
	else if (rm_mode)
		rv = remove_obsoletes(&xh, argv[optind]);

	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
