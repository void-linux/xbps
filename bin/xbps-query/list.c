/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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

#include <sys/types.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xbps.h>

#include "xbps/fmt.h"
#include "xbps/json.h"

#include "defs.h"

struct length_max_cb {
	const char *key;
	int max;
};

static int
length_max_cb(struct xbps_handle *xhp UNUSED, xbps_object_t obj,
		const char *key UNUSED, void *arg, bool *loop_done UNUSED)
{
	struct length_max_cb *ctx = arg;
	const char *s = NULL;
	size_t len;

	if (!xbps_dictionary_get_cstring_nocopy(obj, ctx->key, &s))
		return -errno;

	len = strlen(s);
	if (len > INT_MAX)
		return -ERANGE;
	if ((int)len > ctx->max)
		ctx->max = len;

	return 0;
}

struct list_pkgver_cb {
	unsigned int pkgver_align;
	unsigned int maxcols;
	char *buf;
	struct xbps_fmt *fmt;
};

static int
list_pkgs_pkgdb_cb(struct xbps_handle *xhp UNUSED,
		  xbps_object_t obj,
		  const char *key UNUSED,
		  void *arg,
		  bool *loop_done UNUSED)
{
	struct list_pkgver_cb *ctx = arg;
	const char *pkgver = NULL, *short_desc = NULL, *state_str = NULL;
	unsigned int len;
	pkg_state_t state;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(obj, "short_desc", &short_desc);
	if (!pkgver || !short_desc)
		return EINVAL;

	xbps_pkg_state_dictionary(obj, &state);

	switch (state) {
	case XBPS_PKG_STATE_INSTALLED:     state_str = "ii"; break;
	case XBPS_PKG_STATE_UNPACKED:      state_str = "uu"; break;
	case XBPS_PKG_STATE_HALF_REMOVED:  state_str = "hr"; break;
	case XBPS_PKG_STATE_BROKEN:        state_str = "br"; break;
	case XBPS_PKG_STATE_NOT_INSTALLED: state_str = "??"; break;
	}

	if (!ctx->buf) {
		printf("%s %-*s %s\n", state_str, ctx->pkgver_align, pkgver,
		    short_desc);
		return 0;
	}

	/* add ellipsis if the line was truncated */
	len = snprintf(ctx->buf, ctx->maxcols, "%s %-*s %s\n", state_str,
	    ctx->pkgver_align, pkgver, short_desc);
	if (len >= ctx->maxcols && ctx->maxcols > sizeof("..."))
		memcpy(ctx->buf + ctx->maxcols - sizeof("..."), "...", sizeof("..."));
	fputs(ctx->buf, stdout);

	return 0;
}

int
list_pkgs_pkgdb(struct xbps_handle *xhp)
{
	struct length_max_cb longest = {.key = "pkgver"};
	struct list_pkgver_cb lpc = {0};

	int r = xbps_pkgdb_foreach_cb_multi(xhp, length_max_cb, &longest);
	if (r < 0)
		return r;

	lpc.pkgver_align = longest.max;
	lpc.maxcols = get_maxcols();
	if (lpc.maxcols > 0) {
		lpc.buf = malloc(lpc.maxcols);
		if (!lpc.buf)
			return -errno;
	}

	return xbps_pkgdb_foreach_cb(xhp, list_pkgs_pkgdb_cb, &lpc);
}

struct list_pkgdb_ctx {
	struct xbps_fmt *fmt;
	struct xbps_json_printer *json;
	int (*filter)(xbps_object_t obj);
};

static int
fmt_pkg_cb(FILE *fp, const struct xbps_fmt_var *var, void *data)
{
	const char *pkgver = NULL;
	xbps_dictionary_t pkgd = data;
	xbps_object_t obj;

	obj = xbps_dictionary_get(pkgd, var->name);
	if (obj)
		return xbps_fmt_print_object(var, obj, fp);

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
		xbps_error_printf("invalid package: missing `pkgver`\n");
		return -EINVAL;
	}

	if (strcmp(var->name, "pkgname") == 0) {
		char pkgname[XBPS_NAME_SIZE];
		if (xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			xbps_error_printf("invalid `pkgver`: %s\n", pkgver);
			return -EINVAL;
		}
		return xbps_fmt_print_string(var, pkgname, 0, fp);
	} else if (strcmp(var->name, "version") == 0) {
		const char *version = xbps_pkg_version(pkgver);
		if (!version) {
			xbps_error_printf("invalid `pkgver`: %s\n", pkgver);
			return -EINVAL;
		}
		return xbps_fmt_print_string(var, version, 0, fp);
	} else if (strcmp(var->name, "revision") == 0) {
		const char *revision = xbps_pkg_revision(pkgver);
		if (!revision) {
			xbps_error_printf("invalid `pkgver`: %s\n", pkgver);
			return -EINVAL;
		}
		return xbps_fmt_print_string(var, revision, 0, fp);
	}

	return xbps_fmt_print_object(var, NULL, fp);
}

static int
list_pkgdb_cb(struct xbps_handle *xhp UNUSED, xbps_object_t obj,
		const char *key UNUSED, void *arg, bool *loop_done UNUSED)
{
	struct list_pkgdb_ctx *ctx = arg;
	int r;

	if (ctx->filter) {
		r = ctx->filter(obj);
		if (r <= 0)
			return r;
	}

	if (ctx->fmt) {
		r = xbps_fmt(ctx->fmt, &fmt_pkg_cb, obj, stdout);
	} else if (ctx->json) {
		r = xbps_json_print_xbps_object(ctx->json, obj);
		if (r < 0)
			return r;
		fprintf(ctx->json->file, "\n");
	} else {
		r = -ENOTSUP;
	}
	return r;
}

int
list_pkgdb(struct xbps_handle *xhp, int (*filter)(xbps_object_t), const char *format, int json)
{
	struct list_pkgdb_ctx ctx = {.filter = filter};
	struct xbps_json_printer pr = {0};
	int r;
	if (json > 0) {
		pr.indent = (json-1) * 2;
		pr.compact = pr.indent == 0;
		pr.file = stdout;
		ctx.json = &pr;
	} else if (format) {
		ctx.fmt = xbps_fmt_parse(format);
		if (!ctx.fmt) {
			r = -errno;
			xbps_error_printf("failed to parse format: %s\n", strerror(-r));
			return r;
		}
	}
	r = xbps_pkgdb_foreach_cb(xhp, list_pkgdb_cb, &ctx);
	xbps_fmt_free(ctx.fmt);
	return r;
}

int
list_manual_pkgs(struct xbps_handle *xhp UNUSED,
		 xbps_object_t obj,
		 const char *key UNUSED,
		 void *arg UNUSED,
		 bool *loop_done UNUSED)
{
	const char *pkgver = NULL;
	bool automatic = false;

	xbps_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (automatic == false) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		puts(pkgver);
	}

	return 0;
}

int
list_orphans(struct xbps_handle *xhp, const char *format)
{
	xbps_array_t orphans;
	struct xbps_fmt *fmt;
	int r = 0;

	fmt = xbps_fmt_parse(format);
	if (!fmt) {
		r = -errno;
		xbps_error_printf("failed to parse format: %s\n", strerror(-r));
		return r;
	}

	orphans = xbps_find_pkg_orphans(xhp, NULL);
	if (!orphans) {
		r = -errno;
		xbps_error_printf("failed to find orphans: %s\n", strerror(-r));
		goto err;
	}

	for (unsigned int i = 0; i < xbps_array_count(orphans); i++) {
		xbps_object_t obj = xbps_array_get(orphans, i);
		if (!obj)
			return -errno;
		r = xbps_fmt(fmt, &fmt_pkg_cb, obj, stdout);
		if (r < 0)
			goto err;
	}
err:
	xbps_fmt_free(fmt);
	return r;
}
