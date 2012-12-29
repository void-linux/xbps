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
 *  -# Its state will be changed to XBPS_PKG_STATE_HALF_REMOVED.
 *  -# Its <b>purge-remove</b> target specified in the REMOVE script
 *     will be executed.
 *  -# Its package metadata file will be removed.
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
int
xbps_remove_pkg_files(struct xbps_handle *xhp,
		      prop_dictionary_t dict,
		      const char *key,
		      const char *pkgver)
{
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
	assert(pkgname);
	version = xbps_pkg_version(pkgver);

	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "file", &file);
		path = xbps_xasprintf("%s/%s", xhp->rootdir, file);

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
				xbps_set_cb_state(xhp,
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
					xbps_set_cb_state(xhp,
					    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
					    0, pkgname, version,
					    "%s: %s `%s' SHA256 mismatch, "
					    "preserving file", pkgver,
					    curobj, file);
					free(path);
					continue;
				} else {
					xbps_set_cb_state(xhp,
					    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
					    0, pkgname, version,
					    "%s: %s `%s' SHA256 mismatch, "
					    "forcing removal", pkgver,
					    curobj, file);
				}
			} else if (rv != 0 && rv != ERANGE) {
				xbps_set_cb_state(xhp,
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
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FILE_FAIL,
			    errno, pkgname, version,
			    "%s: failed to remove %s `%s': %s", pkgver,
			    curobj, file, strerror(errno));
			errno = 0;
		} else {
			/* success */
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FILE,
			    0, pkgname, version,
			    "Removed %s `%s'", curobj, file);
		}
		free(path);
	}
	prop_object_iterator_release(iter);
	free(pkgname);

	return rv;
}

int
xbps_remove_pkg(struct xbps_handle *xhp,
		const char *pkgver,
		bool update,
		bool soft_replace)
{
	prop_dictionary_t pkgd = NULL;
	char *pkgname, *buf = NULL;
	const char *version;
	int rv = 0;
	pkg_state_t state = 0;

	assert(xhp);
	assert(pkgver);

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);
	version = xbps_pkg_version(pkgver);
	assert(version);

	if ((rv = xbps_pkg_state_installed(xhp, pkgname, &state)) != 0)
		goto out;

	xbps_dbg_printf(xhp, "attempting to remove %s state %d\n",
	    pkgver, state);

	if (!update)
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE, 0, pkgname, version, NULL);

	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		   "%s: [remove] failed to chdir to rootdir `%s': %s",
		    pkgver, xhp->rootdir, strerror(rv));
		goto out;
	}

	/* internalize pkg dictionary from metadir */
	buf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, pkgname);
	pkgd = prop_dictionary_internalize_from_file(buf);
	free(buf);
	if (pkgd == NULL)
		xbps_dbg_printf(xhp, "WARNING: metaplist for %s "
		    "doesn't exist!\n", pkgname);

	/* If package was "half-removed", remove it fully. */
	if (state == XBPS_PKG_STATE_HALF_REMOVED)
		goto purge;
	/*
	 * Run the pre remove action.
	 */
	if (pkgd) {
		rv = xbps_pkg_exec_script(xhp, pkgd, "remove-script",
		    "pre", update);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
			    errno, pkgname, version,
			    "%s: [remove] REMOVE script failed to "
			    "execute pre ACTION: %s",
			    pkgver, strerror(errno));
			rv = errno;
			goto purge;
		}
	}
	/*
	 * If updating a package, we just need to execute the current
	 * pre-remove action target and we are done. Its files will be
	 * overwritten later in unpack phase.
	 */
	if (update) {
		if (pkgd)
			prop_object_release(pkgd);
		free(pkgname);
		return 0;
	} else if (soft_replace) {
		/*
		 * Soft replace a package. Do not remove its files, but
		 * execute PURGE action, remove metadata files and unregister
		 * from pkgdb.
		 */
		goto softreplace;
	}

	if (pkgd) {
		/* Remove regular files */
		if ((rv = xbps_remove_pkg_files(xhp, pkgd, "files", pkgver)) != 0)
			goto out;
		/* Remove configuration files */
		if ((rv = xbps_remove_pkg_files(xhp, pkgd, "conf_files", pkgver)) != 0)
			goto out;
		/* Remove links */
		if ((rv = xbps_remove_pkg_files(xhp, pkgd, "links", pkgver)) != 0)
			goto out;
		/* Remove dirs */
		if ((rv = xbps_remove_pkg_files(xhp, pkgd, "dirs", pkgver)) != 0)
			goto out;
		/*
		 * Execute the post REMOVE action if file exists and we aren't
		 * updating the package.
		 */
		rv = xbps_pkg_exec_script(xhp, pkgd, "remove-script", "post", false);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
			    rv, pkgname, version,
			    "%s: [remove] REMOVE script failed to execute "
			    "post ACTION: %s", pkgver, strerror(rv));
			goto out;
		}
	}

softreplace:
	/*
	 * Set package state to "half-removed".
	 */
	rv = xbps_set_pkg_state_installed(xhp, pkgname, version,
	     XBPS_PKG_STATE_HALF_REMOVED);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    rv, pkgname, version,
		    "%s: [remove] failed to set state to half-removed: %s",
		    pkgver, strerror(rv));
		goto out;
	}

purge:
	/*
	 * Execute the purge REMOVE action if file exists.
	 */
	if (pkgd) {
		rv = xbps_pkg_exec_script(xhp, pkgd, "remove-script", "purge", false);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
			    rv, pkgname, version,
			    "%s: REMOVE script failed to execute "
			    "purge ACTION: %s", pkgver, strerror(rv));
			goto out;
		}
		prop_object_release(pkgd);
	}
	/*
	 * Remove package metadata plist.
	 */
	buf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, pkgname);
	if (remove(buf) == -1) {
		free(buf);
		if (errno != ENOENT) {
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
			    rv, pkgname, version,
			    "%s: failed to remove metadata file: %s",
			    pkgver, strerror(errno));
		}
	}
	free(buf);
	/*
	 * Unregister package from pkgdb.
	 */
	if ((rv = xbps_unregister_pkg(xhp, pkgver, true)) != 0)
		goto out;

	xbps_dbg_printf(xhp, "[remove] unregister %s returned %d\n", pkgver, rv);

	xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_DONE,
	     0, pkgname, version, NULL);
out:
	if (pkgname != NULL)
		free(pkgname);

	return rv;
}
