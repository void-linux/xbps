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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

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
xbps_register_pkg(struct xbps_handle *xhp, prop_dictionary_t pkgrd, bool flush)
{
	prop_dictionary_t pkgd;
	prop_array_t provides, reqby;
	char outstr[64];
	time_t t;
	struct tm *tmp;
	const char *pkgname, *version, *desc, *pkgver;
	char *buf, *sha256;
	int rv = 0;
	bool autoinst = false;

	assert(prop_object_type(pkgrd) == PROP_TYPE_DICTIONARY);

	prop_dictionary_get_cstring_nocopy(pkgrd, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkgrd, "version", &version);
	prop_dictionary_get_cstring_nocopy(pkgrd, "short_desc", &desc);
	prop_dictionary_get_cstring_nocopy(pkgrd, "pkgver", &pkgver);
	prop_dictionary_get_bool(pkgrd, "automatic-install", &autoinst);
	provides = prop_dictionary_get(pkgrd, "provides");
	reqby = prop_dictionary_get(pkgrd, "requiredby");

	xbps_set_cb_state(xhp, XBPS_STATE_REGISTER, 0, pkgname, version, NULL);

	assert(pkgname != NULL);
	assert(version != NULL);
	assert(desc != NULL);
	assert(pkgver != NULL);

	pkgd = xbps_pkgdb_get_pkgd(xhp, pkgname, false);
	if (pkgd == NULL) {
		rv = ENOENT;
		goto out;
	}
	if (!prop_dictionary_set_cstring_nocopy(pkgd,
	    "version", version)) {
		xbps_dbg_printf(xhp, "%s: invalid version for %s\n",
		    __func__, pkgname);
		rv = EINVAL;
		goto out;
	}
	if (!prop_dictionary_set_cstring_nocopy(pkgd,
	    "pkgver", pkgver)) {
		xbps_dbg_printf(xhp, "%s: invalid pkgver for %s\n",
		    __func__, pkgname);
		rv = EINVAL;
		goto out;
	}
	if (!prop_dictionary_set_cstring_nocopy(pkgd,
	    "short_desc", desc)) {
		xbps_dbg_printf(xhp, "%s: invalid short_desc for %s\n",
		    __func__, pkgname);
		rv = EINVAL;
		goto out;
	}
	if (reqby && !prop_dictionary_set(pkgd, "requiredby", reqby)) {
		xbps_dbg_printf(xhp, "%s: invalid requiredby for %s\n",
		    __func__, pkgname);
		rv = EINVAL;
		goto out;
	}
	prop_dictionary_get_bool(pkgd, "automatic-install", &autoinst);
	if (xhp->flags & XBPS_FLAG_INSTALL_AUTO)
		autoinst = true;
	else if (xhp->flags & XBPS_FLAG_INSTALL_MANUAL)
		autoinst = false;

	if (!prop_dictionary_set_bool(pkgd,
	    "automatic-install", autoinst)) {
		xbps_dbg_printf(xhp, "%s: invalid autoinst for %s\n",
		    __func__, pkgname);
		rv = EINVAL;
		goto out;
	}
	/*
	 * Set the "install-date" object to know the pkg installation date.
	 */
	t = time(NULL);
	if ((tmp = localtime(&t)) == NULL) {
		xbps_dbg_printf(xhp, "%s: localtime failed: %s\n",
		    pkgname, strerror(errno));
		rv = EINVAL;
		goto out;
	}
	if (strftime(outstr, sizeof(outstr)-1, "%F %R %Z", tmp) == 0) {
		xbps_dbg_printf(xhp, "%s: strftime failed: %s\n",
		    pkgname, strerror(errno));
		rv = EINVAL;
		goto out;
	}
	if (!prop_dictionary_set_cstring(pkgd, "install-date", outstr)) {
		xbps_dbg_printf(xhp, "%s: install-date set failed!\n", pkgname);
		rv = EINVAL;
		goto out;
	}

	if (provides) {
		if (!prop_dictionary_set(pkgd, "provides", provides)) {
			xbps_dbg_printf(xhp,
			    "%s: invalid provides for %s\n",
			    __func__, pkgname);
			rv = EINVAL;
			goto out;
		}
	}
	/*
	 * Add the requiredby objects for dependent packages.
	 */
	if (pkgrd && xbps_pkg_has_rundeps(pkgrd)) {
		if ((rv = xbps_requiredby_pkg_add(xhp, pkgrd)) != 0) {
			xbps_dbg_printf(xhp,
			    "%s: requiredby add failed for %s\n",
			    __func__, pkgname);
			goto out;
		}
	}
	/*
	 * Create a hash for the pkg's metafile.
	 */
	buf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, pkgname);
	sha256 = xbps_file_hash(buf);
	assert(sha256);
	prop_dictionary_set_cstring(pkgd, "metafile-sha256", sha256);
	free(sha256);
	free(buf);
	/*
	 * Remove unneeded objs from pkg dictionary.
	 */
	prop_dictionary_remove(pkgd, "remove-and-update");
	prop_dictionary_remove(pkgd, "transaction");

	if (!xbps_pkgdb_replace_pkgd(xhp, pkgd, pkgname, false, flush)) {
		xbps_dbg_printf(xhp,
		    "%s: failed to replace pkgd dict for %s\n",
		    __func__, pkgname);
		goto out;
	}
out:
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REGISTER_FAIL,
		    rv, pkgname, version,
		    "%s: failed to register package: %s",
		    pkgver, strerror(rv));
	}

	return rv;
}

int
xbps_unregister_pkg(struct xbps_handle *xhp,
		    const char *pkgname,
		    const char *version,
		    bool flush)
{
	assert(pkgname != NULL);

	xbps_set_cb_state(xhp, XBPS_STATE_UNREGISTER, 0, pkgname, version, NULL);

	if (!xbps_pkgdb_remove_pkgd(xhp, pkgname, false, flush)) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNREGISTER_FAIL,
		    errno, pkgname, version,
		    "%s: failed to unregister package: %s",
		    pkgname, strerror(errno));
		return errno;
	}
	return 0;
}
