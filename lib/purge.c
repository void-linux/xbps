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
#include <dirent.h>

#include <xbps_api.h>

static int	remove_pkg_metadata(const char *);

int SYMEXPORT
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
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname)) {
			rv = errno;
			break;
		}
		if ((rv = xbps_get_pkg_state_dictionary(obj, &state)) != 0)
			break;
		if (state != XBPS_PKG_STATE_CONFIG_FILES)
			continue;
		if ((rv = xbps_purge_pkg(pkgname, false)) != 0)
			break;
	}
	prop_object_iterator_release(iter);
out:
	xbps_regpkgs_dictionary_release();

	return rv;
}

/*
 * Purge a package that is currently in "config-files" state.
 * This removes configuration files if they weren't modified,
 * removes metadata files and fully unregisters the package.
 */
int SYMEXPORT
xbps_purge_pkg(const char *pkgname, bool check_state)
{
	prop_dictionary_t dict;
	char *path;
	int rv = 0, flags;
	pkg_state_t state = 0;

	assert(pkgname != NULL);
	flags = xbps_get_flags();

	if (check_state) {
		/*
		 * Skip packages that aren't in "config-files" state.
		 */
		if ((rv = xbps_get_pkg_state_installed(pkgname, &state)) != 0)
			return rv;

		if (state != XBPS_PKG_STATE_CONFIG_FILES)
			return 0;
	}

	/*
	 * Remove unmodified configuration files.
	 */
	path = xbps_xasprintf("%s/%s/metadata/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, pkgname, XBPS_PKGFILES);
	if (path == NULL)
                return errno;

	dict = prop_dictionary_internalize_from_file(path);
	if (dict == NULL) {
		free(path);
		return errno;
	}
	free(path);
	if ((rv = xbps_remove_pkg_files(dict, "conf_files")) != 0) {
		prop_object_release(dict);
		return rv;
	}
	/* Also try to remove empty dirs used in conf_files */
	if ((rv = xbps_remove_pkg_files(dict, "dirs")) != 0) {
		prop_object_release(dict);
		return rv;
	}
	prop_object_release(dict);

	/*
	 * Remove metadata dir and unregister package.
	 */
	if ((rv = remove_pkg_metadata(pkgname)) == 0) {
		if ((rv = xbps_unregister_pkg(pkgname)) == 0)
			printf("Package %s has been purged successfully.\n",
			    pkgname);
	}

	return rv;
}

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

		if ((rv = unlink(path)) == -1) {
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
