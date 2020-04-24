/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/plist_find.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
bool
xbps_match_virtual_pkg_in_array(xbps_array_t a, const char *str)
{
	if (xbps_pkgpattern_version(str)) {
		if (xbps_match_pkgdep_in_array(a, str) ||
		    xbps_match_pkgpattern_in_array(a, str))
		return true;
	} else if (xbps_pkg_version(str)) {
		return xbps_match_string_in_array(a, str);
	} else {
		return xbps_match_pkgname_in_array(a, str);
	}
	return false;
}

bool
xbps_match_virtual_pkg_in_dict(xbps_dictionary_t d, const char *str)
{
	xbps_array_t provides;

	assert(xbps_object_type(d) == XBPS_TYPE_DICTIONARY);

	if ((provides = xbps_dictionary_get(d, "provides")))
		return xbps_match_virtual_pkg_in_array(provides, str);

	return false;
}

bool
xbps_match_any_virtualpkg_in_rundeps(xbps_array_t rundeps,
				     xbps_array_t provides)
{
	xbps_object_t obj, obj2;
	xbps_object_iterator_t iter, iter2;
	const char *vpkgver, *pkgpattern;

	iter = xbps_array_iterator(provides);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		vpkgver = xbps_string_cstring_nocopy(obj);
		iter2 = xbps_array_iterator(rundeps);
		assert(iter2);
		while ((obj2 = xbps_object_iterator_next(iter2))) {
			pkgpattern = xbps_string_cstring_nocopy(obj2);
			if (xbps_pkgpattern_match(vpkgver, pkgpattern)) {
				xbps_object_iterator_release(iter2);
				xbps_object_iterator_release(iter);
				return true;
			}
		}
		xbps_object_iterator_release(iter2);
	}
	xbps_object_iterator_release(iter);

	return false;
}

static bool
match_string_in_array(xbps_array_t array, const char *str, int mode)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *pkgdep;
	char pkgname[XBPS_NAME_SIZE];
	bool found = false;

	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);
	assert(str != NULL);

	iter = xbps_array_iterator(array);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		if (mode == 0) {
			/* match by string */
			if (xbps_string_equals_cstring(obj, str)) {
				found = true;
				break;
			}
		} else if (mode == 1) {
			/* match by pkgname against pkgver */
			pkgdep = xbps_string_cstring_nocopy(obj);
			if (!xbps_pkg_name(pkgname, XBPS_NAME_SIZE, pkgdep))
				break;
			if (strcmp(pkgname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 2) {
			/* match by pkgver against pkgname */
			pkgdep = xbps_string_cstring_nocopy(obj);
			if (!xbps_pkg_name(pkgname, XBPS_NAME_SIZE, str))
				break;
			if (strcmp(pkgname, pkgdep) == 0) {
				found = true;
				break;
			}
		} else if (mode == 3) {
			/* match pkgpattern against pkgdep */
			pkgdep = xbps_string_cstring_nocopy(obj);
			if (xbps_pkgpattern_match(pkgdep, str)) {
				found = true;
				break;
			}
		} else if (mode == 4) {
			/* match pkgdep against pkgpattern */
			pkgdep = xbps_string_cstring_nocopy(obj);
			if (xbps_pkgpattern_match(str, pkgdep)) {
				found = true;
				break;
			}
		}
	}
	xbps_object_iterator_release(iter);

	return found;
}

bool
xbps_match_string_in_array(xbps_array_t array, const char *str)
{
	return match_string_in_array(array, str, 0);
}

bool
xbps_match_pkgname_in_array(xbps_array_t array, const char *pkgname)
{
	return match_string_in_array(array, pkgname, 1);
}

bool
xbps_match_pkgver_in_array(xbps_array_t array, const char *pkgver)
{
	return match_string_in_array(array, pkgver, 2);
}

bool
xbps_match_pkgpattern_in_array(xbps_array_t array, const char *pattern)
{
	return match_string_in_array(array, pattern, 3);
}

bool
xbps_match_pkgdep_in_array(xbps_array_t array, const char *pkgver)
{
	return match_string_in_array(array, pkgver, 4);
}
