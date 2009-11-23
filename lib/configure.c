/*-
 * Copyright (c) 2009 Juan Romero Pardines.
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

/*
 * Configure all packages currently in unpacked state.
 */
int SYMEXPORT
xbps_configure_all_pkgs(void)
{
	prop_dictionary_t d;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname, *version;
	int rv = 0;
	pkg_state_t state = 0;

	d = xbps_prepare_regpkgdb_dict();
	if (d == NULL)
		return ENODEV;

	iter = xbps_get_array_iter_from_dict(d, "packages");
	if (iter == NULL)
		return ENOENT;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname)) {
			rv = errno;
			break;
		}
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "version", &version)) {
			rv = errno;
			break;
		}
		if ((rv = xbps_get_pkg_state_dictionary(obj, &state)) != 0)
			break;
		if (state != XBPS_PKG_STATE_UNPACKED)
			continue;
		if ((rv = xbps_configure_pkg(pkgname, version, false)) != 0)
			break;
	}
	prop_object_iterator_release(iter);

	return rv;
}

/*
 * Configure a package that is in unpacked state. This runs the
 * post INSTALL action if required and updates package state to
 * to installed.
 */
int SYMEXPORT
xbps_configure_pkg(const char *pkgname, const char *version, bool check_state)
{
	prop_dictionary_t pkgd;
	const char *rootdir, *lver;
	char *buf;
	int rv = 0, flags = 0;
	pkg_state_t state = 0;
	bool reconfigure = false;

	assert(pkgname != NULL);

	rootdir = xbps_get_rootdir();
	flags = xbps_get_flags();

	if (check_state) {
		if ((rv = xbps_get_pkg_state_installed(pkgname, &state)) != 0)
			return rv;

		if (state == XBPS_PKG_STATE_INSTALLED) {
			if ((flags & XBPS_FLAG_FORCE) == 0)
				return 0;

			reconfigure = true;
		} else if (state != XBPS_PKG_STATE_UNPACKED)
			return EINVAL;
	
		pkgd = xbps_find_pkg_installed_from_plist(pkgname);
		if (pkgd == NULL)
			return ENOENT;

		if (!prop_dictionary_get_cstring_nocopy(pkgd,
		    "version", &lver)) {
			prop_object_release(pkgd);
			return errno;
		}
		prop_object_release(pkgd);
	} else {
		lver = version;
	}

	printf("%sonfiguring package %s-%s...\n",
	    reconfigure ? "Rec" : "C", pkgname, lver);

	buf = xbps_xasprintf(".%s/metadata/%s/INSTALL",
	    XBPS_META_PATH, pkgname);
	if (buf == NULL)
		return errno;

	if (strcmp(rootdir, "") == 0)
		rootdir = "/";

	if (chdir(rootdir) == -1)
		return errno;

	if (access(buf, X_OK) == 0) {
		if ((rv = xbps_file_chdir_exec(rootdir, buf, "post",
		     pkgname, lver, NULL)) != 0) {
			free(buf);
			printf("%s: post INSTALL action returned: %s\n",
			    pkgname, strerror(errno));
			return rv;
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
