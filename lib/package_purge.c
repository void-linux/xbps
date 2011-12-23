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
#include <dirent.h>

#include "xbps_api_impl.h"

/**
 * @file lib/package_purge.c
 * @brief Package purging routines
 * @defgroup purge Package purging functions
 *
 * These functions will purge an specified package or all packages.
 * Only packages in XBPS_PKG_STATE_CONFIG_FILES state will be processed
 * (unless overriden). Package purging steps:
 *
 *  - Unmodified configuration files will be removed.
 *  - The purge action in the REMOVE script will be executed (if found).
 *  - Metadata files will be removed and package will be unregistered
 *    with xbps_unregister_pkg().
 */

static int
remove_pkg_metadata(const char *pkgname,
		    const char *version,
		    const char *pkgver,
		    const char *rootdir)
{
	struct dirent *dp;
	DIR *dirp;
	char *metadir, *path;
	int rv = 0;

	assert(pkgname != NULL);
	assert(rootdir != NULL);

	metadir = xbps_xasprintf("%s/%s/metadata/%s", rootdir,
	     XBPS_META_PATH, pkgname);
	if (metadir == NULL)
		return ENOMEM;

	dirp = opendir(metadir);
	if (dirp == NULL) {
		free(metadir);
		return errno;
	}

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		path = xbps_xasprintf("%s/%s", metadir, dp->d_name);
		if (path == NULL) {
			(void)closedir(dirp);
			free(metadir);
			return ENOMEM;
		}

		if (unlink(path) == -1) {
			xbps_set_cb_state(XBPS_STATE_PURGE_FAIL,
			    errno, pkgname, version,
			    "%s: [purge] failed to remove metafile `%s': %s",
			    pkgver, path, strerror(errno));
		}
		free(path);
	}
	(void)closedir(dirp);
	rv = rmdir(metadir);
	free(metadir);

	return rv;
}

static int
purge_pkgs_cb(prop_object_t obj, void *arg, bool *done)
{
	const char *pkgname;

	(void)arg;
	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	return xbps_purge_pkg(pkgname, true);
}
int
xbps_purge_packages(void)
{
	return xbps_regpkgdb_foreach_pkg_cb(purge_pkgs_cb, NULL);
}

int
xbps_purge_pkg(const char *pkgname, bool check_state)
{
	struct xbps_handle *xhp;
	prop_dictionary_t dict, pkgd;
	const char *version, *pkgver;
	char *buf;
	int rv = 0;
	pkg_state_t state;

	assert(pkgname != NULL);
	xhp = xbps_handle_get();

	/*
	 * Firstly let's get the pkg dictionary from regpkgdb.
	 */
	pkgd = xbps_find_pkg_in_dict_by_name(xhp->regpkgdb_dictionary,
	    "packages", pkgname);
	if (pkgd == NULL) {
		xbps_dbg_printf("[purge] %s: missing pkg dictionary (%s)\n",
		    pkgname, strerror(errno));
		return errno;
	}
	prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(pkgd, "version", &version);
	xbps_set_cb_state(XBPS_STATE_PURGE, 0, pkgname, version, NULL);

	if (check_state) {
		/*
		 * Skip packages that aren't in "config-files" state.
		 */
		if ((rv = xbps_pkg_state_dictionary(pkgd, &state)) != 0)
			return rv;
		if (state != XBPS_PKG_STATE_CONFIG_FILES) {
			xbps_dbg_printf("[purge] %s not in config-files "
			    "state.\n", pkgname);
			return rv;
		}
	}
	/*
	 * Remove unmodified configuration files.
	 */
	dict = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (dict == NULL) {
		xbps_set_cb_state(XBPS_STATE_PURGE_FAIL,
		    errno, pkgname, version,
		    "%s: [purge] failed to read metafile `%s': %s",
		    pkgver, XBPS_PKGFILES, strerror(errno));
		if (errno != ENOENT)
			return errno;
	} else {
		if (prop_dictionary_get(dict, "conf_files")) {
			rv = xbps_remove_pkg_files(dict, "conf_files", pkgver);
			if (rv != 0) {
				prop_object_release(dict);
				xbps_set_cb_state(XBPS_STATE_PURGE_FAIL,
				    rv, pkgname, version,
				    "%s: [purge] failed to remove "
				    "configuration files: %s",
				    pkgver, strerror(rv));
				return rv;
			}
		}
		prop_object_release(dict);
	}
	/*
	 * Execute the purge action in REMOVE script (if found).
	 */
	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(XBPS_STATE_PURGE_FAIL,
		    rv, pkgname, version,
		    "%s: [purge] failed to chdir to rootdir `%s': %s",
		    pkgver, xhp->rootdir, strerror(rv));
		return rv;
	}
	buf = xbps_xasprintf("%s/metadata/%s/REMOVE", XBPS_META_PATH, pkgname);
	if (buf == NULL) {
		rv = ENOMEM;
		return rv;
	}
	if (access(buf, X_OK) == 0) {
		rv = xbps_file_exec(buf, "purge", pkgname, version,
		    "no", xhp->conffile, NULL);
		if (rv != 0) {
			free(buf);
			if (errno && errno != ENOENT) {
				xbps_set_cb_state(XBPS_STATE_PURGE_FAIL,
				    errno, pkgname, version,
				    "%s: [purge] REMOVE script failed to "
				    "execute purge ACTION: %s",
				    pkgver, strerror(errno));
				return rv;
			}
		}
	}
	free(buf);
	/*
	 * Remove metadata dir and unregister package.
	 */
	if ((rv = remove_pkg_metadata(pkgname, version, pkgver,
	    xhp->rootdir)) != 0) {
		xbps_set_cb_state(XBPS_STATE_PURGE_FAIL,
		    rv, pkgname, version,
		    "%s: [purge] failed to remove metadata files: %s",
		    pkgver, strerror(rv));
		if (rv != ENOENT)
			return rv;
	}
	if ((rv = xbps_unregister_pkg(pkgname, version)) != 0)
		return rv;

	xbps_set_cb_state(XBPS_STATE_PURGE_DONE, 0, pkgname, version, NULL);

	return rv;
}
