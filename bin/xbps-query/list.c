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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "defs.h"

struct list_pkgver_cb {
	int pkgver_len;
	int maxcols;
};

int
list_pkgs_in_dict(struct xbps_handle *xhp UNUSED,
		  xbps_object_t obj,
		  const char *key UNUSED,
		  void *arg,
		  bool *loop_done UNUSED)
{
	struct list_pkgver_cb *lpc = arg;
	const char *pkgver, *short_desc, *state_str;
	char tmp[255], *out = NULL;
	int i, len = 0;
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

	snprintf(tmp, sizeof(tmp), "%s %s", state_str, pkgver);
	for (i = strlen(pkgver) + 3; i < lpc->pkgver_len; i++)
		tmp[i] = ' ';

	tmp[i] = '\0';
	len = strlen(tmp) + strlen(short_desc) + 2;
	if (lpc->maxcols && len > lpc->maxcols) {
		out = malloc(lpc->maxcols+1);
		assert(out);
		snprintf(out, lpc->maxcols - 3,
		    "%s %s", tmp, short_desc);
		xbps_strlcat(out, "...\n", lpc->maxcols+1);
		printf("%s", out);
		free(out);
	} else {
		printf("%s %s\n", tmp, short_desc);
	}

	return 0;
}

int
list_manual_pkgs(struct xbps_handle *xhp UNUSED,
		 xbps_object_t obj,
		 const char *key UNUSED,
		 void *arg UNUSED,
		 bool *loop_done UNUSED)
{
	const char *pkgver;
	bool automatic = false;

	xbps_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (automatic == false) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}

int
list_hold_pkgs(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg UNUSED,
		bool *loop_done UNUSED)
{
	const char *pkgver;

	if (xbps_dictionary_get(obj, "hold")) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}

int
list_repolock_pkgs(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg UNUSED,
		bool *loop_done UNUSED)
{
	const char *pkgver;

	if (xbps_dictionary_get(obj, "repolock")) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}

int
list_orphans(struct xbps_handle *xhp)
{
	xbps_array_t orphans;
	const char *pkgver;

	orphans = xbps_find_pkg_orphans(xhp, NULL);
	if (orphans == NULL)
		return EINVAL;

	for (unsigned int i = 0; i < xbps_array_count(orphans); i++) {
		xbps_dictionary_get_cstring_nocopy(xbps_array_get(orphans, i),
		    "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}

int
list_pkgs_pkgdb(struct xbps_handle *xhp)
{
	struct list_pkgver_cb lpc;

	lpc.pkgver_len = find_longest_pkgver(xhp, NULL) + 3; /* for state */
	lpc.maxcols = get_maxcols();

	return xbps_pkgdb_foreach_cb(xhp, list_pkgs_in_dict, &lpc);
}

static int
repo_list_uri_cb(struct xbps_repo *repo, void *arg UNUSED, bool *done UNUSED)
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
			hexfp = xbps_pubkey2fp(repo->xhp, pubkey);
		if (signedby)
			printf("      Signed-by: %s\n", signedby);
		if (pubkeysize && hexfp)
			printf("      %u %s\n", pubkeysize, hexfp);
		if (hexfp)
			free(hexfp);
	}
	return 0;
}

int
repo_list(struct xbps_handle *xhp)
{
	int rv;

	rv = xbps_rpool_foreach(xhp, repo_list_uri_cb, NULL);
	if (rv != 0 && rv != ENOTSUP) {
		fprintf(stderr, "Failed to initialize rpool: %s\n",
		    strerror(rv));
		return rv;
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
	const char *pkgver;
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
