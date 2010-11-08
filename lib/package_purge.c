/*-
 * Copyright (c) 2009-2010 Juan Romero Pardines.
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

/**
 * @file lib/purge.c
 * @brief Package purging routines
 * @defgroup purge Package purging functions
 *
 * These functions will purge an specified package or all packages.
 * Only packages in XBPS_PKG_STATE_CONFIG_FILES state will be processed
 * (unless overriden). Package purging steps:
 *
 *  - Unmodified configuration files and directories containing them
 *    will be removed (if empty).
 *  - Its metadata directory and all its files will be removed.
 *  - It will be unregistered from the installed packages database with
 *    xbps_unregister_pkg().
 */

static int
remove_pkg_metadata(const char *pkgname)
{
	struct dirent *dp;
	DIR *dirp;
	char *metadir, *path;
	int flags = 0, rv = 0;

	assert(pkgname != NULL);

	flags = xbps_get_flags();

	metadir = xbps_xasprintf("%s/%s/metadata/%s", xbps_get_rootdir(),
	     XBPS_META_PATH, pkgname);
	if (metadir == NULL)
		return errno;

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
			return -1;
		}

		if (unlink(path) == -1) {
			if (flags & XBPS_FLAG_VERBOSE)
				printf("WARNING: can't remove %s (%s)\n",
				    pkgname, strerror(errno));
		}
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
	pkg_state_t state = 0;

	if ((d = xbps_regpkgs_dictionary_init()) == NULL)
		return errno;

	iter = xbps_get_array_iter_from_dict(d, "packages");
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if ((rv = xbps_get_pkg_state_dictionary(obj, &state)) != 0)
			break;
		if (state != XBPS_PKG_STATE_CONFIG_FILES)
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		if ((rv = xbps_purge_pkg(pkgname, false)) != 0)
			break;
	}
	prop_object_iterator_release(iter);
out:
	xbps_regpkgs_dictionary_release();

	return rv;
}

int
xbps_purge_pkg(const char *pkgname, bool check_state)
{
	prop_dictionary_t dict, pkgd;
	int rv = 0, flags;
	pkg_state_t state = 0;

	assert(pkgname != NULL);
	flags = xbps_get_flags();

	/*
	 * Firstly let's get the pkg dictionary from regpkgdb.
	 */
	if ((dict = xbps_regpkgs_dictionary_init()) == NULL)
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
	if ((rv = xbps_remove_pkg_files(dict, "conf_files")) != 0) {
		prop_object_release(dict);
		goto out;
	}
	prop_object_release(dict);

	/*
	 * Remove metadata dir and unregister package.
	 */
	if ((rv = remove_pkg_metadata(pkgname)) == 0) {
		if ((rv = xbps_unregister_pkg(pkgname)) == 0) {
			if (flags & XBPS_FLAG_VERBOSE) {
				printf("Package %s purged "
				    "successfully.\n", pkgname);
			}
		}
	}

out:
	xbps_regpkgs_dictionary_release();
	return rv;
}
