/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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
set_new_state(prop_dictionary_t dict, pkg_state_t state)
{
	const struct state *stp;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);

	for (stp = states; stp->string != NULL; stp++)
		if (state == stp->number)
			break;

	if (stp->string == NULL)
		return EINVAL;

	if (!prop_dictionary_set_cstring_nocopy(dict, "state", stp->string))
		return EINVAL;

	return 0;
}

static pkg_state_t
get_state(prop_dictionary_t dict)
{
	const struct state *stp;
	const char *state_str;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);

	if (!prop_dictionary_get_cstring_nocopy(dict,
	    "state", &state_str))
		return 0;

	for (stp = states; stp->string != NULL; stp++)
		if (strcmp(state_str, stp->string) == 0)
			break;

	return stp->number;
}

int
xbps_pkg_state_installed(struct xbps_handle *xhp,
			 const char *pkgname,
			 pkg_state_t *state)
{
	prop_dictionary_t pkgd;

	assert(pkgname != NULL);
	assert(state != NULL);

	pkgd = xbps_pkgdb_get_pkgd(xhp, pkgname, false);
	if (pkgd == NULL)
		return ENOENT;

	*state = get_state(pkgd);
	if (*state == 0)
		return EINVAL;

	return 0;
}

int
xbps_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t *state)
{
	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(state != NULL);

	if ((*state = get_state(dict)) == 0)
		return EINVAL;

	return 0;
}

int
xbps_set_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t state)
{
	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);

	return set_new_state(dict, state);
}

static int
set_pkg_objs(prop_dictionary_t pkgd, const char *name, const char *version)
{
	char *pkgver;

	if (!prop_dictionary_set_cstring_nocopy(pkgd, "pkgname", name))
		return EINVAL;

	if (!prop_dictionary_set_cstring_nocopy(pkgd, "version", version))
		return EINVAL;

	pkgver = xbps_xasprintf("%s-%s", name, version);
	assert(pkgver != NULL);

	if (!prop_dictionary_set_cstring_nocopy(pkgd, "pkgver", pkgver)) {
		free(pkgver);
		return EINVAL;
	}
	free(pkgver);

	return 0;
}

int
xbps_set_pkg_state_installed(struct xbps_handle *xhp,
			     const char *pkgname,
			     const char *version,
			     pkg_state_t state)
{
	prop_dictionary_t pkgd;
	bool newpkg = false;
	int rv;

	assert(pkgname != NULL);

	if (xhp->pkgdb == NULL) {
		xhp->pkgdb = prop_array_create();
		if (xhp->pkgdb == NULL)
			return ENOMEM;

		pkgd = prop_dictionary_create();
		if (pkgd == NULL) {
			prop_object_release(xhp->pkgdb);
			xhp->pkgdb = NULL;
			return ENOMEM;
		}
		if ((rv = set_pkg_objs(pkgd, pkgname, version)) != 0) {
			prop_object_release(xhp->pkgdb);
			prop_object_release(pkgd);
			xhp->pkgdb = NULL;
			return rv;
		}
		if ((rv = set_new_state(pkgd, state)) != 0) {
			prop_object_release(xhp->pkgdb);
			prop_object_release(pkgd);
			xhp->pkgdb = NULL;
			return rv;
		}
		if (!xbps_add_obj_to_array(xhp->pkgdb, pkgd)) {
			prop_object_release(xhp->pkgdb);
			prop_object_release(pkgd);
			xhp->pkgdb = NULL;
			return EINVAL;
		}
	} else {
		pkgd = xbps_pkgdb_get_pkgd(xhp, pkgname, false);
		if (pkgd == NULL) {
			newpkg = true;
			pkgd = prop_dictionary_create();
			if ((rv = set_pkg_objs(pkgd, pkgname, version)) != 0)
				return rv;
		}
		if ((rv = set_new_state(pkgd, state)) != 0) {
			if (newpkg)
				prop_object_release(pkgd);
			return rv;
		}
		if (newpkg) {
			if (!xbps_add_obj_to_array(xhp->pkgdb, pkgd)) {
				prop_object_release(pkgd);
				return EINVAL;
			}
		} else {
			if ((rv = xbps_array_replace_dict_by_name(xhp->pkgdb,
			    pkgd, pkgname)) != 0)
				return rv;
		}
	}

	return rv;
}
