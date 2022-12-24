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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"
#include "uthash.h"

struct item;

struct depn {
	struct depn *dnext;
	struct item *item;
};

struct item {
	char *pkgn;		/* hash key */
	const char *pkgver;
	xbps_array_t rdeps;
	struct depn *dbase;
	UT_hash_handle hh;
};

static struct item *items = NULL;
static xbps_array_t result;

static struct item *
lookupItem(const char *pkgn)
{
	struct item *item = NULL;

	assert(pkgn);

	HASH_FIND_STR(items, pkgn, item);
	return item;
}

static struct item *
addItem(xbps_array_t rdeps, const char *pkgn, const char *pkgver)
{
	struct item *item = NULL;

	assert(pkgn);
	assert(pkgver);

	HASH_FIND_STR(items, pkgn, item);
	if (item)
		return item;

	item = malloc(sizeof(*item));
	assert(item);
	item->pkgn = strdup(pkgn);
	item->pkgver = pkgver;
	item->rdeps = rdeps;
	item->dbase = NULL;
	HASH_ADD_KEYPTR(hh, items, item->pkgn, strlen(pkgn), item);

	return item;
}

static void
addDepn(struct item *item, struct item *xitem)
{
	struct depn *depn = calloc(1, sizeof(*depn));

	assert(depn);
	assert(item);
	assert(xitem);

	depn->item = item;
	depn->dnext = xitem->dbase;
	xitem->dbase = depn;
}

static void
add_deps_recursive(struct item *item, bool first)
{
	struct depn *dep;
	xbps_string_t str;

	if (xbps_match_string_in_array(result, item->pkgver))
		return;

	for (dep = item->dbase; dep; dep = dep->dnext)
		add_deps_recursive(dep->item, false);

	if (first)
		return;

	str = xbps_string_create_cstring(item->pkgver);
	assert(str);
	xbps_array_add_first(result, str);
	xbps_object_release(str);
}

static void
cleanup(void)
{
	struct item *item, *itmp;

	HASH_ITER(hh, items, item, itmp) {
		HASH_DEL(items, item);
		if (item->dbase)
			free(item->dbase);
		free(item->pkgn);
		free(item);
	}
}

/*
 * Recursively calculate all dependencies.
 */
static struct item *
ordered_depends(struct xbps_handle *xhp, xbps_dictionary_t pkgd, bool rpool,
		size_t depth)
{
	xbps_array_t rdeps, provides;
	xbps_string_t str;
	struct item *item = NULL, *xitem = NULL;
	const char *pkgver = NULL, *pkgname = NULL;

	assert(xhp);
	assert(pkgd);

	rdeps = xbps_dictionary_get(pkgd, "run_depends");
	provides = xbps_dictionary_get(pkgd, "provides");
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);

	item = lookupItem(pkgname);
	if (item) {
		add_deps_recursive(item, depth == 0);
		return item;
	}

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
		abort();
	}

	item = addItem(rdeps, pkgname, pkgver);
	assert(item);

	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++) {
		xbps_dictionary_t curpkgd;
		const char *curdep = NULL;
		char curdepname[XBPS_NAME_SIZE];

		xbps_array_get_cstring_nocopy(rdeps, i, &curdep);
		if (rpool) {
			if ((curpkgd = xbps_rpool_get_pkg(xhp, curdep)) == NULL)
				curpkgd = xbps_rpool_get_virtualpkg(xhp, curdep);
		} else {
			if ((curpkgd = xbps_pkgdb_get_pkg(xhp, curdep)) == NULL)
				curpkgd = xbps_pkgdb_get_virtualpkg(xhp, curdep);
			/* Ignore missing local runtime dependencies, because ignorepkg */
			if (curpkgd == NULL)
				continue;
		}
		if (curpkgd == NULL) {
			/* package depends on missing dependencies */
			xbps_dbg_printf("%s: missing dependency '%s'\n", pkgver, curdep);
			errno = ENODEV;
			return NULL;
		}
		if ((!xbps_pkgpattern_name(curdepname, XBPS_NAME_SIZE, curdep)) &&
		    (!xbps_pkg_name(curdepname, XBPS_NAME_SIZE, curdep))) {
			abort();
		}

		if (provides && xbps_match_pkgname_in_array(provides, curdepname)) {
			xbps_dbg_printf("%s: ignoring dependency %s "
			    "already in provides\n", pkgver, curdep);
			continue;
		}
		xitem = lookupItem(curdepname);
		if (xitem) {
			add_deps_recursive(xitem, false);
			continue;
		}
		xitem = ordered_depends(xhp, curpkgd, rpool, depth+1);
		if (xitem == NULL) {
			/* package depends on missing dependencies */
			xbps_dbg_printf("%s: missing dependency '%s'\n", pkgver, curdep);
			errno = ENODEV;
			return NULL;
		}
		assert(xitem);
		addDepn(item, xitem);
	}
	/* all deps were processed, add item to head */
	if (depth > 0 && !xbps_match_string_in_array(result, item->pkgver)) {
		str = xbps_string_create_cstring(item->pkgver);
		assert(str);
		xbps_array_add_first(result, str);
		xbps_object_release(str);
	}
	return item;
}

xbps_array_t HIDDEN
xbps_get_pkg_fulldeptree(struct xbps_handle *xhp, const char *pkg, bool rpool)
{
	xbps_dictionary_t pkgd;

	result = xbps_array_create();
	assert(result);

	if (rpool) {
		if (((pkgd = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL))
			return NULL;
	} else {
		if (((pkgd = xbps_pkgdb_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkgd = xbps_pkgdb_get_virtualpkg(xhp, pkg)) == NULL))
			return NULL;
	}
	if (ordered_depends(xhp, pkgd, rpool, 0) == NULL)
		return NULL;

	cleanup();
	return result;
}
