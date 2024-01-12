/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <regex.h>

#include <xbps.h>


struct locate {
	const char* expr;
	bool case_ignore;
	regex_t regex;
};

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout, "Usage: xbps-locate [OPTIONS] file-pattern...\n"
		"\n"
		"OPTIONS\n"
		" -C, --config <dir>          Path to confdir (xbps.d)\n"
		" -c, --cachedir <dir>        Path to cachedir\n"
		" -d, --debug                 Debug mode shown to stderr\n"
		" -h, --help                  Show usage\n"
		" -e, --regex                 Use extended regular expression pattern\n"
		" -i, --ignore-conf-repos     Ignore repositories defined in xbps.d\n"
		" -I, --ignore-case           Match case insensitive\n"
		" -M, --memory-sync           Remote repository data is fetched and stored\n"
		"                             in memory, ignoring on-disk repodata archives\n"
		" -R, --repository <url>      Add repository to the top of the list\n"
		"                             This option can be specified multiple times\n"
		" -r, --rootdir <dir>         Full path to rootdir\n"
		" -V, --version               Show XBPS version\n"	
		" -v, --verbose               Verbose messages\n");

	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

#define LOWERWHEN(ignorecase, chr) (!(ignorecase) ? (chr) : tolower(chr))

static bool strcontains(const char* haystack, const char* needle, bool ignorecase) {
	const char *a, *b;
	
	b = needle;
    for ( ; *haystack != 0; haystack++) {
		if (*haystack != *b) {
			continue;
		}
		a = haystack;
		do {
			if (*b == 0) 
				return true;
		} while (LOWERWHEN(ignorecase, *a++) == LOWERWHEN(ignorecase, *b++));
		b = needle;
	}
    return false;
}

static int repo_search_files(struct xbps_repo* repo, void* locate_ptr, bool* done) {
	struct locate *locate = locate_ptr;
	const char *pkgname, *file;
	xbps_object_iterator_t iter;
	xbps_object_t pkgkey;
	xbps_array_t pkgfiles;

	(void) done;

	if (repo->files == NULL) {
		xbps_warn_printf("repository %s has no files-entry, skipping.\n", repo->uri);
		return 1;
	}

	iter = xbps_dictionary_iterator(repo->files);

	while ((pkgkey = xbps_object_iterator_next(iter)) != NULL) {
		pkgname  = xbps_dictionary_keysym_cstring_nocopy(pkgkey);
		pkgfiles = xbps_dictionary_get_keysym(repo->files, pkgkey);

		for (unsigned int i = 0; i < xbps_array_count(pkgfiles); i++) {
			if (!xbps_array_get_cstring_nocopy(pkgfiles, i, &file))
				continue;
			if (locate->expr != NULL) {
				if (strcontains(file, locate->expr, locate->case_ignore))
					printf("%s: %s\n", pkgname, file);
			} else {
				if (!regexec(&locate->regex, file, 0, NULL, 0)) 
					printf("%s: %s\n", pkgname, file);
			}
		}
	}

	xbps_object_iterator_release(iter);

	return 1;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "C:c:dehiIMr:R:r:vV";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "cachedir", required_argument, NULL, 'c' },
		{ "debug", no_argument, NULL, 'd' },
		{ "regex", no_argument, NULL, 'e' },
		{ "help", no_argument, NULL, 'h' },
		{ "ignore-conf-repos", no_argument, NULL, 'i' },
		{ "ignore-case", no_argument, NULL, 'I' },
		{ "memory-sync", no_argument, NULL, 'M' },
		{ "regex", no_argument, NULL, 'r' },
		{ "repository", required_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 },
	};

	struct xbps_handle xh;
	struct locate locate;
	const char *rootdir, *cachedir, *confdir;
	int c, flags, rv, regflags;
	bool regex;

	rootdir = cachedir = confdir  = NULL;
	flags = rv = c = 0;
	regex = false;
	regflags = REG_EXTENDED | REG_NOSUB;

	locate.expr = NULL;
	locate.case_ignore = false;

	memset(&xh, 0, sizeof(xh));

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'C':
			confdir = optarg;
			break;
		case 'c':
			cachedir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'i':
			flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
			break;
		case 'I':
			locate.case_ignore = true;
			regflags |= REG_ICASE;
			break;
		case 'M':
			flags |= XBPS_FLAG_REPOS_MEMSYNC;
			break;
		case 'R':
			xbps_repo_store(&xh, optarg);
			break;
		case 'r':
			rootdir = optarg;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'e':
			regex = true;
			break;
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
	
	if (regex) {
		if (regcomp(&locate.regex, *(argv++), regflags) != 0) {
			xbps_error_printf("invalid regular expression\n");
			exit(1);
		}
	} else {
		locate.expr = *(argv++);
	}
	argc--;

	if (argc) {
		/* trailing parameters */
		usage(true);
		/* NOTREACHED */
	}
	/*
	 * Initialize libxbps.
	 */
	if (rootdir)
		xbps_strlcpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	if (cachedir)
		xbps_strlcpy(xh.cachedir, cachedir, sizeof(xh.cachedir));
	if (confdir)
		xbps_strlcpy(xh.confdir, confdir, sizeof(xh.confdir));

	xh.flags = flags;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	xbps_rpool_foreach(&xh, repo_search_files, &locate);

	if (regex)
		regfree(&locate.regex);

	xbps_end(&xh);
	exit(rv);
}
