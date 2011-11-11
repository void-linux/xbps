/*-
 * Copyright (c) 2011 Juan Romero Pardines.
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
#include <string.h>
#include <xbps_api.h>
#include "defs.h"

void
state_cb(struct xbps_state_cb_data *xscd)
{
	prop_dictionary_t pkgd;
	const char *opkgver, *state_descr, *fetchstr;
	char *pkgname;

	if (xscd->desc != NULL && xscd->pkgver == NULL && xscd->err == 0) {
		printf("\n%s ...\n", xscd->desc);
		return;
	}

	switch (xscd->state) {
	case XBPS_STATE_DOWNLOAD:
		printf("Downloading `%s' (from %s) ...\n",
		    xscd->pkgver, xscd->repourl);
		break;
	case XBPS_STATE_DOWNLOAD_FAIL:
		state_descr = "failed to download binary package";
		break;
	case XBPS_STATE_VERIFY:
		printf("Checking `%s' integrity ...\n", xscd->binpkg_fname);
		break;
	case XBPS_STATE_VERIFY_FAIL:
		state_descr = "failed to verify binary package SHA256";
		break;
	case XBPS_STATE_REMOVE:
		printf("Removing `%s' ...\n", xscd->pkgver);
		break;
	case XBPS_STATE_REMOVE_FAIL:
		state_descr = "failed to remove package";
		break;
	case XBPS_STATE_PURGE:
		printf("Purging `%s' ...\n", xscd->pkgver);
		break;
	case XBPS_STATE_PURGE_FAIL:
		state_descr = "failed to purge package";
		break;
	case XBPS_STATE_CONFIGURE:
		printf("Configuring `%s' ...\n", xscd->pkgver);
		break;
	case XBPS_STATE_CONFIGURE_FAIL:
		state_descr = "failed to configure package";
		break;
	case XBPS_STATE_REGISTER_FAIL:
		state_descr = "failed to register package";
		break;
	case XBPS_STATE_REGISTER:
	case XBPS_STATE_INSTALL:
		break;
	case XBPS_STATE_UPDATE:
		pkgname = xbps_pkg_name(xscd->pkgver);
		if (pkgname == NULL) {
			xbps_error_printf("%s: failed to alloc pkgname!\n",
			    __func__);
			exit(EXIT_FAILURE);
		}
		pkgd = xbps_find_pkg_dict_installed(pkgname, false);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &opkgver);
		prop_object_release(pkgd);
		free(pkgname);
		printf("Updating `%s' to `%s'...\n", opkgver, xscd->pkgver);
		break;
	case XBPS_STATE_UPDATE_FAIL:
		state_descr = "failed to update package";
		break;
	case XBPS_STATE_UNPACK:
		printf("Unpacking `%s' (from ../%s) ...\n",
		    xscd->pkgver, xscd->binpkg_fname);
		break;
	case XBPS_STATE_UNPACK_FAIL:
		state_descr = "failed to unpack binary package";
		break;
	case XBPS_STATE_REPOSYNC:
		printf("Synchronizing index for `%s' ...\n",
		    xscd->repourl);
		break;
	case XBPS_STATE_REPOSYNC_FAIL:
		fetchstr = xbps_fetch_error_string();
		xbps_error_printf("Failed to sync index: %s\n",
		    fetchstr ? fetchstr : strerror(xscd->err));
		return;
	default:
		xbps_dbg_printf("%s: unknown state %d %s\n",
		    xscd->pkgver, xscd->state, xscd->desc);
		break;
	}

	if (state_descr != NULL && xscd->err != 0)
		xbps_error_printf("%s: %s: %s\n",
		    xscd->pkgver, state_descr, strerror(xscd->err));
}
