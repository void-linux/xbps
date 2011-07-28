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

#include <string.h>
#include <xbps_api.h>
#include "defs.h"

void
transaction_cb(struct xbps_transaction_cb_data *xtcd)
{
	if (xtcd->desc != NULL && xtcd->pkgver == NULL) {
		printf("\n%s ...\n", xtcd->desc);
		return;
	}

	switch (xtcd->state) {
	case XBPS_TRANS_STATE_DOWNLOAD:
		printf("Downloading `%s' (from %s) ...\n",
		    xtcd->pkgver, xtcd->repourl);
		break;
	case XBPS_TRANS_STATE_VERIFY:
		printf("Checking `%s' integrity ...\n", xtcd->binpkg_fname);
		break;
	case XBPS_TRANS_STATE_REMOVE:
		printf("Removing `%s' ...\n", xtcd->pkgver);
		break;
	case XBPS_TRANS_STATE_PURGE:
		printf("Purging `%s' ...\n", xtcd->pkgver);
		break;
	case XBPS_TRANS_STATE_CONFIGURE:
		printf("Configuring `%s' ...\n", xtcd->pkgver);
		break;
	case XBPS_TRANS_STATE_REGISTER:
	case XBPS_TRANS_STATE_INSTALL:
		break;
	case XBPS_TRANS_STATE_UPDATE:
		printf("Updating `%s' ...\n", xtcd->pkgver);
		break;
	case XBPS_TRANS_STATE_UNPACK:
		printf("Unpacking `%s' (from ../%s) ...\n",
		    xtcd->pkgver, xtcd->binpkg_fname);
		break;
	case XBPS_TRANS_STATE_REPOSYNC:
		printf("Syncing repository pkg-index for `%s' ...\n",
		    xtcd->repourl);
		break;
	default:
		xbps_dbg_printf("%s: unknown transaction state %d %s\n",
		    xtcd->pkgver, xtcd->state, xtcd->desc);
		break;
	}
}

void
transaction_err_cb(struct xbps_transaction_cb_data *xtcd)
{
	const char *state_descr = NULL;

	switch (xtcd->state) {
	case XBPS_TRANS_STATE_DOWNLOAD:
		state_descr = "failed to download binary package";
		break;
	case XBPS_TRANS_STATE_VERIFY:
		state_descr = "failed to verify binary package SHA256";
		break;
	case XBPS_TRANS_STATE_REMOVE:
		state_descr = "failed to remove package";
		break;
	case XBPS_TRANS_STATE_PURGE:
		state_descr = "failed to purge package";
		break;
	case XBPS_TRANS_STATE_CONFIGURE:
		state_descr = "failed to configure package";
		break;
	case XBPS_TRANS_STATE_UPDATE:
		state_descr = "failed to update package";
		break;
	case XBPS_TRANS_STATE_UNPACK:
		state_descr = "failed to unpack binary package";
		break;
	case XBPS_TRANS_STATE_REGISTER:
		state_descr = "failed to register package";
		break;
	case XBPS_TRANS_STATE_REPOSYNC:
		xbps_error_printf("Failed to sync pkg-index: %s\n",
		    xtcd->err);
		return;
	default:
		state_descr = "unknown transaction state";
		break;
	}

	xbps_error_printf("%s: %s: %s\n",
	    xtcd->pkgver, state_descr, strerror(xtcd->err));
}
