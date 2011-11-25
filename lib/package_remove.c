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
 *  -# Its files, dirs and links will be removed. Modified files (not
 *     matching its sha256 hash) are preserved, unless
 *     XBPS_FLAG_FORCE_REMOVE_FILES flag is set via xbps_init::flags member.
 *  -# Its <b>post-remove</b> target specified in the REMOVE script
 *     will be executed.
 *  -# Its requiredby objects will be removed from the installed packages
 *     database.
 *  -# Its state will be changed to XBPS_PKG_STATE_CONFIG_FILES.
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
int
xbps_remove_pkg_files(prop_dictionary_t dict,
		      const char *key,
		      const char *pkgver)
{
	struct xbps_handle *xhp;
	prop_array_t array;
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *file, *sha256, *version, *curobj = NULL;
	char *path = NULL, *pkgname = NULL;
	int rv = 0;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(key != NULL);
	xhp = xbps_handle_get();

	array = prop_dictionary_get(dict, key);
	if (array == NULL)
		return EINVAL;
	else if (prop_array_count(array) == 0)
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
		path = xbps_xasprintf("%s/%s",
		    prop_string_cstring_nocopy(xhp->rootdir), file);
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
		}
		/*
		 * Remove the object if possible.
		 */
		if (remove(path) == -1) {
			xbps_set_cb_state(XBPS_STATE_REMOVE_FILE_FAIL,
			    errno, pkgname, version,
			    "%s: failed to remove %s `%s': %s", pkgver,
			    curobj, file, strerror(errno));
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
	prop_dictionary_t dict;
	char *buf, *pkgver;
	int rv = 0;
	bool rmfile_exists = false;

	assert(pkgname != NULL);
	assert(version != NULL);

	xhp = xbps_handle_get();
	/*
	 * Check if pkg is installed before anything else.
	 */
	if (!xbps_check_is_installed_pkg_by_name(pkgname))
		return ENOENT;

	pkgver = xbps_xasprintf("%s-%s", pkgname, version);
	if (pkgver == NULL)
		return ENOMEM;

	if (!update)
		xbps_set_cb_state(XBPS_STATE_REMOVE, 0, pkgname, version, NULL);

	buf = xbps_xasprintf(".%s/metadata/%s/REMOVE",
	    XBPS_META_PATH, pkgname);
	if (buf == NULL) {
		rv = ENOMEM;
		free(pkgver);
		return rv;
	}

	if (chdir(prop_string_cstring_nocopy(xhp->rootdir)) == -1) {
		rv = errno;
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		   "%s: [remove] failed to chdir to rootdir `%s': %s",
		    pkgver, prop_string_cstring_nocopy(xhp->rootdir),
		    strerror(rv));
		free(buf);
		free(pkgver);
		return rv;
	}

	/*
	 * Run the pre remove action.
	 */
	if (access(buf, X_OK) == 0) {
		rmfile_exists = true;
		if (xbps_file_exec(buf, "pre", pkgname, version,
		    update ? "yes" : "no", NULL) != 0) {
			xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
			    errno, pkgname, version,
			    "%s: [remove] REMOVE script failed to "
			    "execute pre ACTION: %s",
			    pkgver, strerror(errno));
			free(pkgver);
			free(buf);
			return errno;
		}
	} else {
		if (errno != ENOENT) {
			free(pkgver);
			free(buf);
			return errno;
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

	/*
	 * Remove links, files and dirs.
	 */
	dict = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (dict == NULL) {
		free(pkgver);
		free(buf);
		return errno;
	}

	/* Remove links */
	if ((rv = xbps_remove_pkg_files(dict, "links", pkgver)) != 0) {
		prop_object_release(dict);
		free(buf);
		free(pkgver);
		return rv;
	}
	/* Remove regular files */
	if ((rv = xbps_remove_pkg_files(dict, "files", pkgver)) != 0) {
		prop_object_release(dict);
		free(buf);
		free(pkgver);
		return rv;
	}
	/* Remove dirs */
	if ((rv = xbps_remove_pkg_files(dict, "dirs", pkgver)) != 0) {
		prop_object_release(dict);
		free(buf);
		free(pkgver);
		return rv;
	}
	prop_object_release(dict);

	/*
	 * Execute the post REMOVE action if file exists and we aren't
	 * updating the package.
	 */
	if (rmfile_exists &&
	    (xbps_file_exec(buf, "post", pkgname, version, "no", NULL) != 0)) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    errno, pkgname, version,
		    "%s: [remove] REMOVE script failed to execute "
		    "post ACTION: %s", pkgver, strerror(errno));
		free(buf);
		free(pkgver);
		return errno;
	}
	free(buf);

	/*
	 * Update the requiredby array of all required dependencies.
	 */
	if ((rv = xbps_requiredby_pkg_remove(pkgname)) != 0) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: [remove] failed to remove requiredby entries: %s",
		    pkgver, strerror(rv));
		free(pkgver);
		return rv;
	}

	/*
	 * Set package state to "config-files".
	 */
	rv = xbps_set_pkg_state_installed(pkgname, version, pkgver,
	     XBPS_PKG_STATE_CONFIG_FILES);
	if (rv != 0) {
		xbps_set_cb_state(XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: [remove] failed to set state to config-files: %s",
		    pkgver, strerror(rv));
	} else {
		xbps_set_cb_state(XBPS_STATE_REMOVE_DONE,
		     0, pkgname, version, NULL);
	}
	free(pkgver);

	return rv;
}
