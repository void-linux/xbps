/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/package_register.c
 * @brief Package registration routines
 * @defgroup pkg_register Package registration functions
 *
 * Register and unregister packages into/from the installed
 * packages database.
 */

int
xbps_register_pkg(prop_dictionary_t pkgrd)
{
	struct xbps_handle *xhp;
	prop_dictionary_t dict, pkgd;
	prop_array_t array, provides = NULL, reqby;
	const char *pkgname, *version, *desc, *pkgver;
	char *plist;
	int rv = 0;
	bool autoinst = false;

	assert(prop_object_type(pkgrd) == PROP_TYPE_DICTIONARY);

	xhp = xbps_handle_get();
	plist = xbps_xasprintf("%s/%s/%s",
	    prop_string_cstring_nocopy(xhp->rootdir),
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL)
		return ENOMEM;

	prop_dictionary_get_cstring_nocopy(pkgrd, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkgrd, "version", &version);
	prop_dictionary_get_cstring_nocopy(pkgrd, "short_desc", &desc);
	prop_dictionary_get_cstring_nocopy(pkgrd, "pkgver", &pkgver);
	prop_dictionary_get_bool(pkgrd, "automatic-install", &autoinst);
	provides = prop_dictionary_get(pkgrd, "provides");
	reqby = prop_dictionary_get(pkgrd, "requiredby");

	xbps_set_cb_state(XBPS_STATE_REGISTER, 0, pkgname, version, NULL);

	assert(pkgname != NULL);
	assert(version != NULL);
	assert(desc != NULL);
	assert(pkgver != NULL);

	if ((dict = prop_dictionary_internalize_from_zfile(plist)) != NULL) {
		pkgd = xbps_find_pkg_in_dict_by_name(dict,
		    "packages", pkgname);
		if (pkgd == NULL) {
			rv = errno;
			goto out;
		}
		if (!prop_dictionary_set_cstring_nocopy(pkgd,
		    "version", version)) {
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		if (!prop_dictionary_set_cstring_nocopy(pkgd,
		    "pkgver", pkgver)) {
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		if (!prop_dictionary_set_cstring_nocopy(pkgd,
		    "short_desc", desc)) {
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		if (reqby && !prop_dictionary_set(pkgd, "requiredby", reqby)) {
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		prop_dictionary_get_bool(pkgd, "automatic-install", &autoinst);
		if (xhp->install_reason_auto)
			autoinst = true;
		else if (xhp->install_reason_manual)
			autoinst = false;

		if (!prop_dictionary_set_bool(pkgd,
		    "automatic-install", autoinst)) {
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		if (provides) {
			if (!prop_dictionary_set(pkgd, "provides", provides)) {
				prop_object_release(pkgd);
				rv = EINVAL;
				goto out;
			}
		}
		/*
		 * Add the requiredby objects for dependent packages.
		 */
		if (pkgrd && xbps_pkg_has_rundeps(pkgrd)) {
			array = prop_dictionary_get(dict, "packages");
			if (array == NULL) {
				prop_object_release(pkgd);
				rv = EINVAL;
				goto out;
			}
			if ((rv = xbps_requiredby_pkg_add(array, pkgrd)) != 0)
				goto out;
		}
		/*
		 * Write plist file to storage.
		 */
		if (!prop_dictionary_externalize_to_zfile(dict, plist)) {
			rv = errno;
			goto out;
		}
	} else {
		free(plist);
		return ENOENT;
	}
out:
	if (rv != 0) {
		xbps_set_cb_state(XBPS_STATE_REGISTER_FAIL,
		    rv, pkgname, version,
		    "%s: failed to register package: %s",
		    pkgver, strerror(rv));
	}
	prop_object_release(dict);
	free(plist);

	return rv;
}

int
xbps_unregister_pkg(const char *pkgname, const char *version)
{
	struct xbps_handle *xhp;
	char *plist;
	int rv = 0;

	assert(pkgname != NULL);

	xbps_set_cb_state(XBPS_STATE_UNREGISTER, 0, pkgname, version, NULL);

	xhp = xbps_handle_get();
	plist = xbps_xasprintf("%s/%s/%s",
	    prop_string_cstring_nocopy(xhp->rootdir),
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL) {
		rv = ENOMEM;
		goto out;
	}
	if (!xbps_remove_pkg_dict_from_plist_by_name(pkgname, plist)) {
		rv = errno;
		goto out;
	}
out:
	if (rv != 0) {
		xbps_set_cb_state(XBPS_STATE_UNREGISTER_FAIL,
		    rv, pkgname, version,
		    "%s: failed to unregister package: %s",
		    pkgname, strerror(rv));
	}
	free(plist);
	return rv;
}
