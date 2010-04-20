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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>

#include <xbps_api.h>

/**
 * @file lib/remove.c
 * @brief Package removal routines
 * @defgroup pkg_remove Package removal functions
 *
 * These functions will remove a package or only a subset of its
 * files. Package removal steps:
 *  -# Its <b>pre-remove</b> target specified in the REMOVE script
 *     will be executed.
 *  -# Its files, dirs and links will be removed. Modified files (not
 *     matching its sha256 hash) are preserved, unless XBPS_FLAG_FORCE
 *     is set via xbps_set_flags().
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
xbps_remove_pkg_files(prop_dictionary_t dict, const char *key)
{
	prop_array_t array;
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *file, *sha256;
	char *path = NULL;
	int flags = 0, rv = 0;

	assert(dict != NULL);
	assert(key != NULL);

	flags = xbps_get_flags();

	array = prop_dictionary_get(dict, key);
	if (array == NULL || prop_array_count(array) == 0)
		return 0;

	iter = xbps_get_array_iter_from_dict(dict, key);
	if (iter == NULL)
		return errno;

	while ((obj = prop_object_iterator_next(iter))) {
		if (!prop_dictionary_get_cstring_nocopy(obj, "file", &file)) {
			rv = errno;
			break;
		}
		path = xbps_xasprintf("%s/%s", xbps_get_rootdir(), file);
		if (path == NULL) {
			rv = errno;
			break;
		}
		if ((strcmp(key, "files") == 0) ||
		    (strcmp(key, "conf_files") == 0)) {
			/*
			 * Check SHA256 hash in regular files and
			 * configuration files.
			 */
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "sha256", &sha256)) {
				free(path);
				rv = errno;
				break;
			}
			rv = xbps_check_file_hash(path, sha256);
			if (rv == ENOENT) {
				fprintf(stderr,
				    "WARNING: '%s' doesn't exist!\n", file);
				free(path);
				rv = 0;
				continue;
			} else if (rv == ERANGE) {
				rv = 0;
				if (flags & XBPS_FLAG_VERBOSE) {
					if (flags & XBPS_FLAG_FORCE) {
						fprintf(stderr,
						    "WARNING: '%s' SHA256 "
						    "mismatch, forcing "
						    "removal...\n", file);
					} else {
						fprintf(stderr,
					    	"WARNING: '%s' SHA256 "
						"mismatch, preserving...\n",
						file);
					}
				}
				if ((flags & XBPS_FLAG_FORCE) == 0) {
					free(path);
					continue;
				}
			} else if (rv != 0 && rv != ERANGE) {
				free(path);
				break;
			}
		} else if (strcmp(key, "dirs") == 0) {
			if ((rv = rmdir(path)) == -1) {
				rv = 0;
				if (errno == ENOTEMPTY) {
					free(path);
					continue;
				}
				if (flags & XBPS_FLAG_VERBOSE) {
					fprintf(stderr,
					    "WARNING: can't remove "
				    	    "directory %s (%s)\n", file,
				    	strerror(errno));
					free(path);
					continue;
				}
			}
		}
		if (strcmp(key, "dirs")) {
			if ((rv = remove(path)) == -1) {
				if (flags & XBPS_FLAG_VERBOSE)
					fprintf(stderr,
					    "WARNING: can't remove %s "
					    "(%s)\n", file, strerror(errno));

				rv = 0;
				free(path);
				continue;
			}
		}
		if (flags & XBPS_FLAG_VERBOSE)
			printf("Removed: %s\n", file);

		free(path);
	}
	prop_object_iterator_release(iter);

	return rv;
}

int
xbps_remove_pkg(const char *pkgname, const char *version, bool update)
{
	prop_dictionary_t dict;
	const char *rootdir = xbps_get_rootdir();
	char *path, *buf;
	int rv = 0;
	bool prepostf = false;

	assert(pkgname != NULL);
	assert(version != NULL);

	/*
	 * Check if pkg is installed before anything else.
	 */
	if (!xbps_check_is_installed_pkgname(pkgname))
		return ENOENT;

	if (strcmp(rootdir, "") == 0)
		rootdir = "/";

	if (chdir(rootdir) == -1)
		return errno;

	buf = xbps_xasprintf(".%s/metadata/%s/REMOVE",
	    XBPS_META_PATH, pkgname);
	if (buf == NULL)
		return errno;

	/*
	 * Find out if the REMOVE file exists.
	 */
	if (access(buf, X_OK) == 0) {
		/*
		 * Run the pre remove action.
		 */
		prepostf = true;
		rv = xbps_file_chdir_exec(rootdir, buf, "pre", pkgname,
		    version, update ? "yes" : "no", NULL);
		if (rv != 0) {
			fprintf(stderr,
			    "%s: prerm action target error (%s)\n", pkgname,
			    strerror(errno));
			free(buf);
			return rv;
		}
	}

	/*
	 * If updating a package, we just need to execute the current
	 * pre-remove action target, unregister its requiredby entries and
	 * continue. Its files will be overwritten later in unpack phase.
	 */
	if (update) {
		free(buf);
		return xbps_requiredby_pkg_remove(pkgname);
	}

	/*
	 * Remove links, files and dirs.
	 */
	path = xbps_xasprintf("%s/%s/metadata/%s/%s",
	    rootdir, XBPS_META_PATH, pkgname, XBPS_PKGFILES);
	if (path == NULL) {
		free(buf);
		return errno;
	}

	dict = prop_dictionary_internalize_from_zfile(path);
	if (dict == NULL) {
		free(buf);
		free(path);
		return errno;
	}
	free(path);

	/* Remove links */
	if ((rv = xbps_remove_pkg_files(dict, "links")) != 0) {
		free(buf);
		prop_object_release(dict);
		return rv;
	}
	/* Remove regular files */
	if ((rv = xbps_remove_pkg_files(dict, "files")) != 0) {
		free(buf);
		prop_object_release(dict);
		return rv;
	}
	/* Remove dirs */
	if ((rv = xbps_remove_pkg_files(dict, "dirs")) != 0) {
		free(buf);
		prop_object_release(dict);
		return rv;
	}
	prop_object_release(dict);

	/*
	 * Run the post remove action if REMOVE file is there
	 * and we aren't updating a package.
	 */
	if (update == false && prepostf) {
		if ((rv = xbps_file_chdir_exec(rootdir, buf, "post",
		     pkgname, version, NULL)) != 0) {
			fprintf(stderr,
			    "%s: postrm action target error (%s)\n",
			    pkgname, strerror(errno));
			free(buf);
			return rv;
		}
	}
	free(buf);

	/*
	 * Update the requiredby array of all required dependencies.
	 */
	if ((rv = xbps_requiredby_pkg_remove(pkgname)) != 0)
		return rv;

	/*
	 * Set package state to "config-files".
	 */
	rv = xbps_set_pkg_state_installed(pkgname,
	     XBPS_PKG_STATE_CONFIG_FILES);

	return rv;
}
