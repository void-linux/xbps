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
#include <regex.h>

#include <xbps.h>
#include "defs.h"

struct search_data {
	bool regex;
	regex_t regexp;
	unsigned int maxcols;
	const char *pat, *prop, *repourl;
	xbps_array_t results;
	char *linebuf;
	enum search_mode search_mode;
};

static void
print_results(struct xbps_handle *xhp, struct search_data *sd)
{
	const char *pkgver = NULL, *desc = NULL;
	unsigned int align = 0, len;

	/* Iterate over results array and find out largest pkgver string */
	for (unsigned int i = 0; i < xbps_array_count(sd->results); i += 2) {
		xbps_array_get_cstring_nocopy(sd->results, i, &pkgver);
		if ((len = strlen(pkgver)) > align)
			align = len;
	}
	for (unsigned int i = 0; i < xbps_array_count(sd->results); i += 2) {
		xbps_array_get_cstring_nocopy(sd->results, i, &pkgver);
		xbps_array_get_cstring_nocopy(sd->results, i+1, &desc);

		if (sd->linebuf == NULL) {
			printf("[%s] %-*s %s\n",
			    xbps_pkgdb_get_pkg(xhp, pkgver) ? "*" : "-",
			    align, pkgver, desc);
			continue;
		}

		len = snprintf(sd->linebuf, sd->maxcols, "[%s] %-*s %s",
		    xbps_pkgdb_get_pkg(xhp, pkgver) ? "*" : "-",
		    align, pkgver, desc);
		/* add ellipsis if the line was truncated */
		if (len >= sd->maxcols && sd->maxcols > 4) {
			for (unsigned int j = 0; j < 3; j++)
				sd->linebuf[sd->maxcols-j-1] = '.';
			sd->linebuf[sd->maxcols] = '\0';
		}
		puts(sd->linebuf);
	}
}

static void
print_prop_search_result(struct search_data *sd, const char *pkgver, const char *str) {
	if (sd->search_mode == IN_REPO)
		printf("%s: %s (%s)\n", pkgver, str, sd->repourl);
	else
		printf("%s: %s\n", pkgver, str);
}

static void
handle_prop_matching(struct search_data *sd, const char *pkgver, const char *str) {
	if (sd->regex) {
		if (regexec(&sd->regexp, str, 0, 0, 0) == 0) {
			print_prop_search_result(sd, pkgver, str);
		}
	} else {
		if (strcasestr(str, sd->pat)) {
			print_prop_search_result(sd, pkgver, str);
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
	xbps_object_t prop;
	struct search_data *sd = arg;
	const char *pkgver = NULL, *desc = NULL, *str = NULL;
	bool automatic = false;

	if (sd->search_mode == IN_MANUAL) {
		xbps_dictionary_get_bool(obj, "automatic-install", &automatic);
		if (automatic)
			return 0;
	}

	if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver))
		return 0;

	if (sd->prop == NULL) {
		bool vpkgfound = false;
		/* no prop set, match on pkgver/short_desc objects */
		xbps_dictionary_get_cstring_nocopy(obj, "short_desc", &desc);

		if (sd->regex) {
			if ((regexec(&sd->regexp, pkgver, 0, 0, 0) == 0) ||
			    (regexec(&sd->regexp, desc, 0, 0, 0) == 0))
				goto add_and_out;

			return 0;
		}

		vpkgfound = sd->search_mode == IN_REPO && xbps_match_virtual_pkg_in_dict(obj, sd->pat);
		if (vpkgfound)
			goto add_and_out;

		if (strcasestr(pkgver, sd->pat) ||
		    strcasestr(desc, sd->pat)   ||
		    xbps_pkgpattern_match(pkgver, sd->pat))
			goto add_and_out;

		return 0;
	add_and_out:
		xbps_array_add_cstring_nocopy(sd->results, pkgver);
		xbps_array_add_cstring_nocopy(sd->results, desc);
		return 0;
	}

	/* prop set, match on prop object instead */
	prop = xbps_dictionary_get(obj, sd->prop);
	if (prop == NULL)
		return 0;

	switch(xbps_object_type(prop)) {
	case XBPS_TYPE_ARRAY: {
		for (unsigned int i = 0; i < xbps_array_count(prop); i++) {
			xbps_array_get_cstring_nocopy(prop, i, &str);
			handle_prop_matching(sd, pkgver, str);
		}
	} break;
	case XBPS_TYPE_STRING: {
		str = xbps_string_cstring_nocopy(prop);
		handle_prop_matching(sd, pkgver, str);
	} break;
	case XBPS_TYPE_NUMBER: {
		char size[8];

		if (xbps_humanize_number(size, xbps_number_integer_value(prop)) == -1)
			exit(EXIT_FAILURE);

		handle_prop_matching(sd, pkgver, size);
	} break;
	case XBPS_TYPE_BOOL: {
		print_prop_search_result(sd, pkgver, "true");
	} break;
	case XBPS_TYPE_DATA:
	case XBPS_TYPE_DICTIONARY:
	case XBPS_TYPE_DICT_KEYSYM:
	case XBPS_TYPE_UNKNOWN:
		fprintf(stderr, "unsupported property type found in pkg: %s\n", pkgver);
		exit(EXIT_FAILURE);
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
search(struct xbps_handle *xhp,
	bool regex,
	const char *pat,
	const char *prop,
	enum search_mode sm)
{
	struct search_data sd;
	int rv;

	sd.regex = regex;
	if (regex) {
		if (regcomp(&sd.regexp, pat, REG_EXTENDED|REG_NOSUB|REG_ICASE) != 0)
			return errno;
	}

	sd.pat = pat;
	sd.prop = prop;
	sd.search_mode = sm;
	sd.maxcols = get_maxcols();
	sd.linebuf = NULL;

	if (sd.maxcols > 0) {
		sd.linebuf = malloc(sd.maxcols);
		if (sd.linebuf == NULL)
			exit(1);
	}

	sd.results = xbps_array_create();

	if (sd.search_mode == IN_REPO) {
		rv = xbps_rpool_foreach(xhp, search_repo_cb, &sd);
		if (rv != 0 && rv != ENOTSUP) {
			fprintf(stderr, "Failed to initialize rpool: %s\n",
				strerror(rv));
			goto out;
		}
	} else {
		rv = xbps_pkgdb_foreach_cb(xhp, search_array_cb, &sd);
		if (rv != 0) {
			fprintf(stderr, "Failed to initialize pkgdb: %s\n",
				strerror(rv));
			goto out;
		}
	}

	if (!prop && xbps_array_count(sd.results))
		print_results(xhp, &sd);

out:
	if (regex)
		regfree(&sd.regexp);

	if (sd.maxcols)
		free(sd.linebuf);

	xbps_object_release(sd.results);

	return rv;
}
