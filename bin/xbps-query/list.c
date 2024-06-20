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
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "defs.h"

struct list_pkgver_cb {
	unsigned int pkgver_len;
	unsigned int maxcols;
	char *linebuf;
};

int
list_pkgs_in_dict(struct xbps_handle *xhp UNUSED,
		  xbps_object_t obj,
		  const char *key UNUSED,
		  void *arg,
		  bool *loop_done UNUSED)
{
	struct list_pkgver_cb *lpc = arg;
	const char *pkgver = NULL, *short_desc = NULL, *state_str = NULL;
	unsigned int len;
	pkg_state_t state;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(obj, "short_desc", &short_desc);
	if (!pkgver || !short_desc)
		return EINVAL;

	xbps_pkg_state_dictionary(obj, &state);

	if (state == XBPS_PKG_STATE_INSTALLED)
		state_str = "ii";
	else if (state == XBPS_PKG_STATE_UNPACKED)
		state_str = "uu";
	else if (state == XBPS_PKG_STATE_HALF_REMOVED)
		state_str = "hr";
	else
		state_str = "??";

	if (lpc->linebuf == NULL) {
		printf("%s %-*s %s\n",
			state_str,
			lpc->pkgver_len, pkgver,
			short_desc);
		return 0;
	}

	len = snprintf(lpc->linebuf, lpc->maxcols, "%s %-*s %s",
	    state_str,
		lpc->pkgver_len, pkgver,
		short_desc);
	/* add ellipsis if the line was truncated */
	if (len >= lpc->maxcols && lpc->maxcols > 4) {
		for (unsigned int j = 0; j < 3; j++)
			lpc->linebuf[lpc->maxcols-j-1] = '.';
		lpc->linebuf[lpc->maxcols] = '\0';
	}
	puts(lpc->linebuf);

	return 0;
}

int
of_init(struct orderby_information *ofp)
{
	ofp->size = 32;
	ofp->items = malloc(ofp->size * sizeof(ofp->items[0]));
	if (ofp->items == NULL) {
		exit(EXIT_FAILURE);
	}

	return 0;
}

static int
of_put(struct orderby_information *ofp,
		xbps_object_t obj)
{
	ofp->items[ofp->num].obj = obj;
	ofp->items[ofp->num].parent = ofp;

	ofp->num++;

	if (ofp->num == ofp->size) {
		ofp->size *= 2;

		ofp->items = realloc(ofp->items, ofp->size * sizeof(ofp->items[0]));
		if (ofp->items == NULL) {
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

static int
of_compare(const void *a,
		const void *b)
{
	const struct pkgver_ordering *apo, *bpo;
	xbps_object_t aordering, bordering;
	xbps_type_t atype, btype;
	char const *key;
	bool reverse_order;
	int rv;

	apo = a;
	bpo = b;
	key = apo->parent->orderby;
	reverse_order = apo->parent->reverse_order;

	aordering = xbps_dictionary_get(apo->obj, key);
	bordering = xbps_dictionary_get(bpo->obj, key);

	atype = xbps_object_type(aordering);
	btype = xbps_object_type(bordering);

	/* deal with packages which don't contain these values */
	if (atype == XBPS_TYPE_UNKNOWN && btype == XBPS_TYPE_UNKNOWN) {
		rv = 0;
	} else if (atype == XBPS_TYPE_UNKNOWN && btype != XBPS_TYPE_UNKNOWN) {
		rv = -1;
	} else if (atype != XBPS_TYPE_UNKNOWN && btype == XBPS_TYPE_UNKNOWN) {
		rv = 1;
	} else {
		assert(atype == btype);

		if (atype == XBPS_TYPE_STRING) {
			const char *aordering_str, *bordering_str;
			aordering_str = xbps_string_cstring_nocopy(aordering);
			bordering_str = xbps_string_cstring_nocopy(bordering);

			rv = strcmp(aordering_str, bordering_str);

		} else if (atype == XBPS_TYPE_NUMBER) {
			unsigned int aordering_int, bordering_int;
			aordering_int = xbps_number_unsigned_integer_value(aordering);
			bordering_int = xbps_number_unsigned_integer_value(bordering);

			if (aordering_int > bordering_int) {
				rv = 1;
			} else if (aordering_int == bordering_int) {
				rv = 0;
			} else {
				rv = -1;
			}

		} else {
			xbps_error_printf("Can't sort packages by key \"%s\".\n", key);
			exit(EXIT_FAILURE);
			rv = 0;
		}
	}

	return (reverse_order) ? -rv : rv;
}

static void
of_sort(struct orderby_information *ofp)
{
	qsort(ofp->items, ofp->num, sizeof(*ofp->items), of_compare);
}

int
of_print(struct orderby_information *ofp)
{
	if (ofp->orderby){
		of_sort(ofp);
	}

	for(size_t i = 0; i < ofp->num; i++) {
		const char *pkgver;
		pkgver = NULL;

		xbps_dictionary_get_cstring_nocopy(ofp->items[i].obj, "pkgver", &pkgver);

		puts(pkgver);
	}

	return 0;
}

int
of_free(struct orderby_information *ofp)
{
	free(ofp->items);

	return 0;
}

int
of_put_orphans(struct xbps_handle *xhp, struct orderby_information *ofp)
{
	xbps_array_t orphans;

	orphans = xbps_find_pkg_orphans(xhp, NULL);
	if (orphans == NULL)
		return EINVAL;

	for (unsigned int i = 0; i < xbps_array_count(orphans); i++) {
		of_put(ofp, xbps_array_get(orphans, i));
	}

	return 0;
}

int
list_orderby(struct xbps_handle *xhp,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *loop_done UNUSED)
{
	struct orderby_information *ofp = arg;

	if (ofp->filter && ofp->filter(obj)) {
		if (ofp->orderby) {
			/* verify if package has property */
			if (xbps_object_type(xbps_dictionary_get(obj, ofp->orderby))
					== XBPS_TYPE_UNKNOWN) {
				const char *pkgver;
				xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
				if (pkgver == NULL) {
					exit(EXIT_FAILURE);
				}
				xbps_dbg_printf(xhp, "pkg %s doesn't have the \"%s\" property\n",
						pkgver, ofp->orderby);
			}
		}

		of_put(ofp, obj);
	}

	return 0;
}

bool
filter_manual_pkgs(xbps_object_t obj)
{
	bool automatic = false;
	xbps_dictionary_get_bool(obj, "automatic-install", &automatic);
	return !automatic;
}

bool
filter_hold_pkgs(xbps_object_t obj)
{
	return xbps_dictionary_get(obj, "hold");
}

bool
filter_repolock_pkgs(xbps_object_t obj)
{
	return xbps_dictionary_get(obj, "repolock");
}

int
list_pkgs_pkgdb(struct xbps_handle *xhp)
{
	struct list_pkgver_cb lpc;

	lpc.pkgver_len = find_longest_pkgver(xhp, NULL);
	lpc.maxcols = get_maxcols();
	lpc.linebuf = NULL;
	if (lpc.maxcols > 0) {
		lpc.linebuf = malloc(lpc.maxcols);
		if (lpc.linebuf == NULL)
			exit(1);
	}

	return xbps_pkgdb_foreach_cb(xhp, list_pkgs_in_dict, &lpc);
}

static void
repo_list_uri(struct xbps_repo *repo)
{
	const char *signedby = NULL;
	uint16_t pubkeysize = 0;

	printf("%5zd %s",
	    repo->idx ? (ssize_t)xbps_dictionary_count(repo->idx) : -1,
	    repo->uri);
	printf(" (RSA %s)\n", repo->is_signed ? "signed" : "unsigned");
	if (repo->xhp->flags & XBPS_FLAG_VERBOSE) {
		xbps_data_t pubkey;
		char *hexfp = NULL;

		xbps_dictionary_get_cstring_nocopy(repo->idxmeta, "signature-by", &signedby);
		xbps_dictionary_get_uint16(repo->idxmeta, "public-key-size", &pubkeysize);
		pubkey = xbps_dictionary_get(repo->idxmeta, "public-key");
		if (pubkey)
			hexfp = xbps_pubkey2fp(pubkey);
		if (signedby)
			printf("      Signed-by: %s\n", signedby);
		if (pubkeysize && hexfp)
			printf("      %u %s\n", pubkeysize, hexfp);
		if (hexfp)
			free(hexfp);
	}
}

static void
repo_list_uri_err(const char *repouri)
{
	printf("%5zd %s (RSA maybe-signed)\n", (ssize_t) -1, repouri);
}

int
repo_list(struct xbps_handle *xhp)
{
	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		const char *repouri = NULL;
		struct xbps_repo *repo;
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		repo = xbps_repo_open(xhp, repouri);
		if (!repo) {
			repo_list_uri_err(repouri);
			continue;
		}
		repo_list_uri(repo);
		xbps_repo_release(repo);
	}
	return 0;
}

struct fflongest {
	xbps_dictionary_t d;
	unsigned int len;
};

static int
_find_longest_pkgver_cb(struct xbps_handle *xhp UNUSED,
			xbps_object_t obj,
			const char *key UNUSED,
			void *arg,
			bool *loop_done UNUSED)
{
	struct fflongest *ffl = arg;
	const char *pkgver = NULL;
	unsigned int len;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	len = strlen(pkgver);
	if (ffl->len == 0 || len > ffl->len)
		ffl->len = len;

	return 0;
}

unsigned int
find_longest_pkgver(struct xbps_handle *xhp, xbps_object_t o)
{
	struct fflongest ffl;

	ffl.d = o;
	ffl.len = 0;

	if (xbps_object_type(o) == XBPS_TYPE_DICTIONARY) {
		xbps_array_t array;

		array = xbps_dictionary_all_keys(o);
		(void)xbps_array_foreach_cb_multi(xhp, array, o,
		    _find_longest_pkgver_cb, &ffl);
		xbps_object_release(array);
	} else {
		(void)xbps_pkgdb_foreach_cb_multi(xhp,
		    _find_longest_pkgver_cb, &ffl);
	}

	return ffl.len;
}
