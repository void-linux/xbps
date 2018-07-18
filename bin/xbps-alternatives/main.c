/*-
 * Copyright (c) 2015 Juan Romero Pardines.
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
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <syslog.h>

#include <xbps.h>

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-alternatives [OPTIONS] MODE\n\n"
	    "OPTIONS\n"
	    " -C --config <dir>   Path to confdir (xbps.d)\n"
	    " -d --debug          Debug mode shown to stderr\n"
	    " -g --group <name>   Group of alternatives to match\n"
	    " -h --help           Print usage help\n"
	    " -r --rootdir <dir>  Full path to rootdir\n"
	    " -v --verbose        Verbose messages\n"
	    " -V --version        Show XBPS version\n"
	    "MODE\n"
	    " -l --list [PKG]     List all alternatives or from PKG\n"
	    " -s --set PKG        Set alternatives for PKG\n\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
state_cb(const struct xbps_state_cb_data *xscd, void *cbd UNUSED)
{
	bool slog = false;

	if ((xscd->xhp->flags & XBPS_FLAG_DISABLE_SYSLOG) == 0) {
		slog = true;
		openlog("xbps-alternatives", 0, LOG_USER);
	}
	if (xscd->desc) {
		printf("%s\n", xscd->desc);
		if (slog)
			syslog(LOG_NOTICE, "%s", xscd->desc);
	}
	return 0;
}

static void
list_pkg_alternatives(xbps_dictionary_t pkgd, const char *group, bool print_key)
{
	xbps_dictionary_t pkg_alternatives;
	xbps_array_t allkeys;

	pkg_alternatives = xbps_dictionary_get(pkgd, "alternatives");
	if (pkg_alternatives == NULL)
		return;

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_object_t keysym;
		xbps_array_t array;
		const char *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		array = xbps_dictionary_get_keysym(pkg_alternatives, keysym);

		if (group && strcmp(group, keyname))
			continue;

		if (print_key)
			printf("%s\n", keyname);

		for (unsigned int x = 0; x < xbps_array_count(array); x++) {
			const char *str;

			xbps_array_get_cstring_nocopy(array, x, &str);
			printf("  - %s\n", str);
		}
	}
	xbps_object_release(allkeys);
}

static int
list_alternatives(struct xbps_handle *xhp, const char *pkgname, const char *grp)
{
	xbps_dictionary_t alternatives, pkgd;
	xbps_array_t allkeys;

	(void)xbps_pkgdb_get_pkg(xhp, "foo");

	if (pkgname) {
		/* list alternatives for pkgname */
		if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL)
			return ENOENT;

		list_pkg_alternatives(pkgd, NULL, true);
		return 0;
	}
	assert(xhp->pkgdb);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL)
		return ENOENT;

	allkeys = xbps_dictionary_all_keys(alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		const char *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		array = xbps_dictionary_get_keysym(alternatives, keysym);

		if (grp && strcmp(grp, keyname))
			continue;

		printf("%s\n", keyname);
		for (unsigned int x = 0; x < xbps_array_count(array); x++) {
			const char *str;

			xbps_array_get_cstring_nocopy(array, x, &str);
			printf(" - %s%s\n", str, x == 0 ? " (current)" : "");
			pkgd = xbps_pkgdb_get_pkg(xhp, str);
			assert(pkgd);
			list_pkg_alternatives(pkgd, keyname, false);
		}
	}
	xbps_object_release(allkeys);

	return 0;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "C:dg:hls:r:Vv";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "debug", no_argument, NULL, 'd' },
		{ "group", required_argument, NULL, 'g' },
		{ "help", no_argument, NULL, 'h' },
		{ "list", no_argument, NULL, 'l' },
		{ "set", required_argument, NULL, 's' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	const char *confdir, *rootdir, *group, *pkg;
	int c, rv, flags = 0;
	bool list_mode = false, set_mode = false;

	confdir = rootdir = group = pkg = NULL;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'C':
			confdir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'g':
			group = optarg;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'l':
			list_mode = true;
			break;
		case 's':
			set_mode = true;
			pkg = optarg;
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
		case '?':
		default:
			usage(true);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (!list_mode && !set_mode)
		usage(true);
	else if (argc && list_mode)
		pkg = *argv;

	memset(&xh, 0, sizeof(xh));
	xh.state_cb = state_cb;
	if (rootdir)
		xbps_strlcpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	if (confdir)
		xbps_strlcpy(xh.confdir, confdir, sizeof(xh.confdir));

	xh.flags = flags;

	/* initialize xbps */
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}
	if (set_mode) {
		/* in set mode pkgdb must be locked and flushed on success */
		if ((rv = xbps_pkgdb_lock(&xh)) != 0) {
			fprintf(stderr, "failed to lock pkgdb: %s\n", strerror(rv));
			exit(EXIT_FAILURE);
		}
		if ((rv = xbps_alternatives_set(&xh, pkg, group)) == 0)
			rv = xbps_pkgdb_update(&xh, true, true);
	} else if (list_mode) {
		/* list alternative groups */
		rv = list_alternatives(&xh, pkg, group);
	}

	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
