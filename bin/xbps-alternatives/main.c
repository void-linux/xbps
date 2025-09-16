/*-
 * Copyright (c) 2015 Juan Romero Pardines.
 * Copyright (c) 2020 Duncan Overbruck <mail@duncano.de>.
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
	    " -C --config <dir>        Path to confdir (xbps.d)\n"
	    " -d --debug               Debug mode shown to stderr\n"
	    " -g --group <name>        Group of alternatives to match\n"
	    " -h --help                Show usage\n"
	    " -i, --ignore-conf-repos  Ignore repositories defined in xbps.d\n"
	    " -R, --repository         Enable repository mode. This mode explicitly\n"
	    "                          looks for packages in repositories\n"
	    "     --repository=<url>   Enable repository mode and add repository\n"
	    "                          to the top of the list. This option can be\n"
	    "                          specified multiple times\n"
	    " -r --rootdir <dir>       Full path to rootdir\n"
	    " -v --verbose             Verbose messages\n"
	    " -V --version             Show XBPS version\n"
	    "MODE\n"
	    " -l --list [PKG]          List all alternatives or from PKG\n"
	    " -s --set PKG             Set alternatives for PKG\n");
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
			const char *str = NULL;

			xbps_array_get_cstring_nocopy(array, x, &str);
			printf("  - %s\n", str);
		}
	}
	xbps_object_release(allkeys);
}

static void
print_alternatives(struct xbps_handle *xhp, xbps_dictionary_t alternatives, const char *grp, bool repo_mode)
{
	xbps_array_t allkeys;
	xbps_dictionary_t pkgd;

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
			const char *str = NULL;

			xbps_array_get_cstring_nocopy(array, x, &str);
			printf(" - %s%s\n", str, !repo_mode && x == 0 ? " (current)" : "");
			pkgd = xbps_pkgdb_get_pkg(xhp, str);
			if (pkgd == NULL && repo_mode)
				pkgd = xbps_rpool_get_pkg(xhp, str);
			assert(pkgd);
			list_pkg_alternatives(pkgd, keyname, false);
		}
	}
	xbps_object_release(allkeys);
}

static int
list_alternatives(struct xbps_handle *xhp, const char *pkgname, const char *grp)
{
	xbps_dictionary_t alternatives, pkgd;

	if (pkgname) {
		/* list alternatives for pkgname */
		if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL)
			return -ENOENT;

		list_pkg_alternatives(pkgd, NULL, true);
		return 0;
	} else {
		// XXX: initializing the pkgdb.
		(void)xbps_pkgdb_get_pkg(xhp, "foo");
	}
	assert(xhp->pkgdb);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL)
		return -ENOENT;

	print_alternatives(xhp, alternatives, grp, false);
	return 0;
}

struct search_data {
	const char *group;
	xbps_dictionary_t result;
};

static int
search_array_cb(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	xbps_object_iterator_t iter;
	xbps_dictionary_t alternatives;
	struct search_data *sd = arg;
	const char *pkgver = NULL;

	if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver))
		return 0;

	alternatives = xbps_dictionary_get(obj, "alternatives");
	if (alternatives == NULL)
		return 0;

	iter = xbps_dictionary_iterator(alternatives);
	assert(iter);

	/*
	 * Register all provided groups in the result dictionary.
	 */
	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t grouparr;
		const char *group = xbps_dictionary_keysym_cstring_nocopy(obj);
		bool alloc = false;

		/* skip the group if we search for a specific one */
		if (sd->group != NULL && strcmp(sd->group, group) != 0)
			continue;

		grouparr = xbps_dictionary_get(sd->result, group);
		if (grouparr == NULL) {
			if ((grouparr = xbps_array_create()) == NULL) {
				xbps_error_printf("Failed to create array: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			alloc = true;
			xbps_dictionary_set(sd->result, group, grouparr);
		} else {
			/*
			 * check if pkgver is already in the group array,
			 * this only happens if multiple repositories provide
			 * the same pkgver.
			 */
			if (xbps_match_string_in_array(grouparr, pkgver))
				continue;
		}
		xbps_array_add_cstring_nocopy(grouparr, pkgver);

		if (alloc)
			xbps_object_release(grouparr);
	}

	return 0;
}

static int
search_repo_cb(struct xbps_repo *repo, void *arg, bool *done UNUSED)
{
	xbps_array_t allkeys;
	int rv;

	if (repo->idx == NULL)
		return 0;

	allkeys = xbps_dictionary_all_keys(repo->idx);
	rv = xbps_array_foreach_cb(repo->xhp, allkeys, repo->idx, search_array_cb, arg);
	xbps_object_release(allkeys);
	return rv;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "C:dg:hils:Rr:Vv";
	const struct option longopts[] = {
		{ "config", required_argument, NULL, 'C' },
		{ "debug", no_argument, NULL, 'd' },
		{ "group", required_argument, NULL, 'g' },
		{ "help", no_argument, NULL, 'h' },
		{ "ignore-conf-repos", no_argument, NULL, 'i' },
		{ "list", no_argument, NULL, 'l' },
		{ "set", required_argument, NULL, 's' },
		{ "repository", optional_argument, NULL, 'R' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh = { 0 };
	const char *group, *pkg;
	int c, rv;
	bool list_mode = false, set_mode = false, repo_mode = false;

	group = pkg = NULL;

	xh.state_cb = state_cb;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'C':
			xbps_strlcpy(xh.confdir, optarg, sizeof(xh.confdir));
			break;
		case 'd':
			xh.flags |= XBPS_FLAG_DEBUG;
			break;
		case 'g':
			group = optarg;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'i':
			xh.flags |= XBPS_FLAG_IGNORE_CONF_REPOS;
			break;
		case 'l':
			list_mode = true;
			break;
		case 's':
			set_mode = true;
			pkg = optarg;
			break;
		case 'R':
			if (optarg != NULL) {
				xbps_repo_store(&xh, optarg);
			}
			repo_mode = true;
			break;
		case 'r':
			xbps_strlcpy(xh.rootdir, optarg, sizeof(xh.rootdir));
			break;
		case 'v':
			xh.flags |= XBPS_FLAG_VERBOSE;
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
	else if (argc > 0 && list_mode) {
		pkg = *argv++;
		argc -= 1;
		if (argc > 0)
			usage(true);
	}

	/* initialize xbps */
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}
	if (set_mode) {
		// XXX: xbps_pkgdb_init
		(void)xbps_pkgdb_get_pkg(&xh, "foo");

		/* in set mode pkgdb must be locked and flushed on success */
		if (xbps_pkgdb_lock(&xh) < 0) {
			xbps_end(&xh);
			exit(EXIT_FAILURE);
		}
		if ((rv = xbps_alternatives_set(&xh, pkg, group)) == 0)
			rv = xbps_pkgdb_update(&xh, true, false);
		else
			xbps_error_printf("failed to update alternatives group: %s\n", strerror(rv));
	} else if (list_mode) {
		/* list alternative groups */
		if (repo_mode) {
			struct search_data sd = { 0 };
			if ((sd.result = xbps_dictionary_create()) == NULL) {
				xbps_error_printf("Failed to create dictionary: %s\n", strerror(errno));
				xbps_end(&xh);
				exit(EXIT_FAILURE);
			}
			sd.group = group;
			rv = xbps_rpool_foreach(&xh, search_repo_cb, &sd);
			if (rv != 0 && rv != ENOTSUP) {
				fprintf(stderr, "Failed to initialize rpool: %s\n",
				    strerror(rv));
				xbps_end(&xh);
				exit(EXIT_FAILURE);
			}
			if (xbps_dictionary_count(sd.result) > 0) {
				print_alternatives(&xh, sd.result, group, true);
			} else {
				xbps_error_printf("no alternatives groups found\n");
				xbps_end(&xh);
				exit(EXIT_FAILURE);
			}
		} else {
			rv = list_alternatives(&xh, pkg, group);
			if (rv == ENOENT) {
				xbps_error_printf("no alternatives groups found");
				fprintf(stderr, pkg ? " for package %s\n" : "\n", pkg);
			}
		}
	}

	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
