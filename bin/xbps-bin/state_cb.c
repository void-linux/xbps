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
#include <errno.h>
#include <xbps_api.h>
#include "defs.h"

void
state_cb(const struct xbps_state_cb_data *xscd, void *cbdata)
{
	const struct xbps_handle *xhp = xbps_handle_get();
	prop_dictionary_t pkgd;
	const char *pkgver;

	(void)cbdata;

	switch (xscd->state) {
	case XBPS_STATE_DOWNLOAD:
	case XBPS_STATE_VERIFY:
	case XBPS_STATE_REMOVE:
	case XBPS_STATE_PURGE:
	case XBPS_STATE_CONFIGURE:
	case XBPS_STATE_REGISTER:
	case XBPS_STATE_UNREGISTER:
	case XBPS_STATE_INSTALL:
	case XBPS_STATE_UNPACK:
	case XBPS_STATE_REPOSYNC:
	case XBPS_STATE_CONFIG_FILE:
		printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_UPDATE:
		pkgd = xbps_find_pkg_dict_installed(xscd->pkgname, false);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		prop_object_release(pkgd);
		printf("Updating `%s' to `%s-%s'...\n", pkgver,
		    xscd->pkgname, xscd->version);
		break;
	case XBPS_STATE_REMOVE_FILE:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE:
		if (xhp->flags & XBPS_FLAG_VERBOSE)
			printf("%s\n", xscd->desc);
		else {
			printf("%s\n", xscd->desc);
			printf("\033[1A\033[K");
		}
		break;
	case XBPS_STATE_UNPACK_FAIL:
	case XBPS_STATE_UPDATE_FAIL:
	case XBPS_STATE_CONFIGURE_FAIL:
	case XBPS_STATE_REGISTER_FAIL:
	case XBPS_STATE_UNREGISTER_FAIL:
	case XBPS_STATE_PURGE_FAIL:
	case XBPS_STATE_REMOVE_FAIL:
	case XBPS_STATE_VERIFY_FAIL:
	case XBPS_STATE_DOWNLOAD_FAIL:
	case XBPS_STATE_REPOSYNC_FAIL:
	case XBPS_STATE_CONFIG_FILE_FAIL:
		xbps_error_printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_REMOVE_FILE_FAIL:
	case XBPS_STATE_REMOVE_FILE_HASH_FAIL:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL:
		/* Ignore errors due to not empty directories */
		if (xscd->err == ENOTEMPTY)
			return;

		xbps_error_printf("%s\n", xscd->desc);
		break;
	default:
		xbps_dbg_printf("unknown state %d\n", xscd->state);
		break;
	}
}
