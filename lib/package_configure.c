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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

/**
 * @file lib/package_configure.c
 * @brief Package configuration routines
 * @defgroup configure Package configuration functions
 *
 * Configure a package or all packages. Only packages in XBPS_PKG_STATE_UNPACKED
 * state will be processed (unless overriden). Package configuration steps:
 *  - Its <b>post-install</b> target in the INSTALL script will be executed.
 *  - Its state will be changed to XBPS_PKG_STATE_INSTALLED if previous step
 *    ran successful.
 *
 * @note
 * If the \a XBPS_FLAG_FORCE is set through xbps_set_flags(), the package
 * (or packages) will be reconfigured even if its state is
 * XBPS_PKG_STATE_INSTALLED.
 */

int
xbps_configure_all_pkgs(void)
{
	prop_dictionary_t d;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname, *version;
	int rv = 0;

	if ((d = xbps_regpkgdb_dictionary_get()) == NULL)
		return errno;

	iter = xbps_get_array_iter_from_dict(d, "packages");
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		rv = xbps_configure_pkg(pkgname, version, true, false);
		if (rv != 0)
			break;
	}
	prop_object_iterator_release(iter);
out:
	xbps_regpkgdb_dictionary_release();

	return rv;
}

int
xbps_configure_pkg(const char *pkgname,
		   const char *version,
		   bool check_state,
		   bool update)
{
	prop_dictionary_t pkgd;
	const char *lver, *rootdir = xbps_get_rootdir();
	char *buf;
	int rv = 0, flags = xbps_get_flags();
	pkg_state_t state = 0;
	bool reconfigure = false;

	assert(pkgname != NULL);

	if (check_state) {
		rv = xbps_get_pkg_state_installed(pkgname, &state);
		if (rv != 0)
			return EINVAL;

		if (state == XBPS_PKG_STATE_INSTALLED) {
			if ((flags & XBPS_FLAG_FORCE) == 0)
				return 0;

			reconfigure = true;
		} else if (state != XBPS_PKG_STATE_UNPACKED)
			return EINVAL;
	
		pkgd = xbps_find_pkg_dict_installed(pkgname, false);
		prop_dictionary_get_cstring_nocopy(pkgd, "version", &lver);
		prop_object_release(pkgd);
	} else {
		lver = version;
	}

	xbps_printf("%sonfiguring package `%s-%s' ...\n",
	    reconfigure ? "Rec" : "C", pkgname, lver);

	buf = xbps_xasprintf(".%s/metadata/%s/INSTALL",
	    XBPS_META_PATH, pkgname);
	if (buf == NULL)
		return ENOMEM;

	if (chdir(rootdir) == -1) {
		xbps_dbg_printf("%s: [configure] chdir to '%s' returned %s\n",
		    pkgname, rootdir, strerror(errno));
		free(buf);
		return EINVAL;
	}

	if (access(buf, X_OK) == 0) {
		if (xbps_file_exec(buf, "post",
		    pkgname, lver, update ? "yes" : "no", NULL) != 0) {
			free(buf);
			xbps_error_printf("%s: post install script error: %s\n",
			    pkgname, strerror(errno));
			return errno;
		}
	} else {
		if (errno != ENOENT) {
			free(buf);
			return errno;
		}
	}
	free(buf);

	return xbps_set_pkg_state_installed(pkgname, XBPS_PKG_STATE_INSTALLED);
}
