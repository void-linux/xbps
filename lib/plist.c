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
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/plist.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
bool
xbps_add_obj_to_dict(prop_dictionary_t dict, prop_object_t obj,
		       const char *key)
{
	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(obj != NULL);
	assert(key != NULL);

	if (!prop_dictionary_set(dict, key, obj)) {
		prop_object_release(dict);
		errno = EINVAL;
		return false;
	}

	prop_object_release(obj);
	return true;
}

bool
xbps_add_obj_to_array(prop_array_t array, prop_object_t obj)
{
	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(obj != NULL);

	if (!prop_array_add(array, obj)) {
		prop_object_release(array);
		errno = EINVAL;
		return false;
	}

	prop_object_release(obj);
	return true;
}

int
xbps_callback_array_iter(struct xbps_handle *xhp,
	prop_array_t array,
	int (*fn)(struct xbps_handle *, prop_object_t, void *, bool *),
	void *arg)
{
	prop_object_t obj;
	prop_object_iterator_t iter;
	int rv = 0;
	bool loop_done = false;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(fn != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return ENOMEM;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		rv = (*fn)(xhp, obj, arg, &loop_done);
		if (rv != 0 || loop_done)
			break;
	}
	prop_object_iterator_release(iter);

	return rv;
}

int
xbps_callback_array_iter_in_dict(struct xbps_handle *xhp,
	prop_dictionary_t dict,
	const char *key,
	int (*fn)(struct xbps_handle *, prop_object_t, void *, bool *),
	void *arg)
{
	prop_object_t obj;
	prop_array_t array;
	size_t i;
	int rv = 0;
	bool cbloop_done = false;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(xhp != NULL);
	assert(key != NULL);
	assert(fn != NULL);

	array = prop_dictionary_get(dict, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY)
		return EINVAL;

	for (i = 0; i < prop_array_count(array); i++) {
		obj = prop_array_get(array, i);
		if (obj == NULL)
			continue;
		rv = (*fn)(xhp, obj, arg, &cbloop_done);
		if (rv != 0 || cbloop_done)
			break;
	}

	return rv;
}

int
xbps_callback_array_iter_reverse(struct xbps_handle *xhp,
	prop_array_t array,
	int (*fn)(struct xbps_handle *, prop_object_t, void *, bool *),
	void *arg)
{
	prop_object_t obj;
	unsigned int cnt;
	int rv;
	bool loop_done = false;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(fn != NULL);
	assert(xhp != NULL);

	if ((cnt = prop_array_count(array)) == 0)
		return 0;

	while (cnt--) {
		obj = prop_array_get(array, cnt);
		if (obj == NULL)
			continue;
		rv = (*fn)(xhp, obj, arg, &loop_done);
		if (rv != 0 || loop_done)
			break;
	}

	return rv;
}

int
xbps_callback_array_iter_reverse_in_dict(struct xbps_handle *xhp,
	prop_dictionary_t dict,
	const char *key,
	int (*fn)(struct xbps_handle *, prop_object_t, void *, bool *),
	void *arg)
{
	prop_array_t array;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(key != NULL);
	assert(fn != NULL);
	assert(xhp != NULL);

	array = prop_dictionary_get(dict, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		xbps_dbg_printf(xhp, "invalid key '%s' for dictionary", key);
		return EINVAL;
	}

	return xbps_callback_array_iter_reverse(xhp, array, fn, arg);
}

prop_object_iterator_t
xbps_array_iter_from_dict(prop_dictionary_t dict, const char *key)
{
	prop_array_t array;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(key != NULL);

	array = prop_dictionary_get(dict, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		errno = EINVAL;
		return NULL;
	}

	return prop_array_iterator(array);
}

static int
array_replace_dict(prop_array_t array,
		   prop_dictionary_t dict,
		   const char *str,
		   bool bypattern)
{
	prop_object_t obj;
	size_t i;
	const char *curpkgname;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(str != NULL);

	for (i = 0; i < prop_array_count(array); i++) {
		obj = prop_array_get(array, i);
		if (obj == NULL)
			continue;
		if (bypattern) {
			/* pkgpattern match */
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curpkgname);
			if (xbps_pkgpattern_match(curpkgname, str)) {
				if (!prop_array_set(array, i, dict))
					return EINVAL;

				return 0;
			}
		} else {
			/* pkgname match */
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &curpkgname);
			if (strcmp(curpkgname, str) == 0) {
				if (!prop_array_set(array, i, dict))
					return EINVAL;

				return 0;
			}
		}
	}
	/* no match */
	return ENOENT;
}

int
xbps_array_replace_dict_by_name(prop_array_t array,
				prop_dictionary_t dict,
				const char *pkgname)
{
	return array_replace_dict(array, dict, pkgname, false);
}

int
xbps_array_replace_dict_by_pattern(prop_array_t array,
				   prop_dictionary_t dict,
				   const char *pattern)
{
	return array_replace_dict(array, dict, pattern, true);
}

prop_dictionary_t
xbps_pkgd_from_metadir(struct xbps_handle *xhp, const char *name)
{
	prop_dictionary_t pkgd, plistd = NULL;
	const char *savedpkgname;
	char *plistf;

	assert(name != NULL);

	savedpkgname = name;
	plistf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, name);

	if (access(plistf, R_OK) == -1) {
		pkgd = xbps_find_virtualpkg_dict_installed(xhp, name, false);
		if (pkgd == NULL)
			pkgd = xbps_find_pkg_dict_installed(xhp, name, false);

		if (pkgd != NULL) {
			free(plistf);
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "pkgname", &savedpkgname);
			plistf = xbps_xasprintf("%s/.%s.plist",
			    xhp->metadir, savedpkgname);
		}
	}

	plistd = prop_dictionary_internalize_from_zfile(plistf);
	free(plistf);
	if (plistd == NULL) {
		xbps_dbg_printf(xhp, "cannot read from metadata %s for %s: %s\n",
		    plistf, savedpkgname, strerror(errno));
		return NULL;
	}

	return plistd;
}
