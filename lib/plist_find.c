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

static prop_dictionary_t
get_pkg_in_array(prop_array_t array, const char *str, bool virtual)
{
	prop_object_t obj = NULL;
	const char *pkgver, *dpkgn;
	size_t i;
	bool found = false;

	for (i = 0; i < prop_array_count(array); i++) {
		obj = prop_array_get(array, i);
		if (virtual) {
			/*
			 * Check if package pattern matches
			 * any virtual package version in dictionary.
			 */
			if (xbps_pkgpattern_version(str))
				found = xbps_match_virtual_pkg_in_dict(obj, str, true);
			else
				found = xbps_match_virtual_pkg_in_dict(obj, str, false);

			if (found)
				break;
		} else if (xbps_pkgpattern_version(str)) {
			/* ,atch by pattern against pkgver */
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &pkgver))
				continue;
			if (xbps_pkgpattern_match(pkgver, str)) {
				found = true;
				break;
			}
		} else if (xbps_pkg_version(str)) {
			/* match by exact pkgver */
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &pkgver))
				continue;
			if (strcmp(str, pkgver) == 0) {
				found = true;
				break;
			}
		} else {
			/* match by pkgname */
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &dpkgn))
				continue;
			if (strcmp(dpkgn, str) == 0) {
				found = true;
				break;
			}
		}
	}
	return found ? obj : NULL;
}

prop_dictionary_t HIDDEN
xbps_find_pkg_in_array(prop_array_t a, const char *s)
{
	assert(prop_object_type(a) == PROP_TYPE_ARRAY);
	assert(s);

	return get_pkg_in_array(a, s, false);
}

prop_dictionary_t HIDDEN
xbps_find_virtualpkg_in_array(struct xbps_handle *x,
			      prop_array_t a,
			      const char *s)
{
	prop_dictionary_t pkgd;
	const char *vpkg;
	bool bypattern = false;

	assert(x);
	assert(prop_object_type(a) == PROP_TYPE_ARRAY);
	assert(s);

	if (xbps_pkgpattern_version(s))
		bypattern = true;

	if ((vpkg = vpkg_user_conf(x, s, bypattern))) {
		if ((pkgd = get_pkg_in_array(a, vpkg, true)))
			return pkgd;
	}

	return get_pkg_in_array(a, s, true);
}
