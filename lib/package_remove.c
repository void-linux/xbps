/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>

#include "xbps_api_impl.h"

/**
 * @file lib/package_remove.c
 * @brief Package removal routines
 * @defgroup pkg_remove Package removal functions
 *
 * These functions will remove a package or only a subset of its
 * files. Package removal steps:
 *  -# Its <b>pre-remove</b> target specified in the REMOVE script
 *     will be executed.
 *  -# Its links, files, conf_files and dirs will be removed.
 *     Modified files (not matchings its sha256 hash) are preserved, unless
 *     XBPS_FLAG_FORCE_REMOVE_FILES flag is set via xbps_init::flags member.
 *  -# Its <b>post-remove</b> target specified in the REMOVE script
 *     will be executed.
 *  -# Its requiredby objects will be removed from the installed packages
 *     database.
 *  -# Its state will be changed to XBPS_PKG_STATE_HALF_UNPACKED.
 *  -# Its <b>purge-remove</b> target specified in the REMOVE script
 *     will be executed.
 *  -# Its package metadata directory will be removed.
 *  -# Package will be unregistered from package database.
 *
 * @note
 *  -# If a package is going to be updated, only steps <b>1</b> and <b>4</b>
 *     will be executed.
 *  -# If a package is going to be removed, all steps will be executed.
 *
 * The following image shows the structure of an internalized package's
 * files.plist dictionary:
 *
 * @image html images/xbps_pkg_files_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: XBPS_PKGFILES plist file.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
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

int
xbps_remove_pkg_files(prop_dictionary_t dict,
		      const char *key,
		      const char *pkgver)
{
	struct xbps_handle *xhp;
	struct stat st;
	prop_array_t array;
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *file, *sha256, *version, *curobj = NULL;
	char *path = NULL, *pkgname = NULL;
	char buf[PATH_MAX];
	int rv = 0;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(key != NULL);
	xhp = xbps_handle_get();

	array = prop_dictionary_get(dict, key);
	if ((prop_object_type(array) != PROP_TYPE_ARRAY) ||
	    prop_array_count(array) == 0)
		return 0;

	iter = xbps_array_iter_from_dict(dict, key);
	if (iter == NULL)
		return ENOMEM;

	if (strcmp(key, "files") == 0)
		curobj = "file";
	else if (strcmp(key, "conf_files") == 0)
		curobj = "configuration file";
	else if (strcmp(key, "links") == 0)
		curobj = "link";
	else if (strcmp(key, "dirs") == 0)
		curobj = "directory";

	pkgname = xbps_pkg_name(pkgver);
	version = xbps_pkg_version(pkgver);

	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "file", &file);
		path = xbps_xasprintf("%s/%s", xhp->rootdir, file);
		if (path == NULL) {
			rv = ENOMEM;
			break;
		}

		if ((strcmp(key, "files") == 0) ||
		    (strcmp(key, "conf_files") == 0)) {
			/*
			 * Check SHA256 hash in regular files and
			 * configuration files.
			 */
			prop_dictionary_get_cstring_nocopy(obj,
			    "sha256", &sha256);
			rv = xbps_file_hash_check(path, sha256);
			if (rv == ENOENT) {
				/* missing file, ignore it */
				xbps_set_cb_state(
				    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
				    rv, pkgname, version,
				    "%s: failed to check hash for %s `%s': %s",
				    pkgver, curobj, file, strerror(rv));
				free(path);
				rv = 0;
				continue;
			} else if (rv == ERANGE) {
				rv = 0;
				if ((xhp->flags &
				    XBPS_FLAG_FORCE_REMOVE_FILES) == 0) {
					xbps_set_cb_state(
					    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
					    0, pkgname, version,
					    "%s: %s `%s' SHA256 mismatch, "
					    "preserving file", pkgver,
					    curobj, file);
					free(path);
					continue;
				} else {
					xbps_set_cb_state(
					    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
					    0, pkgname, version,
					    "%s: %s `%s' SHA256 mismatch, "
					    "forcing removal", pkgver,
					    curobj, file);
				}
			} else if (rv != 0 && rv != ERANGE) {
				xbps_set_cb_state(
				    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
				    rv, pkgname, version,
				    "%s: [remove] failed to check hash for "
				    "%s `%s': %s", pkgver, curobj, file,
				    strerror(rv));
				free(path);
				break;
			}
		} else if (strcmp(key, "links") == 0) {
			/*
			 * All regular files from package were removed at this
			 * point, so we will only remove dangling symlinks.
			 */
			if (realpath(path, buf) == NULL) {
				if (errno != ENOENT) {
					free(path);
					rv = errno;
					break;
				}
			}
			if (stat(buf, &st) == 0) {
				free(path);
				continue;
			}
		}
		/*
		 * Remove the object if possible.
		 */
		if (remove(path) == -1) {
			xbps_set_cb_state(XBPS_STATE_REMOVE_FILE_FAIL,
			    errno, pkgname, version,
			    "%s: failed to remove %s `%s': %s", pkgver,
			    curobj, file, strerror(errno));
			errno = 0;
		} else {
			/* success */
			xbps_set_cb_state(XBPS_STATE_REMOVE_FILE,
			    0, pkgname, version,
			    "Removed %s `%s'", curobj, file);
		}
		free(path);
	}
	prop_object_iterator_release(iter);
	if (pkgname)
		free(pkgname);

	return rv;
}

int
xbps_remove_pkg(const char *pkgname, const char *version, bool update)
{
	struct xbps_handle *xhp;
	prop_dictionary_t pkgd = NULL;
	char *buf = NULL, *pkgver = NULL;
	int rv = 0;
	bool rmfile_exists = false;
	pkg_state_t state = 0;

	assert(pkgname != NULL);
	assert(version != NULL);

	xhp = xbps_handle_get();

	buf = xbps_xasprintf("%s/metadata/%s/REMOVE",
	    XBPS_META_PATH, pkgname);
	if (buf == NULL) {
		rv = ENOMEM;
		goto out;
	}

	pkgver = xbps_xasprintf("%s-%s", pkgname, version);
	if (pkgver == NULL) {
		rv = ENOMEM;
		goto out;
	}

	if ((rv = xbps_pkg_state_installed(pkgname, &state)) != 0)
		goto out;

	if (!update)
		xbps_set_cb_state(XBPS_STATE_REMOVE, 0, pkgname, version, NULL);

	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		   "%s: [remove] failed to chdir to rootdir `%s': %s",
		    pkgver, xhp->rootdir, strerror(rv));
		goto out;
	}
	/* If package was "half-removed", remove it fully. */
	if (state == XBPS_PKG_STATE_HALF_REMOVED)
		goto purge;
	/*
	 * Run the pre remove action.
	 */
	if (access(buf, X_OK) == 0) {
		rmfile_exists = true;
		if (xbps_file_exec(buf, "pre", pkgname, version,
		    update ? "yes" : "no", xhp->conffile, NULL) != 0) {
			xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
			    errno, pkgname, version,
			    "%s: [remove] REMOVE script failed to "
			    "execute pre ACTION: %s",
			    pkgver, strerror(errno));
			rv = errno;
			goto out;
		}
	} else {
		if (errno != ENOENT) {
			xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
			    errno, pkgname, version,
			    "%s: [remove] REMOVE script failed to "
			    "execute pre ACTION: %s",
			    pkgver, strerror(errno));
			rv = errno;
			goto out;
		}
	}
	/*
	 * If updating a package, we just need to execute the current
	 * pre-remove action target, unregister its requiredby entries and
	 * continue. Its files will be overwritten later in unpack phase.
	 */
	if (update) {
		free(pkgver);
		free(buf);
		return xbps_requiredby_pkg_remove(pkgname);
	}

	pkgd = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (pkgd) {
		/* Remove regular files */
		if ((rv = xbps_remove_pkg_files(pkgd, "files", pkgver)) != 0)
			goto out;
		/* Remove configuration files */
		if ((rv = xbps_remove_pkg_files(pkgd, "conf_files", pkgver)) != 0)
			goto out;
		/* Remove links */
		if ((rv = xbps_remove_pkg_files(pkgd, "links", pkgver)) != 0)
			goto out;
		/* Remove dirs */
		if ((rv = xbps_remove_pkg_files(pkgd, "dirs", pkgver)) != 0)
			goto out;
	}
	/*
	 * Execute the post REMOVE action if file exists and we aren't
	 * updating the package.
	 */
	if (rmfile_exists &&
	    ((rv = xbps_file_exec(buf, "post", pkgname, version, "no",
	     xhp->conffile, NULL)) != 0)) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: [remove] REMOVE script failed to execute "
		    "post ACTION: %s", pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Update the requiredby array of all required dependencies.
	 */
	if ((rv = xbps_requiredby_pkg_remove(pkgname)) != 0) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: [remove] failed to remove requiredby entries: %s",
		    pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Set package state to "half-removed".
	 */
	rv = xbps_set_pkg_state_installed(pkgname, version,
	     XBPS_PKG_STATE_HALF_REMOVED);
	if (rv != 0) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: [remove] failed to set state to half-removed: %s",
		    pkgver, strerror(rv));
		goto out;
	}

purge:
	/*
	 * Execute the purge REMOVE action if file exists.
	 */
	if (access(buf, X_OK) == 0) {
	    if ((rv = xbps_file_exec(buf, "purge", pkgname, version, "no",
	        xhp->conffile, NULL)) != 0) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: REMOVE script failed to execute "
		    "purge ACTION: %s", pkgver, strerror(rv));
		goto out;
	    }
	}
	/*
	 * Remove package metadata directory.
	 */
	rv = remove_pkg_metadata(pkgname, version, pkgver, xhp->rootdir);
	if (rv != 0) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: failed to remove metadata files: %s",
		    pkgver, strerror(rv));
		if (rv != ENOENT)
			goto out;
	}
	/*
	 * Unregister package from pkgdb.
	 */
	if ((rv = xbps_unregister_pkg(pkgname, version, false)) != 0)
		goto out;

	xbps_set_cb_state(XBPS_STATE_REMOVE_DONE,
	     0, pkgname, version, NULL);

out:
	if (buf != NULL)
		free(buf);
	if (pkgver != NULL)
		free(pkgver);
	if (pkgd != NULL)
		prop_object_release(pkgd);

	return rv;
}
