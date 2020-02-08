/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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

struct state {
	const char *string;
	pkg_state_t number;
};

static const struct state states[] = {
	{ "unpacked", 		XBPS_PKG_STATE_UNPACKED },
	{ "installed",		XBPS_PKG_STATE_INSTALLED },
	{ "broken",		XBPS_PKG_STATE_BROKEN },
	{ "half-removed",	XBPS_PKG_STATE_HALF_REMOVED },
	{ "not-installed",	XBPS_PKG_STATE_NOT_INSTALLED },
	{ NULL,			0 }
};


/**
 * @file lib/package_state.c
 * @brief Package state handling routines
 * @defgroup pkgstates Package state handling functions
 */

static int
set_new_state(xbps_dictionary_t dict, pkg_state_t state)
{
	const struct state *stp;

	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);

	for (stp = states; stp->string != NULL; stp++)
		if (state == stp->number)
			break;

	if (stp->string == NULL)
		return EINVAL;

	if (!xbps_dictionary_set_cstring_nocopy(dict, "state", stp->string))
		return EINVAL;

	return 0;
}

static pkg_state_t
get_state(xbps_dictionary_t dict)
{
	const struct state *stp;
	const char *state_str;

	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);

	if (!xbps_dictionary_get_cstring_nocopy(dict,
	    "state", &state_str))
		return 0;

	for (stp = states; stp->string != NULL; stp++)
		if (strcmp(state_str, stp->string) == 0)
			break;

	return stp->number;
}

int
xbps_pkg_state_installed(struct xbps_handle *xhp,
			 const char *pkgver,
			 pkg_state_t *state)
{
	xbps_dictionary_t pkgd;

	assert(pkgver != NULL);
	assert(state != NULL);

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgver);
	if (pkgd == NULL)
		return ENOENT;

	*state = get_state(pkgd);
	if (*state == 0)
		return EINVAL;

	return 0;
}

int
xbps_pkg_state_dictionary(xbps_dictionary_t dict, pkg_state_t *state)
{
	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(state != NULL);

	if ((*state = get_state(dict)) == 0)
		return EINVAL;

	return 0;
}

int
xbps_set_pkg_state_dictionary(xbps_dictionary_t dict, pkg_state_t state)
{
	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);

	return set_new_state(dict, state);
}

int
xbps_set_pkg_state_installed(struct xbps_handle *xhp,
			     const char *pkgver,
			     pkg_state_t state)
{
	xbps_dictionary_t pkgd;
	char pkgname[XBPS_NAME_SIZE];
	int rv = 0;

	assert(pkgver != NULL);

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgver);
	if (pkgd == NULL) {
		pkgd = xbps_dictionary_create();
		if (pkgd == NULL)
			return ENOMEM;

		if (!xbps_dictionary_set_cstring_nocopy(pkgd,
		    "pkgver", pkgver)) {
			xbps_object_release(pkgd);
			return EINVAL;
		}
		if ((rv = set_new_state(pkgd, state)) != 0) {
			xbps_object_release(pkgd);
			return rv;
		}
		if (!xbps_pkg_name(pkgname, XBPS_NAME_SIZE, pkgver)) {
			abort();
		}
		if (!xbps_dictionary_set(xhp->pkgdb, pkgname, pkgd)) {
			xbps_object_release(pkgd);
			return EINVAL;
		}
		xbps_object_release(pkgd);
	} else {
		if ((rv = set_new_state(pkgd, state)) != 0)
			return rv;

		if (!xbps_pkg_name(pkgname, XBPS_NAME_SIZE, pkgver)) {
			abort();
		}
		if (!xbps_dictionary_set(xhp->pkgdb, pkgname, pkgd)) {
			return EINVAL;
		}
	}

	return rv;
}
