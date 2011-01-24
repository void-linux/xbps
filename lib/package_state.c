/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "xbps_api_impl.h"

struct state {
	const char *string;
	pkg_state_t number;
};

static const struct state states[] = {
	{ "unpacked", 		XBPS_PKG_STATE_UNPACKED },
	{ "installed",		XBPS_PKG_STATE_INSTALLED },
	{ "broken",		XBPS_PKG_STATE_BROKEN },
	{ "config-files",	XBPS_PKG_STATE_CONFIG_FILES },
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
	const char *pkgname;


	for (stp = states; stp->string != NULL; stp++)
		if (state == stp->number)
			break;

	if (stp->string == NULL)
		return -1;

	if (!prop_dictionary_set_cstring_nocopy(dict, "state", stp->string))
		return EINVAL;

	if (prop_dictionary_get_cstring_nocopy(dict, "pkgname", &pkgname)) {
		xbps_dbg_printf("%s: changed pkg state to '%s'\n",
		    pkgname, stp->string);
	}

	return 0;
}

static pkg_state_t
get_state(prop_dictionary_t dict)
{
	const struct state *stp;
	const char *state_str;

	assert(dict != NULL);

	prop_dictionary_get_cstring_nocopy(dict, "state", &state_str);
	assert(state_str != NULL);

	for (stp = states; stp->string != NULL; stp++)
		if (strcmp(state_str, stp->string) == 0)
			break;

	return stp->number;
}

int
xbps_get_pkg_state_installed(const char *pkgname, pkg_state_t *state)
{
	prop_dictionary_t pkgd;

	assert(pkgname != NULL);

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL)
		return errno;

	*state = get_state(pkgd);
	prop_object_release(pkgd);
	if (*state == 0)
		return EINVAL;

	return 0;
}

int
xbps_get_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t *state)
{
	assert(dict != NULL);

	if ((*state = get_state(dict)) == 0)
		return EINVAL;

	return 0;
}

int
xbps_set_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t state)
{
	assert(dict != NULL);

	return set_new_state(dict, state);
}

int
xbps_set_pkg_state_installed(const char *pkgname, pkg_state_t state)
{
	prop_dictionary_t dict = NULL, pkgd;
	prop_array_t array;
	char *plist;
	int rv = 0;
	bool newpkg = false;

	plist = xbps_xasprintf("%s/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL)
		return ENOMEM;

	if ((dict = prop_dictionary_internalize_from_zfile(plist)) == NULL) {
		dict = prop_dictionary_create();
		if (dict == NULL) {
			rv = ENOMEM;
			goto out;
		}
		array = prop_array_create();
		if (array == NULL) {
			rv = ENOMEM;
			goto out;
		}
		pkgd = prop_dictionary_create();
		if (pkgd == NULL) {
			rv = ENOMEM;
			prop_object_release(array);
			goto out;
		}

		if (!prop_dictionary_set_cstring_nocopy(pkgd, "pkgname",
		    pkgname)) {
			rv = EINVAL;
			prop_object_release(array);
			prop_object_release(pkgd);
			goto out;
		}
		if ((rv = set_new_state(pkgd, state)) != 0) {
			prop_object_release(array);
			prop_object_release(pkgd);
			goto out;
		}
		if (!xbps_add_obj_to_array(array, pkgd)) {
			rv = EINVAL;
			prop_object_release(array);
			prop_object_release(pkgd);
			goto out;
		}
		if (!xbps_add_obj_to_dict(dict, array, "packages")) {
			rv = EINVAL;
			prop_object_release(array);
			goto out;
		}

	} else {
		pkgd = xbps_find_pkg_in_dict_by_name(dict,
		    "packages", pkgname);
		if (pkgd == NULL) {
			if (errno && errno != ENOENT) {
				rv = errno;
				goto out;
			}

			newpkg = true;
			pkgd = prop_dictionary_create();
			if (!prop_dictionary_set_cstring_nocopy(pkgd,
			    "pkgname", pkgname)) {
				rv = EINVAL;
				prop_object_release(pkgd);
				goto out;
			}
		}
		array = prop_dictionary_get(dict, "packages");
		if (array == NULL) {
			rv = EINVAL;
			if (newpkg)
				prop_object_release(pkgd);
			goto out;
		}
		if ((rv = set_new_state(pkgd, state)) != 0) {
			if (newpkg)
				prop_object_release(pkgd);
			goto out;
		}
		if (newpkg && !xbps_add_obj_to_array(array, pkgd)) {
			rv = EINVAL;
			prop_object_release(pkgd);
			goto out;
		}
	}

	if (!prop_dictionary_externalize_to_zfile(dict, plist)) {
		rv = errno;
		xbps_dbg_printf("[pkgstate] cannot write plist '%s': %s\n",
		    plist, strerror(errno));
	}

out:
	if (dict)
		prop_object_release(dict);
	if (plist)
		free(plist);

	return rv;
}
