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

#include <xbps_api.h>
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
remove_pkg_metadata(const char *pkgname)
{
	struct dirent *dp;
	DIR *dirp;
	char *metadir, *path;
	int rv = 0;

	assert(pkgname != NULL);

	metadir = xbps_xasprintf("%s/%s/metadata/%s", xbps_get_rootdir(),
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

		if (unlink(path) == -1)
			xbps_warn_printf("can't remove metadata file: "
			    "`%s': %s\n", dp->d_name, strerror(errno));

		free(path);
	}
	(void)closedir(dirp);
	rv = rmdir(metadir);
	free(metadir);

	return rv;
}

int
xbps_purge_all_pkgs(void)
{

	prop_dictionary_t d;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname;
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
		if ((rv = xbps_purge_pkg(pkgname, true)) != 0)
			break;
	}
	prop_object_iterator_release(iter);
out:
	xbps_regpkgdb_dictionary_release();

	return rv;
}

int
xbps_purge_pkg(const char *pkgname, bool check_state)
{
	prop_dictionary_t dict, pkgd;
	const char *version;
	char *buf;
	int rv = 0, flags = xbps_get_flags();
	pkg_state_t state;

	assert(pkgname != NULL);
	/*
	 * Firstly let's get the pkg dictionary from regpkgdb.
	 */
	if ((dict = xbps_regpkgdb_dictionary_get()) == NULL)
		return errno;

	pkgd = xbps_find_pkg_in_dict_by_name(dict, "packages", pkgname);
	if (pkgd == NULL) {
		rv = errno;
		goto out;
	}
	if (check_state) {
		/*
		 * Skip packages that aren't in "config-files" state.
		 */
		if ((rv = xbps_get_pkg_state_dictionary(pkgd, &state)) != 0)
			goto out;
		if (state != XBPS_PKG_STATE_CONFIG_FILES)
			goto out;
	}
	/*
	 * Remove unmodified configuration files.
	 */
	dict = xbps_get_pkg_dict_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (dict == NULL) {
		rv = errno;
		goto out;
	}
	if (prop_dictionary_get(dict, "conf_files")) {
		if ((rv = xbps_remove_pkg_files(dict, "conf_files")) != 0) {
			prop_object_release(dict);
			goto out;
		}
	}
	/*
	 * Execute the purge action in REMOVE script (if found).
	 */
	if (chdir(xbps_get_rootdir()) == -1) {
		rv = errno;
		prop_object_release(dict);
		xbps_error_printf("[purge] %s: cannot change to rootdir: %s.\n",
		    pkgname, strerror(rv));
		goto out;
	}
	buf = xbps_xasprintf(".%s/metadata/%s/REMOVE", XBPS_META_PATH, pkgname);
	if (buf == NULL) {
		prop_object_release(dict);
		rv = ENOMEM;
		goto out;
	}
	if (access(buf, X_OK) == 0) {
		prop_dictionary_get_cstring_nocopy(pkgd, "version", &version);

		if (xbps_file_exec(buf, "purge",
		    pkgname, version, "no", NULL) != 0) {
			free(buf);
			if (errno && errno != ENOENT) {
				rv = errno;
				xbps_error_printf("%s: purge action error in "
				    "REMOVE script: %s\n", pkgname,
				    strerror(errno));
				prop_object_release(dict);
				goto out;
			}
		}
	}
	free(buf);
	prop_object_release(dict);
	/*
	 * Remove metadata dir and unregister package.
	 */
	if ((rv = remove_pkg_metadata(pkgname)) != 0) {
		xbps_error_printf("%s: couldn't remove metadata files: %s\n",
		    pkgname, strerror(rv));
		goto out;
	}
	if ((rv = xbps_unregister_pkg(pkgname)) != 0) {
		xbps_error_printf("%s: couldn't unregister package: %s\n",
		    pkgname, strerror(rv));
		goto out;
	}
	if (flags & XBPS_FLAG_VERBOSE)
		xbps_printf("Package %s purged successfully.\n", pkgname);

out:
	xbps_regpkgdb_dictionary_release();

	return rv;
}
