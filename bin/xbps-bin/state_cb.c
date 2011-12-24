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
#include <syslog.h>
#include <xbps_api.h>
#include "defs.h"

void
state_cb(const struct xbps_state_cb_data *xscd, void *cbdata)
{
	const struct xbps_handle *xhp = xbps_handle_get();
	prop_dictionary_t pkgd;
	const char *version;

	(void)cbdata;

	if (xhp->syslog_enabled)
		openlog("xbps-bin", LOG_CONS, LOG_USER);

	switch (xscd->state) {
	/* notifications */
	case XBPS_STATE_TRANS_DOWNLOAD:
		printf("[*] Downloading binary packages\n");
		break;
	case XBPS_STATE_TRANS_VERIFY:
		printf("[*] Verifying binary package integrity\n");
		break;
	case XBPS_STATE_TRANS_RUN:
		printf("[*] Running transaction tasks\n");
		break;
	case XBPS_STATE_TRANS_CONFIGURE:
		printf("[*] Configuring unpacked packages\n");
		break;
	case XBPS_STATE_REPOSYNC:
	case XBPS_STATE_DOWNLOAD:
	case XBPS_STATE_VERIFY:
	case XBPS_STATE_CONFIG_FILE:
		if (xscd->desc != NULL)
			printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_REMOVE:
		printf("Removing `%s-%s' ...\n", xscd->pkgname, xscd->version);
		break;
	case XBPS_STATE_CONFIGURE:
		printf("Configuring `%s-%s' ...\n", xscd->pkgname,
		    xscd->version);
		break;
	case XBPS_STATE_REGISTER:
	case XBPS_STATE_UNREGISTER:
		/* empty */
		break;
	case XBPS_STATE_UNPACK:
		printf("Unpacking `%s-%s' ...\n", xscd->pkgname, xscd->version);
		break;
	case XBPS_STATE_INSTALL:
		printf("Installing `%s-%s' ...\n",
		    xscd->pkgname, xscd->version);
		break;
	case XBPS_STATE_UPDATE:
		pkgd = xbps_find_pkg_dict_installed(xscd->pkgname, false);
		prop_dictionary_get_cstring_nocopy(pkgd, "version", &version);
		prop_object_release(pkgd);
		printf("Updating `%s' (`%s' to `%s') ...\n", xscd->pkgname,
		    version, xscd->version);
		break;
	/* success */
	case XBPS_STATE_REMOVE_FILE:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE:
		if (xhp->flags & XBPS_FLAG_VERBOSE)
			printf("%s\n", xscd->desc);
		else {
			printf("%s\n", xscd->desc);
			printf("\033[1A\033[K");
		}
		break;
	case XBPS_STATE_INSTALL_DONE:
		printf("Installed `%s-%s' successfully.\n",
		    xscd->pkgname, xscd->version);
		if (xhp->syslog_enabled)
			syslog(LOG_NOTICE, "Installed `%s-%s' successfully "
			    "(rootdir: %s).", xscd->pkgname, xscd->version,
			    xhp->rootdir);
		break;
	case XBPS_STATE_UPDATE_DONE:
		printf("Updated `%s' to `%s' successfully.\n",
		    xscd->pkgname, xscd->version);
		if (xhp->syslog_enabled)
			syslog(LOG_NOTICE, "Updated `%s' to `%s' successfully "
			    "(rootdir: %s).", xscd->pkgname, xscd->version,
			    xhp->rootdir);
		break;
	case XBPS_STATE_REMOVE_DONE:
		printf("Removed `%s-%s' successfully.\n",
		    xscd->pkgname, xscd->version);
		if (xhp->syslog_enabled)
			syslog(LOG_NOTICE, "Removed `%s-%s' successfully "
			    "(rootdir: %s).", xscd->pkgname, xscd->version,
			    xhp->rootdir);
		break;
	/* errors */
	case XBPS_STATE_UNPACK_FAIL:
	case XBPS_STATE_UPDATE_FAIL:
	case XBPS_STATE_CONFIGURE_FAIL:
	case XBPS_STATE_REGISTER_FAIL:
	case XBPS_STATE_UNREGISTER_FAIL:
	case XBPS_STATE_REMOVE_FAIL:
	case XBPS_STATE_VERIFY_FAIL:
	case XBPS_STATE_DOWNLOAD_FAIL:
	case XBPS_STATE_REPOSYNC_FAIL:
	case XBPS_STATE_CONFIG_FILE_FAIL:
		xbps_error_printf("%s\n", xscd->desc);
		if (xhp->syslog_enabled)
			syslog(LOG_ERR, "%s", xscd->desc);
		break;
	case XBPS_STATE_REMOVE_FILE_FAIL:
	case XBPS_STATE_REMOVE_FILE_HASH_FAIL:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL:
		/* Ignore errors due to not empty directories */
		if (xscd->err == ENOTEMPTY)
			return;

		xbps_error_printf("%s\n", xscd->desc);
		if (xhp->syslog_enabled)
			syslog(LOG_ERR, "%s", xscd->desc);
		break;
	default:
		xbps_dbg_printf("unknown state %d\n", xscd->state);
		break;
	}
}
