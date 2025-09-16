/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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
#include <assert.h>
#include <xbps.h>
#include "defs.h"

#include <libintl.h>
#include <locale.h>
#define _(STRING) gettext(STRING)

int
state_cb(const struct xbps_state_cb_data *xscd, void *cbdata UNUSED)
{
	xbps_dictionary_t pkgd;
	const char *instver, *newver;
	char pkgname[XBPS_NAME_SIZE];
	int rv = 0;
	bool slog = false;

	if ((xscd->xhp->flags & XBPS_FLAG_DISABLE_SYSLOG) == 0) {
		slog = true;
		openlog("xbps-install", 0, LOG_USER);
	}

	switch (xscd->state) {
	/* notifications */
	case XBPS_STATE_TRANS_DOWNLOAD:
		printf(_("\n[*] Downloading packages\n"));
		break;
	case XBPS_STATE_TRANS_VERIFY:
		printf(_("\n[*] Verifying package integrity\n"));
		break;
	case XBPS_STATE_TRANS_FILES:
		printf(_("\n[*] Collecting package files\n"));
		break;
	case XBPS_STATE_TRANS_RUN:
		printf(_("\n[*] Unpacking packages\n"));
		break;
	case XBPS_STATE_TRANS_CONFIGURE:
		printf(_("\n[*] Configuring unpacked packages\n"));
		break;
	case XBPS_STATE_PKGDB:
		printf(_("[*] pkgdb upgrade in progress, please wait...\n"));
		break;
	case XBPS_STATE_REPOSYNC:
		printf(_("[*] Updating repository `%s' ...\n"), xscd->arg);
		break;
	case XBPS_STATE_TRANS_ADDPKG:
		if (xscd->xhp->flags & XBPS_FLAG_VERBOSE)
			printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_VERIFY:
		printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_FILES:
		printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_CONFIG_FILE:
		if (xscd->desc != NULL)
			printf("%s\n", xscd->desc);
		break;
	case XBPS_STATE_REMOVE:
		printf(_("%s: removing ...\n"), xscd->arg);
		break;
	case XBPS_STATE_CONFIGURE:
		printf(_("%s: configuring ...\n"), xscd->arg);
		break;
	case XBPS_STATE_CONFIGURE_DONE:
		/* empty */
		break;
	case XBPS_STATE_UNPACK:
		printf(_("%s: unpacking ...\n"), xscd->arg);
		break;
	case XBPS_STATE_INSTALL:
	case XBPS_STATE_DOWNLOAD:
		/* empty */
		break;
	case XBPS_STATE_UPDATE:
		if (!xbps_pkg_name(pkgname, sizeof(pkgname), xscd->arg)) {
			abort();
		}
		newver = xbps_pkg_version(xscd->arg);
		pkgd = xbps_pkgdb_get_pkg(xscd->xhp, pkgname);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &instver);
		printf(_("%s: updating to %s ...\n"), instver, newver);
		if (slog) {
			syslog(LOG_NOTICE, _("%s: updating to %s ... "
			    "(rootdir: %s)\n"), instver, newver,
			    xscd->xhp->rootdir);
		}
		break;
	/* success */
	case XBPS_STATE_REMOVE_FILE:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE:
		if (xscd->xhp->flags & XBPS_FLAG_VERBOSE)
			printf("%s\n", xscd->desc);
		else {
			printf("%s\n", xscd->desc);
			printf("\033[1A\033[K");
		}
		break;
	case XBPS_STATE_INSTALL_DONE:
		printf(_("%s: installed successfully.\n"), xscd->arg);
		if (slog) {
			syslog(LOG_NOTICE, _("Installed `%s' successfully "
			    "(rootdir: %s)."), xscd->arg,
			    xscd->xhp->rootdir);
		}
		break;
	case XBPS_STATE_UPDATE_DONE:
		printf(_("%s: updated successfully.\n"), xscd->arg);
		if (slog) {
			syslog(LOG_NOTICE, _("Updated `%s' successfully "
			    "(rootdir: %s)."), xscd->arg,
			    xscd->xhp->rootdir);
		}
		break;
	case XBPS_STATE_REMOVE_DONE:
		printf(_("%s: removed successfully.\n"), xscd->arg);
		if (slog) {
			syslog(LOG_NOTICE, _("Removed `%s' successfully "
			    "(rootdir: %s)."), xscd->arg,
			    xscd->xhp->rootdir);
		}
		break;
	case XBPS_STATE_PKGDB_DONE:
		printf(_("The pkgdb file has been upgraded successfully, please reexec "
		    "the command again.\n"));
		break;
	case XBPS_STATE_REPO_KEY_IMPORT:
		printf("%s\n", xscd->desc);
		printf(_("Fingerprint: %s\n"), xscd->arg);
		rv = yesno(_("Do you want to import this public key?"));
		break;
	case XBPS_STATE_UNPACK_FILE_PRESERVED:
		printf("%s\n", xscd->desc);
		break;
	/* errors */
	case XBPS_STATE_TRANS_FAIL:
	case XBPS_STATE_UNPACK_FAIL:
	case XBPS_STATE_UPDATE_FAIL:
	case XBPS_STATE_CONFIGURE_FAIL:
	case XBPS_STATE_REMOVE_FAIL:
	case XBPS_STATE_VERIFY_FAIL:
	case XBPS_STATE_FILES_FAIL:
	case XBPS_STATE_DOWNLOAD_FAIL:
	case XBPS_STATE_REPOSYNC_FAIL:
	case XBPS_STATE_CONFIG_FILE_FAIL:
		xbps_error_printf("%s\n", xscd->desc);
		if (slog) {
			syslog(LOG_ERR, "%s", xscd->desc);
		}
		break;
	case XBPS_STATE_REMOVE_FILE_FAIL:
	case XBPS_STATE_REMOVE_FILE_HASH_FAIL:
	case XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL:
		/* Ignore errors due to not empty directories or directories being a mount point */
		if (xscd->err == ENOTEMPTY || xscd->err == EBUSY)
			return 0;

		xbps_error_printf("%s\n", xscd->desc);
		if (slog) {
			syslog(LOG_ERR, "%s", xscd->desc);
		}
		break;
	default:
		if (xscd->desc)
			printf("%s\n", xscd->desc);
		else
			xbps_dbg_printf(_("%s: unknown state %d\n"), xscd->arg, xscd->state);

		break;
	}

	return rv;
}
