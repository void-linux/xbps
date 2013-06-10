/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
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

#ifdef HAVE_STRCASESTR
# define _GNU_SOURCE    /* for strcasestr(3) */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

struct search_data {
	int npatterns;
	char **patterns;
	size_t maxcols;
	prop_array_t results;
};

static void
print_results(struct xbps_handle *xhp, struct search_data *sd)
{
	const char *pkgver, *desc, *inststr;
	char tmp[256], *out;
	size_t i, j, tlen = 0, len = 0;

	/* Iterate over results array and find out largest pkgver string */
	for (i = 0; i < prop_array_count(sd->results); i++) {
		prop_array_get_cstring_nocopy(sd->results, i, &pkgver);
		len = strlen(pkgver);
		if (tlen == 0 || len > tlen)
			tlen = len;
		i++;
	}
	for (i = 0; i < prop_array_count(sd->results); i++) {
		prop_array_get_cstring_nocopy(sd->results, i, &pkgver);
		prop_array_get_cstring_nocopy(sd->results, i+1, &desc);
		strncpy(tmp, pkgver, sizeof(tmp));
		for (j = strlen(tmp); j < tlen; j++)
			tmp[j] = ' ';

		tmp[j] = '\0';
		if (xbps_pkgdb_get_pkg(xhp, pkgver))
			inststr = "[*]";
		else
			inststr = "[-]";

		len = strlen(inststr) + strlen(tmp) + strlen(desc) + 3;
		if (len > sd->maxcols) {
			out = malloc(sd->maxcols+1);
			assert(out);
			snprintf(out, sd->maxcols-3, "%s %s %s",
			    inststr, tmp, desc);
			strncat(out, "...\n", sd->maxcols);
			printf("%s", out);
			free(out);
		} else {
			printf("%s %s %s\n", inststr, tmp, desc);
		}
		i++;
	}
}

static int
search_pkgs_cb(struct xbps_repo *repo, void *arg, bool *done)
{
	prop_array_t allkeys;
	prop_dictionary_t pkgd;
	prop_dictionary_keysym_t ksym;
	struct search_data *sd = arg;
	const char *pkgver, *desc;
	size_t i;
	int x;

	(void)done;

	allkeys = prop_dictionary_all_keys(repo->idx);
	for (i = 0; i < prop_array_count(allkeys); i++) {
		prop_array_t provides = NULL;

		ksym = prop_array_get(allkeys, i);
		pkgd = prop_dictionary_get_keysym(repo->idx, ksym);

		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(pkgd, "short_desc", &desc);
		provides = prop_dictionary_get(pkgd, "provides");

		for (x = 0; x < sd->npatterns; x++) {
			size_t j;
			bool vpkgfound = false;

			for (j = 0; j < prop_array_count(provides); j++) {
				const char *vpkgver;
				char *tmp, *vpkgname;

				prop_array_get_cstring_nocopy(provides, j, &vpkgver);
				if (strchr(vpkgver, '_') == NULL)
					tmp = xbps_xasprintf("%s_1", vpkgver);
				else
					tmp = strdup(vpkgver);

				vpkgname = xbps_pkg_name(tmp);
				if (strcasecmp(vpkgname, sd->patterns[x]) == 0) {
					free(vpkgname);
					free(tmp);
					vpkgfound = true;
					break;
				}
				free(vpkgname);
				free(tmp);
			}
			if ((xbps_pkgpattern_match(pkgver, sd->patterns[x])) ||
			    (strcasestr(pkgver, sd->patterns[x])) ||
			    (strcasestr(desc, sd->patterns[x])) || vpkgfound) {
				prop_array_add_cstring_nocopy(sd->results, pkgver);
				prop_array_add_cstring_nocopy(sd->results, desc);
			}
		}
	}
	prop_object_release(allkeys);

	return 0;
}

int
repo_search(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct search_data sd;
	int rv;

	sd.npatterns = npatterns;
	sd.patterns = patterns;
	sd.maxcols = get_maxcols();
	sd.results = prop_array_create();

	rv = xbps_rpool_foreach(xhp, search_pkgs_cb, &sd);
	if (rv != 0 && rv != ENOTSUP)
		fprintf(stderr, "Failed to initialize rpool: %s\n",
		    strerror(rv));

	print_results(xhp, &sd);

	return rv;
}
