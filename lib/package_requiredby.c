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

static int
add_pkg_into_reqby(prop_dictionary_t pkgd, const char *pkgver)
{
	prop_array_t reqby;
	prop_string_t reqstr;
	bool alloc = false;

	assert(prop_object_type(pkgd) == PROP_TYPE_DICTIONARY);

	if ((reqby = prop_dictionary_get(pkgd, "requiredby")) == NULL) {
		alloc = true;
		if ((reqby = prop_array_create()) == NULL)
			return ENOMEM;
	}

	/* the entry already exists, do nothing */
	if (xbps_match_string_in_array(reqby, pkgver))
		return 0;

	reqstr = prop_string_create_cstring(pkgver);
	if (reqstr == NULL) {
		if (alloc)
			prop_object_release(reqby);

		return ENOMEM;
	}

	if (!xbps_add_obj_to_array(reqby, reqstr)) {
		if (alloc)
			prop_object_release(reqby);

		prop_object_release(reqstr);
		return EINVAL;
	}

	if (!alloc)
		return 0;

	if (!xbps_add_obj_to_dict(pkgd, reqby, "requiredby")) {
		if (alloc)
			prop_object_release(reqby);

		return EINVAL;
	}

	return 0;
}

static int
remove_pkg_from_reqby(prop_object_t obj, void *arg, bool *loop_done)
{
	prop_array_t reqby;
	const char *pkgname = arg;

	(void)loop_done;

	reqby = prop_dictionary_get(obj, "requiredby");
	if (reqby == NULL || prop_array_count(reqby) == 0)
		return 0;

	if (xbps_match_pkgname_in_array(reqby, pkgname)) {
		if (!xbps_remove_pkgname_from_array(reqby, pkgname))
			return EINVAL;
	}

	return 0;
}

int HIDDEN
xbps_requiredby_pkg_remove(const char *pkgname)
{
	assert(pkgname != NULL);
	return xbps_pkgdb_foreach_cb(remove_pkg_from_reqby, __UNCONST(pkgname));
}

int HIDDEN
xbps_requiredby_pkg_add(struct xbps_handle *xhp, prop_dictionary_t pkgd)
{
	prop_array_t pkg_rdeps;
	prop_object_t obj, pkgd_pkgdb;
	prop_object_iterator_t iter;
	const char *pkgver, *str;
	int rv = 0;

	assert(prop_object_type(pkgd) == PROP_TYPE_DICTIONARY);

	prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	pkg_rdeps = prop_dictionary_get(pkgd, "run_depends");
	if (pkg_rdeps == NULL || prop_array_count(pkg_rdeps) == 0)
		return EINVAL;

	iter = prop_array_iterator(pkg_rdeps);
	if (iter == NULL)
		return ENOMEM;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		str = prop_string_cstring_nocopy(obj);
		if (str == NULL) {
			rv = EINVAL;
			break;
		}
		xbps_dbg_printf("%s: adding reqby entry for %s\n", __func__, str);

		pkgd_pkgdb = xbps_find_virtualpkg_conf_in_array_by_pattern(
		    xhp->pkgdb, str);
		if (pkgd_pkgdb == NULL) {
			pkgd_pkgdb =
			    xbps_find_virtualpkg_in_array_by_pattern(
			    xhp->pkgdb, str);
			if (pkgd_pkgdb == NULL) {
				rv = ENOENT;
				xbps_dbg_printf("%s: couldnt find `%s' "
				     "entry in pkgdb\n", __func__, str);
				break;
			}
		}
		rv = add_pkg_into_reqby(pkgd_pkgdb, pkgver);
		if (rv != 0)
			break;
	}
	prop_object_iterator_release(iter);

	return rv;
}
