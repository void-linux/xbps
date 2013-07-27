/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
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
bool HIDDEN
xbps_add_obj_to_dict(xbps_dictionary_t dict, xbps_object_t obj,
		       const char *key)
{
	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(obj != NULL);
	assert(key != NULL);

	if (!xbps_dictionary_set(dict, key, obj)) {
		xbps_object_release(dict);
		errno = EINVAL;
		return false;
	}

	xbps_object_release(obj);
	return true;
}

bool HIDDEN
xbps_add_obj_to_array(xbps_array_t array, xbps_object_t obj)
{
	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);
	assert(obj != NULL);

	if (!xbps_array_add(array, obj)) {
		xbps_object_release(array);
		errno = EINVAL;
		return false;
	}

	xbps_object_release(obj);
	return true;
}

int
xbps_callback_array_iter(struct xbps_handle *xhp,
	xbps_array_t array,
	int (*fn)(struct xbps_handle *, xbps_object_t, void *, bool *),
	void *arg)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	int rv = 0;
	bool loop_done = false;

	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);
	assert(fn != NULL);

	iter = xbps_array_iterator(array);
	if (iter == NULL)
		return ENOMEM;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		rv = (*fn)(xhp, obj, arg, &loop_done);
		if (rv != 0 || loop_done)
			break;
	}
	xbps_object_iterator_release(iter);

	return rv;
}

int
xbps_callback_array_iter_in_dict(struct xbps_handle *xhp,
	xbps_dictionary_t dict,
	const char *key,
	int (*fn)(struct xbps_handle *, xbps_object_t, void *, bool *),
	void *arg)
{
	xbps_object_t obj;
	xbps_array_t array;
	unsigned int i;
	int rv = 0;
	bool cbloop_done = false;

	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(xhp != NULL);
	assert(key != NULL);
	assert(fn != NULL);

	array = xbps_dictionary_get(dict, key);
	for (i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		if (obj == NULL)
			continue;
		rv = (*fn)(xhp, obj, arg, &cbloop_done);
		if (rv != 0 || cbloop_done)
			break;
	}

	return rv;
}

xbps_object_iterator_t
xbps_array_iter_from_dict(xbps_dictionary_t dict, const char *key)
{
	xbps_array_t array;

	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(key != NULL);

	array = xbps_dictionary_get(dict, key);
	if (xbps_object_type(array) != XBPS_TYPE_ARRAY) {
		errno = EINVAL;
		return NULL;
	}

	return xbps_array_iterator(array);
}

static int
array_replace_dict(xbps_array_t array,
		   xbps_dictionary_t dict,
		   const char *str,
		   bool bypattern)
{
	xbps_object_t obj;
	unsigned int i;
	const char *curpkgver;
	char *curpkgname;

	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);
	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(str != NULL);

	for (i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		if (obj == NULL)
			continue;
		if (bypattern) {
			/* pkgpattern match */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curpkgver);
			if (xbps_pkgpattern_match(curpkgver, str)) {
				if (!xbps_array_set(array, i, dict))
					return EINVAL;

				return 0;
			}
		} else {
			/* pkgname match */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curpkgver);
			curpkgname = xbps_pkg_name(curpkgver);
			assert(curpkgname);
			if (strcmp(curpkgname, str) == 0) {
				if (!xbps_array_set(array, i, dict)) {
					free(curpkgname);
					return EINVAL;
				}
				free(curpkgname);
				return 0;
			}
			free(curpkgname);
		}
	}
	/* no match */
	return ENOENT;
}

int HIDDEN
xbps_array_replace_dict_by_name(xbps_array_t array,
				xbps_dictionary_t dict,
				const char *pkgver)
{
	return array_replace_dict(array, dict, pkgver, false);
}

int HIDDEN
xbps_array_replace_dict_by_pattern(xbps_array_t array,
				   xbps_dictionary_t dict,
				   const char *pattern)
{
	return array_replace_dict(array, dict, pattern, true);
}
