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

static bool
remove_obj_from_array(xbps_array_t array, const char *str, int mode)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *curname, *pkgdep;
	char curpkgname[XBPS_NAME_SIZE];
	unsigned int idx = 0;
	bool found = false;

	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);

	iter = xbps_array_iterator(array);
	if (iter == NULL)
		return false;

	while ((obj = xbps_object_iterator_next(iter))) {
		if (mode == 0) {
			/* exact match, obj is a string */
			if (xbps_string_equals_cstring(obj, str)) {
				found = true;
				break;
			}
		} else if (mode == 1) {
			/* match by pkgname, obj is a string */
			pkgdep = xbps_string_cstring_nocopy(obj);
			if (!xbps_pkg_name(curpkgname, sizeof(curpkgname), pkgdep))
				break;

			if (strcmp(curpkgname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 2) {
			/* match by pkgname, obj is a dictionary  */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &curname);
			if (strcmp(curname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 3) {
			/* match by pkgver, obj is a dictionary */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curname);
			if (strcmp(curname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 4) {
			/* match by pattern, obj is a dictionary */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curname);
			if (xbps_pkgpattern_match(curname, str)) {
				found = true;
				break;
			}
		}
		idx++;
	}
	xbps_object_iterator_release(iter);

	if (!found) {
		errno = ENOENT;
		return false;
	}

	xbps_array_remove(array, idx);
	return true;
}

bool
xbps_remove_string_from_array(xbps_array_t array, const char *str)
{
	return remove_obj_from_array(array, str, 0);
}

bool
xbps_remove_pkgname_from_array(xbps_array_t array, const char *str)
{
	return remove_obj_from_array(array, str, 1);
}

bool HIDDEN
xbps_remove_pkg_from_array_by_name(xbps_array_t array, const char *str)
{
	return remove_obj_from_array(array, str, 2);
}

bool HIDDEN
xbps_remove_pkg_from_array_by_pkgver(xbps_array_t array, const char *str)
{
	return remove_obj_from_array(array, str, 3);
}

bool HIDDEN
xbps_remove_pkg_from_array_by_pattern(xbps_array_t array, const char *str)
{
	return remove_obj_from_array(array, str, 4);
}
