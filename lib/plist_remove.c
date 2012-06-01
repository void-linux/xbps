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
 * @file lib/plist_remove.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
static bool
remove_obj_from_array(prop_array_t array, const char *str, int mode,
		     const char *targetarch)
{
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *curname, *pkgdep, *arch;
	char *curpkgname;
	size_t idx = 0;
	bool found = false, chkarch;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return false;

	while ((obj = prop_object_iterator_next(iter))) {
		if (mode == 0) {
			/* exact match, obj is a string */
			if (prop_string_equals_cstring(obj, str)) {
				found = true;
				break;
			}
		} else if (mode == 1) {
			/* match by pkgname, obj is a string */
			pkgdep = prop_string_cstring_nocopy(obj);
			curpkgname = xbps_pkg_name(pkgdep);
			if (curpkgname == NULL)
				break;
			if (strcmp(curpkgname, str) == 0) {
				free(curpkgname);
				found = true;
				break;
			}
			free(curpkgname);
		} else if (mode == 2) {
			/* match by pkgname, obj is a dictionary  */
			chkarch = prop_dictionary_get_cstring_nocopy(obj,
			    "architecture", &arch);
			if (chkarch && !xbps_pkg_arch_match(arch, targetarch)) {
				idx++;
				continue;
			}
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &curname);
			if (strcmp(curname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 3) {
			chkarch = prop_dictionary_get_cstring_nocopy(obj,
			    "architecture", &arch);
			if (chkarch && !xbps_pkg_arch_match(arch, targetarch)) {
				idx++;
				continue;
			}
			/* match by pkgver, obj is a dictionary */
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curname);
			if (strcmp(curname, str) == 0) {
				found = true;
				break;
			}
		} else if (mode == 4) {
			chkarch = prop_dictionary_get_cstring_nocopy(obj,
			    "architecture", &arch);
			if (chkarch && !xbps_pkg_arch_match(arch, targetarch)) {
				idx++;
				continue;
			}
			/* match by pattern, obj is a dictionary */
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curname);
			if (xbps_pkgpattern_match(curname, str)) {
				found = true;
				break;
			}
		}
		idx++;
	}
	prop_object_iterator_release(iter);

	if (!found) {
		errno = ENOENT;
		return false;
	}

	prop_array_remove(array, idx);
	return true;
}

bool
xbps_remove_string_from_array(prop_array_t array, const char *str)
{
	return remove_obj_from_array(array, str, 0, NULL);
}

bool
xbps_remove_pkgname_from_array(prop_array_t array, const char *name)
{
	return remove_obj_from_array(array, name, 1, NULL);
}

bool
xbps_remove_pkg_from_array_by_name(prop_array_t array, const char *name,
				   const char *targetarch)
{
	return remove_obj_from_array(array, name, 2, targetarch);
}

bool
xbps_remove_pkg_from_array_by_pkgver(prop_array_t array, const char *pkgver,
				     const char *targetarch)
{
	return remove_obj_from_array(array, pkgver, 3, targetarch);
}

bool
xbps_remove_pkg_from_array_by_pattern(prop_array_t array, const char *p,
				      const char *targetarch)
{
	return remove_obj_from_array(array, p, 4, targetarch);
}
