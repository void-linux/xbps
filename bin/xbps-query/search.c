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

struct search_ctx {
	bool regex, repo_mode;
	regex_t regexp;
	unsigned int maxcols;
	const char *pattern;
	const char *repourl;
	xbps_array_t results;
	char *linebuf;
};

static void
print_results(struct xbps_handle *xhp, struct search_ctx *sd)
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
			sd->linebuf[sd->maxcols - 4] = '.';
			sd->linebuf[sd->maxcols - 3] = '.';
			sd->linebuf[sd->maxcols - 2] = '.';
		}
		puts(sd->linebuf);
	}
}

static int
search_cb(struct xbps_handle *xhp UNUSED, xbps_object_t pkgd,
    const char *key UNUSED, void *arg, bool *done UNUSED)
{
	struct search_ctx *ctx = arg;
	bool vpkgfound = false;
	const char *pkgver = NULL, *desc = NULL;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
		abort();

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "short_desc", &desc)) {
		xbps_error_printf("%s: missing short_desc property\n", pkgver);
		return -EINVAL;
	}

	if (ctx->repo_mode && xbps_match_virtual_pkg_in_dict(pkgd, ctx->pattern))
		vpkgfound = true;

	if (ctx->regex) {
		if ((regexec(&ctx->regexp, pkgver, 0, 0, 0) == 0) ||
		    (regexec(&ctx->regexp, desc, 0, 0, 0) == 0)) {
			if (!xbps_array_add_cstring_nocopy(ctx->results, pkgver))
				return xbps_error_oom();
			if (!xbps_array_add_cstring_nocopy(ctx->results, desc))
				return xbps_error_oom();
		}
		return 0;
	}
	if (vpkgfound) {
		if (!xbps_array_add_cstring_nocopy(ctx->results, pkgver))
			return xbps_error_oom();
		if (!xbps_array_add_cstring_nocopy(ctx->results, desc))
			return xbps_error_oom();
	} else {
		if ((strcasestr(pkgver, ctx->pattern)) ||
		    (strcasestr(desc, ctx->pattern)) ||
		    (xbps_pkgpattern_match(pkgver, ctx->pattern))) {
			if (!xbps_array_add_cstring_nocopy(ctx->results, pkgver))
				return xbps_error_oom();
			if (!xbps_array_add_cstring_nocopy(ctx->results, desc))
				return xbps_error_oom();
		}
	}
	return 0;
}

static int
search_repo_cb(struct xbps_repo *repo, void *arg, bool *done UNUSED)
{
	xbps_array_t keys;
	struct search_ctx *ctx = arg;
	int rv;

	keys = xbps_dictionary_all_keys(repo->idx);
	if (!keys)
		return xbps_error_oom();

	ctx->repourl = repo->uri;
	rv = xbps_array_foreach_cb(repo->xhp, keys, repo->idx, search_cb, ctx);

	xbps_object_release(keys);
	return rv;
}

int
search(struct xbps_handle *xhp, bool repo_mode, const char *pattern, bool regex)
{
	struct search_ctx ctx = {
		.repo_mode = repo_mode,
		.regex = regex,
		.pattern = pattern,
		.maxcols = get_maxcols(),
	};
	int r;

	if (regex) {
		r = regcomp(
		    &ctx.regexp, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE);
		if (r != 0) {
			char errbuf[4096];
			regerror(r, &ctx.regexp, errbuf, sizeof(errbuf));
			xbps_error_printf("failed to compile regexp: %s: %s\n",
			    pattern, errbuf);
			return EXIT_FAILURE;
		}
	}

	ctx.results = xbps_array_create();
	if (!ctx.results) {
		r = xbps_error_oom();
		goto err;
	}

	if (ctx.maxcols > 0) {
		ctx.linebuf = malloc(ctx.maxcols);
		if (!ctx.linebuf) {
			r = xbps_error_oom();
			goto err;
		}
	}

	if (repo_mode)
		r = xbps_rpool_foreach(xhp, search_repo_cb, &ctx);
	else
		r = xbps_pkgdb_foreach_cb(xhp, search_cb, &ctx);
	if (r != 0)
		goto err;

	print_results(xhp, &ctx);

err:
	if (ctx.results)
		xbps_object_release(ctx.results);
	if (regex)
		regfree(&ctx.regexp);
	free(ctx.linebuf);
	return r == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

struct search_prop_ctx {
	bool regex, repo_mode;
	const char *pattern;
	const char *prop;
	regex_t regexp;
	const char *repourl;
};

static int
match_prop_cstring(const struct search_prop_ctx *ctx, const char *pkgver, const char *str)
{
	if (ctx->regex) {
		if (regexec(&ctx->regexp, str, 0, 0, 0) != 0)
			return 0;
	} else {
		if (!strcasestr(str, ctx->pattern))
			return 0;
	}

	if (ctx->repo_mode)
		printf("%s: %s (%s)\n", pkgver, str, ctx->repourl);
	else
		printf("%s: %s\n", pkgver, str);

	return 0;
}

static int
match_prop_number(const struct search_prop_ctx *ctx, const char *pkgver, xbps_number_t num)
{
	char fmt[8];

	if (xbps_humanize_number(fmt, xbps_number_integer_value(num)) == -1)
		return -EINVAL;

	return match_prop_cstring(ctx, pkgver, fmt);
}

static int
match_prop_object(const struct search_prop_ctx *ctx, const char *pkgver, xbps_object_t obj)
{
	switch (xbps_object_type(obj)) {
	case XBPS_TYPE_UNKNOWN:
	case XBPS_TYPE_ARRAY:
	case XBPS_TYPE_DATA:
	case XBPS_TYPE_DICTIONARY:
	case XBPS_TYPE_DICT_KEYSYM:
		return 0;
	case XBPS_TYPE_BOOL:
		return match_prop_cstring(
		    ctx, pkgver, xbps_bool_true(obj) ? "true" : "false");
	case XBPS_TYPE_NUMBER:
		return match_prop_number(ctx, pkgver, obj);
	case XBPS_TYPE_STRING:
		return match_prop_cstring(
		    ctx, pkgver, xbps_string_cstring_nocopy(obj));
	}
	abort();
}

static int
match_prop_array(const struct search_prop_ctx *ctx, const char *pkgver, xbps_array_t array)
{
	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		int r = match_prop_object(ctx, pkgver, xbps_array_get(array, i));
		if (r < 0)
			return r;
	}
	return 0;
}

static int
search_prop_cb(struct xbps_handle *xhp UNUSED, xbps_object_t pkgd,
    const char *key UNUSED, void *arg, bool *done UNUSED)
{
	const struct search_prop_ctx *ctx = arg;
	xbps_object_t obj;
	const char *pkgver = NULL;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
		abort();

	obj = xbps_dictionary_get(pkgd, ctx->prop);
	if (xbps_object_type(obj) == XBPS_TYPE_ARRAY)
		return match_prop_array(ctx, pkgver, obj);
	return match_prop_object(ctx, pkgver, obj);
}

static int
search_prop_repo_cb(struct xbps_repo *repo, void *arg, bool *done UNUSED)
{
	xbps_array_t keys;
	struct search_prop_ctx *ctx = arg;
	int rv;

	keys = xbps_dictionary_all_keys(repo->idx);
	if (!keys)
		return xbps_error_oom();

	ctx->repourl = repo->uri;
	rv = xbps_array_foreach_cb(
	    repo->xhp, keys, repo->idx, search_prop_cb, ctx);

	xbps_object_release(keys);
	return rv;
}

int
search_prop(struct xbps_handle *xhp, bool repo_mode, const char *pattern, const char *prop, bool regex)
{
	struct search_prop_ctx ctx = {
		.repo_mode = repo_mode,
		.regex = regex,
		.pattern = pattern,
		.prop = prop,
	};
	int r;

	if (regex) {
		r = regcomp(
		    &ctx.regexp, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE);
		if (r != 0) {
			char errbuf[4096];
			regerror(r, &ctx.regexp, errbuf, sizeof(errbuf));
			xbps_error_printf("failed to compile regexp: %s: %s\n",
			    pattern, errbuf);
			return EXIT_FAILURE;
		}
	}

	if (repo_mode)
		r = xbps_rpool_foreach(xhp, search_prop_repo_cb, &ctx);
	else
		r = xbps_pkgdb_foreach_cb(xhp, search_prop_cb, &ctx);
	if (r != 0)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}
