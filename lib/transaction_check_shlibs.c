/*-
 * Copyright (c) 2014-2020 Juan Romero Pardines.
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
#include <stdbool.h>
#include <errno.h>

#include "uthash.h"

#include "xbps.h"
#include "xbps/xbps_array.h"
#include "xbps/xbps_dictionary.h"
#include "xbps_api_impl.h"

/*
 * Verify shlib-{provides,requires} for packages in transaction.
 * This will catch cases where a package update would break its reverse
 * dependencies due to an incompatible SONAME bump:
 *
 * 	- foo-1.0 is installed and provides the 'libfoo.so.0' soname.
 * 	- foo-2.0 provides the 'libfoo.so.1' soname.
 * 	- baz-1.0 requires 'libfoo.so.0'.
 * 	- foo is updated to 2.0, hence baz-1.0 is now broken.
 *
 * Abort transaction if such case is found.
 */

struct shlib_entry {
	const char *name;
	UT_hash_handle hh;
};

struct shlib_ctx {
	struct xbps_handle *xhp;
	struct shlib_entry *entries;
	xbps_dictionary_t seen;
	xbps_array_t missing;
};

static struct shlib_entry *
shlib_entry_find(struct shlib_entry *head, const char *name)
{
	struct shlib_entry *res = NULL;
	HASH_FIND_STR(head, name, res);
	return res;
}

static struct shlib_entry *
shlib_entry_get(struct shlib_ctx *ctx, const char *name)
{
	struct shlib_entry *res = shlib_entry_find(ctx->entries, name);
	if (res)
		return res;
	res = calloc(1, sizeof(*res));
	if (!res) {
		xbps_error_printf("out of memory\n");
		errno = ENOMEM;
		return NULL;
	}
	res->name = name;
	HASH_ADD_STR(ctx->entries, name, res);
	return res;
}

static int
collect_shlib_array(struct shlib_ctx *ctx, xbps_array_t array)
{
	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		struct shlib_entry *entry;
		const char *shlib = NULL;
		if (!xbps_array_get_cstring_nocopy(array, i, &shlib))
			return -EINVAL;
		entry = shlib_entry_get(ctx, shlib);
		if (!entry)
			return -errno;
	}
	return 0;
}

static int
collect_shlibs(struct shlib_ctx *ctx, xbps_array_t pkgs)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_bool_t placeholder;

	// can't set null values to xbps_dictionary so just use one boolean
	placeholder = xbps_bool_create(true);
	if (!placeholder) {
		xbps_error_printf("out of memory\n");
		return -ENOMEM;
	}

	ctx->seen = xbps_dictionary_create();
	if (!ctx->seen) {
		xbps_error_printf("out of memory\n");
		return -ENOMEM;
	}

	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++) {
		const char *pkgname;
		xbps_dictionary_t pkgd = xbps_array_get(pkgs, i);
		xbps_array_t array;

		if (xbps_transaction_pkg_type(pkgd) == XBPS_TRANS_HOLD)
			continue;
		if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname)) {
			xbps_error_printf("invalid package: missing `pkgname` property\n");
			return -EINVAL;
		}
		if (!xbps_dictionary_set(ctx->seen, pkgname, placeholder)) {
			xbps_error_printf("out of memory\n");
			return -ENOMEM;
		}

		if (xbps_transaction_pkg_type(pkgd) == XBPS_TRANS_REMOVE)
			continue;

		array = xbps_dictionary_get(pkgd, "shlib-provides");
		if (array) {
			int r = collect_shlib_array(ctx, array);
			if (r < 0)
				return r;
		}
	}

	iter = xbps_dictionary_iterator(ctx->xhp->pkgdb);
	if (!iter) {
		xbps_error_printf("out of memory\n");
		return -ENOMEM;
	}

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t array;
		xbps_dictionary_t pkgd;
		const char *pkgname = NULL;

		pkgname = xbps_dictionary_keysym_cstring_nocopy(obj);
		/* ignore internal objs */
		if (strncmp(pkgname, "_XBPS_", 6) == 0)
			continue;

		pkgd = xbps_dictionary_get_keysym(ctx->xhp->pkgdb, obj);

		if (xbps_dictionary_get(ctx->seen, pkgname))
			continue;

		array = xbps_dictionary_get(pkgd, "shlib-provides");
		if (array) {
			int r = collect_shlib_array(ctx, array);
			if (r < 0)
				return r;
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
check_shlibs(struct shlib_ctx *ctx, xbps_array_t pkgs)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;

	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++) {
		xbps_array_t array;
		xbps_dictionary_t pkgd = xbps_array_get(pkgs, i);
		xbps_trans_type_t ttype = xbps_transaction_pkg_type(pkgd);

		if (ttype == XBPS_TRANS_HOLD || ttype == XBPS_TRANS_REMOVE)
			continue;

		array = xbps_dictionary_get(pkgd, "shlib-requires");
		if (!array)
			continue;
		for (unsigned int j = 0; j < xbps_array_count(array); j++) {
			const char *pkgver = NULL;
			const char *shlib = NULL;
			char *missing;
			if (!xbps_array_get_cstring_nocopy(array, j, &shlib))
				return -EINVAL;
			if (shlib_entry_find(ctx->entries, shlib))
				continue;
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
				return -EINVAL;
			missing = xbps_xasprintf(
			    "%s: broken, unresolvable shlib `%s'",
			    pkgver, shlib);
			if (!xbps_array_add_cstring_nocopy(ctx->missing, missing)) {
				xbps_error_printf("out of memory\n");
				return -ENOMEM;
			}
		}
	}

	iter = xbps_dictionary_iterator(ctx->xhp->pkgdb);
	if (!iter) {
		xbps_error_printf("out of memory\n");
		return -ENOMEM;
	}

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_array_t array;
		xbps_dictionary_t pkgd;
		const char *pkgname = NULL;

		pkgname = xbps_dictionary_keysym_cstring_nocopy(obj);
		/* ignore internal objs */
		if (strncmp(pkgname, "_XBPS_", 6) == 0)
			continue;

		pkgd  = xbps_dictionary_get_keysym(ctx->xhp->pkgdb, obj);

		if (xbps_dictionary_get(ctx->seen, pkgname))
			continue;

		array = xbps_dictionary_get(pkgd, "shlib-requires");
		if (!array)
			continue;
		for (unsigned int i = 0; i < xbps_array_count(array); i++) {
			const char *pkgver = NULL;
			const char *shlib = NULL;
			char *missing;
			if (!xbps_array_get_cstring_nocopy(array, i, &shlib))
				return -EINVAL;
			if (shlib_entry_find(ctx->entries, shlib))
				continue;
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
				return -EINVAL;
			missing = xbps_xasprintf(
			    "%s: broken, unresolvable shlib `%s'", pkgver,
			    shlib);
			if (!xbps_array_add_cstring_nocopy(ctx->missing, missing)) {
				xbps_error_printf("out of memory\n");
				return -ENOMEM;
			}
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

bool HIDDEN
xbps_transaction_check_shlibs(struct xbps_handle *xhp, xbps_array_t pkgs)
{
	struct shlib_entry *entry, *tmp;
	struct shlib_ctx ctx = { .xhp = xhp };
	int r;

	ctx.missing = xbps_dictionary_get(xhp->transd, "missing_shlibs");

	r = collect_shlibs(&ctx, pkgs);
	if (r < 0)
		goto err;

	r = check_shlibs(&ctx, pkgs);
	if (r < 0)
		goto err;

	if (xbps_array_count(ctx.missing) == 0)
		xbps_dictionary_remove(xhp->transd, "missing_shlibs");

	r = 0;
err:
	HASH_ITER(hh, ctx.entries, entry, tmp) {
		HASH_DEL(ctx.entries, entry);
		free(entry);
	}
	if (ctx.seen)
		xbps_object_release(ctx.seen);
	return r == 0;
}
