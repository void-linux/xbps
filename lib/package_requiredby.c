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

#include "xbps_api_impl.h"

static int
add_pkg_into_reqby(prop_dictionary_t pkgd, const char *pkgver)
{
	prop_array_t reqby;
	prop_string_t reqstr;
	bool alloc = false;

	if ((reqby = prop_dictionary_get(pkgd, "requiredby")) == NULL) {
		alloc = true;
		if ((reqby = prop_array_create()) == NULL)
			return ENOMEM;
	}

	if (xbps_match_string_in_array(reqby, pkgver)) {
		if (alloc)
			prop_object_release(reqby);

		return EEXIST;
	}

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

	if (xbps_match_pkgname_in_array(reqby, pkgname))
		if (!xbps_remove_pkgname_from_array(reqby, pkgname))
			return EINVAL;

	return 0;
}

int HIDDEN
xbps_requiredby_pkg_remove(const char *pkgname)
{
	const struct xbps_handle *xhp;
	prop_dictionary_t dict;
	char *plist;
	int rv = 0;

	assert(pkgname != NULL);

	xhp = xbps_handle_get();
	plist = xbps_xasprintf("%s/%s/%s", xhp->rootdir,
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL)
		return ENOMEM;

	if ((dict = prop_dictionary_internalize_from_zfile(plist)) == NULL) {
		free(plist);
		xbps_dbg_printf("[reqby-rm] cannot internalize "
		    "regpkgdb plist for '%s': %s\n", pkgname, strerror(errno));
		return errno;
	}

	rv = xbps_callback_array_iter_in_dict(dict, "packages",
	    remove_pkg_from_reqby, __UNCONST(pkgname));
	if (rv != 0)
		goto out;

	if (!prop_dictionary_externalize_to_zfile(dict, plist)) {
		xbps_dbg_printf("[reqby-rm] cannot externalize plist for "
		    "'%s': %s\n", pkgname, strerror(errno));
		rv = errno;
	}

out:
	prop_object_release(dict);
	free(plist);

	return rv;
}

int HIDDEN
xbps_requiredby_pkg_add(prop_array_t pkgs_array, prop_dictionary_t pkgd)
{
	prop_array_t pkg_rdeps;
	prop_object_t obj, pkgd_regpkgdb;
	prop_object_iterator_t iter;
	const char *pkgver, *str;
	int rv = 0;

	assert(pkgs_array != NULL);
	assert(pkgd != NULL);

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
		pkgd_regpkgdb =
		    xbps_find_pkg_in_array_by_pattern(pkgs_array, str);
		if (pkgd_regpkgdb == NULL)
			return EINVAL;

		rv = add_pkg_into_reqby(pkgd_regpkgdb, pkgver);
		if (rv != 0 && rv != EEXIST)
			break;
	}
	prop_object_iterator_release(iter);

	return rv;
}
