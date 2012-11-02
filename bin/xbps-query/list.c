/*-
 * Copyright (c) 2008-2012 Juan Romero Pardines.
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
#include <sys/ioctl.h>

#include "defs.h"

struct list_pkgver_cb {
	size_t pkgver_len;
	size_t maxcols;
};

size_t
get_maxcols(void)
{
	struct winsize ws;

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) == 0)
		return ws.ws_col;

	return 80;
}

int
list_pkgs_in_dict(struct xbps_handle *xhp,
		  prop_object_t obj,
		  void *arg,
		  bool *loop_done)
{
	struct list_pkgver_cb *lpc = arg;
	const char *pkgver, *short_desc, *arch;
	char *tmp = NULL, *out = NULL;
	size_t i, len = 0;
	bool chkarch;

	(void)xhp;
	(void)loop_done;

	chkarch = prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	if (chkarch && !xbps_pkg_arch_match(xhp, arch, NULL))
		return 0;

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "short_desc", &short_desc);
	if (!pkgver && !short_desc)
		return EINVAL;

	tmp = calloc(1, lpc->pkgver_len + 1);
	assert(tmp);
	memcpy(tmp, pkgver, lpc->pkgver_len);
	for (i = strlen(tmp); i < lpc->pkgver_len; i++)
		tmp[i] = ' ';

	tmp[i] = '\0';
	len = strlen(tmp) + strlen(short_desc) + 1;
	if (len > lpc->maxcols) {
		out = malloc(lpc->maxcols);
		assert(out);
		snprintf(out, lpc->maxcols-2, "%s %s", tmp, short_desc);
		strncat(out, "...", lpc->maxcols);
		printf("%s\n", out);
		free(out);
	} else {
		printf("%s %s\n", tmp, short_desc);
	}
	free(tmp);

	return 0;
}

int
list_manual_pkgs(struct xbps_handle *xhp,
		 prop_object_t obj,
		 void *arg,
		 bool *loop_done)
{
	const char *pkgver;
	bool automatic = false;

	(void)xhp;
	(void)arg;
	(void)loop_done;

	prop_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (automatic == false) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}

int
list_orphans(struct xbps_handle *xhp)
{
	prop_array_t orphans;
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *pkgver;

	orphans = xbps_find_pkg_orphans(xhp, NULL);
	if (orphans == NULL)
		return EINVAL;

	if (prop_array_count(orphans) == 0)
		return 0;

	iter = prop_array_iterator(orphans);
	if (iter == NULL)
		return ENOMEM;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}
	prop_object_iterator_release(iter);

	return 0;
}

int
list_pkgs_pkgdb(struct xbps_handle *xhp)
{
	struct list_pkgver_cb lpc;

	lpc.pkgver_len = find_longest_pkgver(xhp, NULL);
	lpc.maxcols = get_maxcols();

	return xbps_pkgdb_foreach_cb(xhp, list_pkgs_in_dict, &lpc);
}

static int
repo_list_uri_cb(struct xbps_handle *xhp,
		 struct xbps_rpool_index *rpi,
		 void *arg,
		 bool *done)
{
	(void)xhp;
	(void)arg;
	(void)done;

	printf("%s (%zu packages)\n", rpi->uri,
	    (size_t)prop_array_count(rpi->repo));

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

static int
_find_longest_pkgver_cb(struct xbps_handle *xhp,
			prop_object_t obj,
			void *arg,
			bool *loop_done)
{
	size_t *len = arg;
	const char *pkgver;

	(void)xhp;
	(void)loop_done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	if (*len == 0 || strlen(pkgver) > *len)
		*len = strlen(pkgver);

	return 0;
}

size_t
find_longest_pkgver(struct xbps_handle *xhp, prop_object_t o)
{
	size_t len = 0;

	if (prop_object_type(o) == PROP_TYPE_ARRAY)
		(void)xbps_callback_array_iter(xhp, o,
		    _find_longest_pkgver_cb, &len);
	else
		(void)xbps_pkgdb_foreach_cb(xhp,
		    _find_longest_pkgver_cb, &len);

	return len;
}

int
list_strings_sep_in_array(struct xbps_handle *xhp,
			  prop_object_t obj,
			  void *arg,
			  bool *loop_done)
{
	const char *sep = arg;

	(void)xhp;
	(void)loop_done;

	printf("%s%s\n", sep ? sep : "", prop_string_cstring_nocopy(obj));

	return 0;
}
