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

#ifdef HAVE_STRCASESTR
# define _GNU_SOURCE    /* for strcasestr(3) */
#endif

#include "compat.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>
#include <assert.h>
#include <regex.h>

#include <xbps.h>
#include "defs.h"

struct search_data {
	bool regex, repo_mode;
	regex_t regexp;
	int maxcols;
	const char *pat, *prop, *repourl;
	xbps_array_t results;
};

static void
print_results(struct xbps_handle *xhp, struct search_data *sd)
{
	const char *pkgver, *desc, *inststr;
	char tmp[256], *out;
	unsigned int j, tlen = 0, len = 0;

	/* Iterate over results array and find out largest pkgver string */
	for (unsigned int i = 0; i < xbps_array_count(sd->results); i+=2) {
		xbps_array_get_cstring_nocopy(sd->results, i, &pkgver);
		len = strlen(pkgver);
		if (tlen == 0 || len > tlen)
			tlen = len;
	}
	for (unsigned int i = 0; i < xbps_array_count(sd->results); i+=2) {
		xbps_array_get_cstring_nocopy(sd->results, i, &pkgver);
		xbps_array_get_cstring_nocopy(sd->results, i+1, &desc);
		xbps_strlcpy(tmp, pkgver, sizeof(tmp));
		for (j = strlen(tmp); j < tlen; j++)
			tmp[j] = ' ';

		tmp[j] = '\0';
		if (xbps_pkgdb_get_pkg(xhp, pkgver))
			inststr = "[*]";
		else
			inststr = "[-]";

		len = strlen(inststr) + strlen(tmp) + strlen(desc) + 3;
		if (sd->maxcols && (int)len > sd->maxcols) {
			out = malloc(sd->maxcols+1);
			assert(out);
			snprintf(out, sd->maxcols-3, "%s %s %s",
			    inststr, tmp, desc);
			xbps_strlcat(out, "...\n", sd->maxcols+1);
			printf("%s", out);
			free(out);
		} else {
			printf("%s %s %s\n", inststr, tmp, desc);
		}
	}
}

static int
search_array_cb(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	xbps_object_t obj2;
	struct search_data *sd = arg;
	const char *pkgver = NULL, *desc, *str;

	if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver))
		return 0;

	if (sd->prop == NULL) {
		bool vpkgfound = false;
		/* no prop set, match on pkgver/short_desc objects */
		xbps_dictionary_get_cstring_nocopy(obj, "short_desc", &desc);

		if (sd->repo_mode && xbps_match_virtual_pkg_in_dict(obj, sd->pat))
			vpkgfound = true;

		if (sd->regex) {
			if ((regexec(&sd->regexp, pkgver, 0, 0, 0) == 0) ||
			    (regexec(&sd->regexp, desc, 0, 0, 0) == 0)) {
				xbps_array_add_cstring_nocopy(sd->results, pkgver);
				xbps_array_add_cstring_nocopy(sd->results, desc);
			}
			return 0;
		}
		if (vpkgfound) {
			xbps_array_add_cstring_nocopy(sd->results, pkgver);
			xbps_array_add_cstring_nocopy(sd->results, desc);
		} else {
			if ((strcasestr(pkgver, sd->pat)) ||
			    (strcasestr(desc, sd->pat)) ||
			    (xbps_pkgpattern_match(pkgver, sd->pat))) {
				xbps_array_add_cstring_nocopy(sd->results, pkgver);
				xbps_array_add_cstring_nocopy(sd->results, desc);
			}
		}
		return 0;
	}
	/* prop set, match on prop object instead */
	obj2 = xbps_dictionary_get(obj, sd->prop);
	if (xbps_object_type(obj2) == XBPS_TYPE_ARRAY) {
		/* property is an array */
		for (unsigned int i = 0; i < xbps_array_count(obj2); i++) {
			xbps_array_get_cstring_nocopy(obj2, i, &str);
			if (sd->regex) {
				if (regexec(&sd->regexp, str, 0, 0, 0) == 0) {
					if (sd->repo_mode)
						printf("%s: %s (%s)\n", pkgver, str, sd->repourl);
					else
						printf("%s: %s\n", pkgver, str);
				}
			} else {
				if (strcasestr(str, sd->pat)) {
					if (sd->repo_mode)
						printf("%s: %s (%s)\n", pkgver, str, sd->repourl);
					else
						printf("%s: %s\n", pkgver, str);
				}
			}
		}
	} else if (xbps_object_type(obj2) == XBPS_TYPE_NUMBER) {
		/* property is a number */
		char size[8];

		if (xbps_humanize_number(size, xbps_number_integer_value(obj2)) == -1)
			exit(EXIT_FAILURE);

		if (sd->regex) {
			if (regexec(&sd->regexp, size, 0, 0, 0) == 0) {
				if (sd->repo_mode)
					printf("%s: %s (%s)\n", pkgver, size, sd->repourl);
				else
					printf("%s: %s\n", pkgver, size);
			}
		} else {
			if (strcasestr(size, sd->pat)) {
				if (sd->repo_mode)
					printf("%s: %s (%s)\n", pkgver, size, sd->repourl);
				else
					printf("%s: %s\n", pkgver, size);
			}
		}
	} else if (xbps_object_type(obj2) == XBPS_TYPE_BOOL) {
		/* property is a bool */
		if (sd->repo_mode)
			printf("%s: true (%s)\n", pkgver, sd->repourl);
		else
			printf("%s: true\n", pkgver);

	} else if (xbps_object_type(obj2) == XBPS_TYPE_STRING) {
		/* property is a string */
		str = xbps_string_cstring_nocopy(obj2);
		if (sd->regex) {
			if (regexec(&sd->regexp, str, 0, 0, 0) == 0) {
				if (sd->repo_mode)
					printf("%s: %s (%s)\n", pkgver, str, sd->repourl);
				else
					printf("%s: %s\n", pkgver, str);
			}
		} else {
			if (strcasestr(str, sd->pat)) {
				if (sd->repo_mode)
					printf("%s: %s (%s)\n", pkgver, str, sd->repourl);
				else
					printf("%s: %s\n", pkgver, str);
			}
		}
	}
	return 0;
}

static int
search_repo_cb(struct xbps_repo *repo, void *arg, bool *done UNUSED)
{
	xbps_array_t allkeys;
	struct search_data *sd = arg;
	int rv;

	if (repo->idx == NULL)
		return 0;

	sd->repourl = repo->uri;
	allkeys = xbps_dictionary_all_keys(repo->idx);
	rv = xbps_array_foreach_cb(repo->xhp, allkeys, repo->idx, search_array_cb, sd);
	xbps_object_release(allkeys);
	return rv;
}

int
search(struct xbps_handle *xhp, bool repo_mode, const char *pat, const char *prop, bool regex)
{
	struct search_data sd;
	int rv;

	sd.regex = regex;
	if (regex) {
		if (regcomp(&sd.regexp, pat, REG_EXTENDED|REG_NOSUB|REG_ICASE) != 0)
			return errno;
	}
	sd.repo_mode = repo_mode;
	sd.pat = pat;
	sd.prop = prop;
	sd.maxcols = get_maxcols();
	sd.results = xbps_array_create();

	if (repo_mode) {
		rv = xbps_rpool_foreach(xhp, search_repo_cb, &sd);
		if (rv != 0 && rv != ENOTSUP) {
			fprintf(stderr, "Failed to initialize rpool: %s\n",
			    strerror(rv));
			return rv;
		}
	} else {
		rv = xbps_pkgdb_foreach_cb(xhp, search_array_cb, &sd);
		if (rv != 0) {
			fprintf(stderr, "Failed to initialize pkgdb: %s\n",
			    strerror(rv));
			return rv;
		}
	}
	if (!prop && xbps_array_count(sd.results)) {
		print_results(xhp, &sd);
		xbps_object_release(sd.results);
	}
	if (regex)
		regfree(&sd.regexp);

	return rv;
}
