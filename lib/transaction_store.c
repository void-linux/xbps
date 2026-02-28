/*-
 * Copyright (c) 2014-2020 Juan Romero Pardines.
 * Copyright (c) 2026 Duncan Overbruck <mail@duncano.de>.
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

#include <stdlib.h>

#include "xbps.h"
#include "xbps_api_impl.h"

static int
transaction_replace_package(xbps_array_t pkgs, const char *pkgname, const char *pkgver)
{
	xbps_dictionary_t pkgd;
	const char *curpkgver = NULL;
	int r;

	pkgd = xbps_find_pkg_in_array(pkgs, pkgname, 0);
	if (!pkgd)
		return 1;

	/* compare version stored in transaction vs current */
	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &curpkgver))
		xbps_unreachable();

	r = xbps_cmpver(pkgver, curpkgver);
	if (r == 0)
		return 0;
	// XXX: should it be possible to replace with lower version?
	if (r == -1)
		return 0;

	if (!xbps_remove_pkg_from_array_by_pkgver(pkgs, curpkgver))
		xbps_unreachable();

	xbps_dbg_printf("[trans] replaced %s with %s\n", curpkgver, pkgver);

	return 1;
}

// XXX: the automatic self replace is weird, there should be a better way
// than having to add and remove it from the metdata...
static int
package_set_self_replace(xbps_dictionary_t pkgd, const char *pkgname)
{
	char buf[XBPS_NAME_SIZE + sizeof(">=0") - 1];
	xbps_array_t replaces;

	replaces = xbps_dictionary_get(pkgd, "replaces");
	if (!replaces) {
		replaces = xbps_array_create();
		if (!replaces)
			return xbps_error_oom();
	} else {
		xbps_object_retain(replaces);
	}

	snprintf(buf,sizeof(buf), "%s>=0", pkgname);
	if (!xbps_array_add_cstring(replaces, buf)) {
		xbps_object_release(replaces);
		return xbps_error_oom();
	}

	if (!xbps_dictionary_set(pkgd, "replaces", replaces)) {
		xbps_object_release(replaces);
		return xbps_error_oom();
	}

	xbps_object_release(replaces);
	return 0;
}

int HIDDEN
transaction_package_set_action(xbps_dictionary_t pkgd, xbps_trans_type_t ttype)
{
	uint8_t v;
	switch (ttype) {
	case XBPS_TRANS_INSTALL:
	case XBPS_TRANS_UPDATE:
	case XBPS_TRANS_CONFIGURE:
	case XBPS_TRANS_REMOVE:
	case XBPS_TRANS_REINSTALL:
	case XBPS_TRANS_HOLD:
	case XBPS_TRANS_DOWNLOAD:
		break;
	case XBPS_TRANS_UNKNOWN:
		return -EINVAL;
	}
	v = ttype;
	if (!xbps_dictionary_set_uint8(pkgd, "transaction", v))
		return xbps_error_oom();
	return 0;
}

static int
package_set_auto_install(xbps_dictionary_t pkgd, bool value)
{
	if (!value)
		return 0;
	if (!xbps_dictionary_set_bool(pkgd, "automatic-install", true))
		return xbps_error_oom();
	return 0;
}

int HIDDEN
transaction_store(struct xbps_handle *xhp, xbps_dictionary_t pkgrd,
    xbps_trans_type_t ttype, bool autoinst, bool replace)
{
	xbps_array_t pkgs;
	xbps_dictionary_t pkgd;
	const char *pkgver = NULL, *pkgname = NULL, *repo = NULL;
	int r;

	assert(xhp);
	assert(pkgrd);

	if (!xbps_dictionary_get_cstring_nocopy(pkgrd, "pkgver", &pkgver))
		xbps_unreachable();
	if (!xbps_dictionary_get_cstring_nocopy(pkgrd, "pkgname", &pkgname))
		xbps_unreachable();

	pkgs = xbps_dictionary_get(xhp->transd, "packages");
	if (!pkgs)
		xbps_unreachable();

	r = transaction_replace_package(pkgs, pkgname, pkgver);
	if (r < 0)
		return r;
	if (r == 0)
		return 0;

	pkgd = xbps_dictionary_copy_mutable(pkgrd);
	if (!pkgd) {
		xbps_object_release(pkgd);
		return xbps_error_oom();
	}

	r = transaction_package_set_action(pkgd, ttype);
	if (r < 0) {
		xbps_object_release(pkgd);
		return r;
	}
	r = package_set_auto_install(pkgd, autoinst);
	if (r < 0) {
		xbps_object_release(pkgd);
		return r;
	}
	r = package_set_self_replace(pkgd, pkgname);
	if (r < 0) {
		xbps_object_release(pkgd);
		return r;
	}

	if (replace) {
		if (!xbps_dictionary_set_bool(pkgd, "replaced", true)) {
			xbps_object_release(pkgd);
			return r;
		}
	}

	if (!xbps_array_add(pkgs, pkgd)) {
		xbps_object_release(pkgd);
		return xbps_error_oom();
	}

	xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repo);

	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_ADDPKG, 0, pkgver,
	    "Found %s in repository %s", pkgver, repo);

	xbps_dbg_printf("[trans] `%s' stored%s (%s)\n", pkgver,
	    autoinst ? " as automatic install" : "", repo);
	xbps_object_release(pkgd);

	return 0;
}
