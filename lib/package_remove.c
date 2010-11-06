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
#include <libgen.h>

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
	const char *file, *sha256, *curobj = NULL;
	char *dname = NULL, *path = NULL;
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

	if (strcmp(key, "files") == 0)
		curobj = "file";
	else if (strcmp(key, "conf_files") == 0)
		curobj = "configuration file";
	else if (strcmp(key, "links") == 0)
		curobj = "link";
	else if (strcmp(key, "dirs") == 0)
		curobj = "directory";

	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "file", &file);
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
			prop_dictionary_get_cstring_nocopy(obj,
			    "sha256", &sha256);
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
		}
		/*
		 * Remove the object if possible.
		 */
		if (remove(path) == -1) {
			if (flags & XBPS_FLAG_VERBOSE)
				fprintf(stderr,
				    "WARNING: can't remove %s %s "
				    "(%s)\n", curobj, file, strerror(errno));

		} else {
			/* Success */
			if (flags & XBPS_FLAG_VERBOSE)
				printf("Removed %s: %s\n", curobj, file);
		}
		/*
		 * When purging a package, also remove the directory where
		 * the conf_files are living on.
		 */
		if (strcmp(key, "conf_files") == 0) {
			dname = dirname(path);
			if (rmdir(dname) == -1) {
				if (errno != ENOTEMPTY) {
					fprintf(stderr,
				    	    "WARNING: can't remove %s %s "
				    	    "(%s)\n", curobj, file,
					    strerror(errno));
				}
			} else {
				if (flags & XBPS_FLAG_VERBOSE)
					printf("Removed empty directory: "
					    "%s\n", dname);
			}
		}
		free(path);
	}
	prop_object_iterator_release(iter);

	return rv;
}

int
xbps_remove_pkg(const char *pkgname, const char *version, bool update)
{
	prop_dictionary_t dict;
	char *path, *buf;
	int rv = 0;

	assert(pkgname != NULL);
	assert(version != NULL);

	/*
	 * Check if pkg is installed before anything else.
	 */
	if (!xbps_check_is_installed_pkgname(pkgname))
		return ENOENT;

	buf = xbps_xasprintf(".%s/metadata/%s/REMOVE",
	    XBPS_META_PATH, pkgname);
	if (buf == NULL)
		return errno;

	if (chdir(xbps_get_rootdir()) == -1) {
		free(buf);
		return errno;
	}

	/*
	 * Run the pre remove action.
	 */
	rv = xbps_file_exec(buf, "pre", pkgname, version,
	    update ? "yes" : "no", NULL);
	if (rv != 0 && errno != ENOENT) {
		fprintf(stderr,
		    "%s: prerm action target error (%s)\n", pkgname,
		    strerror(errno));
		free(buf);
		return rv;
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
	path = xbps_xasprintf(".%s/metadata/%s/%s",
	    XBPS_META_PATH, pkgname, XBPS_PKGFILES);
	if (path == NULL) {
		free(buf);
		return errno;
	}
	dict = prop_dictionary_internalize_from_zfile(path);
	if (dict == NULL) {
		free(path);
		free(buf);
		return errno;
	}
	free(path);

	/* Remove links */
	if ((rv = xbps_remove_pkg_files(dict, "links")) != 0) {
		prop_object_release(dict);
		free(buf);
		return rv;
	}
	/* Remove regular files */
	if ((rv = xbps_remove_pkg_files(dict, "files")) != 0) {
		prop_object_release(dict);
		free(buf);
		return rv;
	}
	/* Remove dirs */
	if ((rv = xbps_remove_pkg_files(dict, "dirs")) != 0) {
		prop_object_release(dict);
		free(buf);
		return rv;
	}
	prop_object_release(dict);

	/*
	 * Execute the post REMOVE action if file exists and we aren't
	 * updating the package.
	 */
	rv = xbps_file_exec(buf, "post", pkgname, version, "no", NULL);
	if (rv != 0 && errno != ENOENT) {
		fprintf(stderr,
		    "%s: postrm action target error (%s)\n", pkgname,
		    strerror(errno));
		free(buf);
		return rv;
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
