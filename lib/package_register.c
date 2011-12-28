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
xbps_register_pkg(prop_dictionary_t pkgrd, bool flush)
{
	struct xbps_handle *xhp;
	prop_array_t array;
	prop_dictionary_t pkgd = NULL;
	prop_array_t provides, reqby;
	const char *pkgname, *version, *desc, *pkgver;
	int rv = 0;
	bool autoinst = false;

	assert(prop_object_type(pkgrd) == PROP_TYPE_DICTIONARY);

	xhp = xbps_handle_get();

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

	pkgd = xbps_regpkgdb_get_pkgd(pkgname, false);
	if (pkgd == NULL) {
		rv = ENOENT;
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
		if ((rv = xbps_requiredby_pkg_add(xhp, pkgrd)) != 0) {
			prop_object_release(pkgd);
			goto out;
		}
	}
	array = prop_dictionary_get(xhp->regpkgdb, "packages");
	rv = xbps_array_replace_dict_by_name(array, pkgd, pkgname);
	if (rv != 0)
		goto out;
	if (flush)
		rv = xbps_regpkgdb_update(xhp, true);

out:
	if (pkgd != NULL)
		prop_object_release(pkgd);

	if (rv != 0) {
		xbps_set_cb_state(XBPS_STATE_REGISTER_FAIL,
		    rv, pkgname, version,
		    "%s: failed to register package: %s",
		    pkgver, strerror(rv));
	}

	return rv;
}

int
xbps_unregister_pkg(const char *pkgname, const char *version, bool flush)
{
	struct xbps_handle *xhp;

	assert(pkgname != NULL);

	xbps_set_cb_state(XBPS_STATE_UNREGISTER, 0, pkgname, version, NULL);

	if (!xbps_regpkgdb_remove_pkgd(pkgname)) {
		xbps_set_cb_state(XBPS_STATE_UNREGISTER_FAIL,
		    errno, pkgname, version,
		    "%s: failed to unregister package: %s",
		    pkgname, strerror(errno));
		return errno;
	}
	if (flush) {
		xhp = xbps_handle_get();
		return xbps_regpkgdb_update(xhp, true);
	}

	return 0;
}
