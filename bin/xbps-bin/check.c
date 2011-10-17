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
#include <assert.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps_api.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package. This
 * consists in five tasks:
 *
 * 	o Check for metadata files (files.plist and props.plist),
 * 	  we only check if the file exists and its dictionary can
 * 	  be externalized and is not empty.
 * 	o Check for missing installed files.
 * 	o Check for target file in symlinks, so that we can check that
 * 	  they have not been modified.
 * 	o Check the hash for all installed files, except
 * 	  configuration files (which is expected if they are modified).
 * 	o Check for missing run time dependencies.
 */

int
check_pkg_integrity_all(void)
{
	const struct xbps_handle *xhp;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	const char *pkgname, *version;
	size_t npkgs = 0, nbrokenpkgs = 0;

	xhp = xbps_handle_get();
	iter = xbps_array_iter_from_dict(xhp->regpkgdb_dictionary, "packages");
	if (iter == NULL)
		return -1;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		printf("Checking %s-%s ...\n", pkgname, version);
		if (check_pkg_integrity(pkgname) != 0)
			nbrokenpkgs++;

		npkgs++;
	}
	prop_object_iterator_release(iter);

	printf("%zu package%s processed: %zu broken.\n", npkgs,
	    npkgs == 1 ? "" : "s", nbrokenpkgs);

	return 0;
}

int
check_pkg_integrity(const char *pkgname)
{
	struct xbps_handle *xhp;
	prop_dictionary_t pkgd, propsd = NULL, filesd = NULL;
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *file, *sha256, *reqpkg, *tgt = NULL;
	char *path, buf[PATH_MAX];
	int rv = 0;
	bool broken = false, test_broken = false;

	assert(pkgname != NULL);
	xhp = xbps_handle_get();

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL) {
		printf("Package %s is not installed.\n", pkgname);
		return 0;
	}
	prop_object_release(pkgd);

	/*
	 * Check for props.plist metadata file.
	 */
	propsd = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGPROPS);
	if (prop_object_type(propsd) != PROP_TYPE_DICTIONARY) {
		xbps_error_printf("%s: unexistent %s or invalid metadata "
		    "file.\n", pkgname, XBPS_PKGPROPS);
		broken = true;
		goto out;
	} else if (prop_dictionary_count(propsd) == 0) {
		xbps_error_printf("%s: incomplete %s metadata file.\n",
		    pkgname, XBPS_PKGPROPS);
		broken = true;
		goto out;
	}

	/*
	 * Check for files.plist metadata file.
	 */
	filesd = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (prop_object_type(filesd) != PROP_TYPE_DICTIONARY) {
		xbps_error_printf("%s: unexistent %s or invalid metadata "
		    "file.\n", pkgname, XBPS_PKGFILES);
		broken = true;
		goto out;
	} else if (prop_dictionary_count(filesd) == 0) {
		xbps_error_printf("%s: incomplete %s metadata file.\n",
		    pkgname, XBPS_PKGFILES);
		broken = true;
		goto out;
	}

	/*
	 * Check for target files in symlinks.
	 */
	array = prop_dictionary_get(filesd, "links");
	if ((prop_object_type(array) == PROP_TYPE_ARRAY) &&
	     prop_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(filesd, "links");
		if (iter == NULL)
			abort();

		while ((obj = prop_object_iterator_next(iter))) {
			if (!prop_dictionary_get_cstring_nocopy(obj, "target", &tgt))
				continue;
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			if (strcmp(tgt, "") == 0) {
				xbps_warn_printf("%s: `%s' symlink with "
				    "empty target object!\n", pkgname, file);
				continue;
			}
			path = xbps_xasprintf("%s/%s",
			    prop_string_cstring_nocopy(xhp->rootdir), file);
			if (path == NULL)
				abort();

			memset(&buf, 0, sizeof(buf));
			if (realpath(path, buf) == NULL) {
				xbps_error_printf("%s: broken symlink `%s': "
				    "%s\n", pkgname, file, strerror(errno));
				test_broken = true;
				continue;
			}

			free(path);
			if (!prop_string_equals_cstring(xhp->rootdir, "/") &&
			    strstr(buf, prop_string_cstring_nocopy(xhp->rootdir)))
				path = buf + prop_string_size(xhp->rootdir);
			else
				path = buf;

			if (strcmp(path, tgt)) {
				xbps_error_printf("%s: modified symlink `%s', "
				    "target: `%s' (shall be: `%s')\n",
				    pkgname, file, tgt, path);
				test_broken = true;
			}
			path = NULL;
		}
		prop_object_iterator_release(iter);
	}
	if (test_broken) {
		test_broken = false;
		xbps_error_printf("%s: links check FAILED.\n", pkgname);
		broken = true;
	}

	/*
	 * Check for missing files and its hash.
	 */
	array = prop_dictionary_get(filesd, "files");
	if ((prop_object_type(array) == PROP_TYPE_ARRAY) &&
	     prop_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(filesd, "files");
		if (iter == NULL)
			abort();

		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			path = xbps_xasprintf("%s/%s",
			    prop_string_cstring_nocopy(xhp->rootdir), file);
			if (path == NULL) {
				prop_object_iterator_release(iter);
				abort();
			}
                        prop_dictionary_get_cstring_nocopy(obj,
                            "sha256", &sha256);
			rv = xbps_file_hash_check(path, sha256);
			switch (rv) {
			case 0:
				break;
			case ENOENT:
				xbps_error_printf("%s: unexistent file %s.\n",
				    pkgname, file);
				test_broken = true;
				break;
			case ERANGE:
                                xbps_error_printf("%s: hash mismatch for %s.\n",
				    pkgname, file);
				test_broken = true;
				break;
			default:
				xbps_error_printf(
				    "%s: can't check `%s' (%s)\n",
				    pkgname, file, strerror(rv));
				break;
			}
			free(path);
                }
                prop_object_iterator_release(iter);
	}
	if (test_broken) {
		test_broken = false;
		broken = true;
		xbps_error_printf("%s: files check FAILED.\n", pkgname);
	}

	/*
	 * Check for missing configuration files.
	 */
	array = prop_dictionary_get(filesd, "conf_files");
	if (array && prop_object_type(array) == PROP_TYPE_ARRAY &&
	    prop_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(filesd, "conf_files");
		if (iter == NULL)
			abort();

		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			path = xbps_xasprintf("%s/%s",
			    prop_string_cstring_nocopy(xhp->rootdir), file);
			if (path == NULL) {
				prop_object_iterator_release(iter);
				abort();
			}
			if ((rv = access(path, R_OK)) == -1) {
				if (errno == ENOENT) {
					xbps_error_printf(
					    "%s: unexistent file %s\n",
					    pkgname, file);
					test_broken = true;
				} else
					xbps_error_printf(
					    "%s: can't check `%s' (%s)\n",
					    pkgname, file,
					    strerror(errno));
			}
			free(path);
		}
		prop_object_iterator_release(iter);
	}
	if (test_broken) {
		test_broken = false;
		xbps_error_printf("%s: conf files check FAILED.\n", pkgname);
		broken = true;
	}

	/*
	 * Check for missing run time dependencies.
	 */
	if (!xbps_pkg_has_rundeps(propsd))
		goto out;

	iter = xbps_array_iter_from_dict(propsd, "run_depends");
	if (iter == NULL)
		abort();

	while ((obj = prop_object_iterator_next(iter))) {
		reqpkg = prop_string_cstring_nocopy(obj);
		if (reqpkg == NULL) {
			prop_object_iterator_release(iter);
			abort();
		}
		if (xbps_check_is_installed_pkg_by_pattern(reqpkg) <= 0) {
			xbps_error_printf("%s: dependency not satisfied: %s\n",
			    pkgname, reqpkg);
			test_broken = true;
		}
	}
	prop_object_iterator_release(iter);
	if (test_broken) {
		xbps_error_printf("%s: rundeps check FAILED.\n", pkgname);
		broken = true;
	}

out:
	if (filesd)
		prop_object_release(filesd);
	if (propsd)
		prop_object_release(propsd);
	if (broken)
		return -1;

	return 0;
}
