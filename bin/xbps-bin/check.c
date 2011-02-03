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

#include <xbps_api.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package. This
 * consists in four tasks:
 *
 * 	o Check for metadata files (files.plist and props.plist),
 * 	  we only check if the file exists and its dictionary can
 * 	  be externalized and is not empty.
 * 	o Check for missing installed files.
 * 	o Check the hash for all installed files, except
 * 	  configuration files (which is expected if they are modified).
 * 	o Check for missing run time dependencies.
 */

int
xbps_check_pkg_integrity_all(void)
{
	prop_dictionary_t d;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	const char *pkgname, *version;
	int rv = 0;
	size_t npkgs = 0, nbrokenpkgs = 0;

	d = xbps_regpkgdb_dictionary_get();
	if (d == NULL)
		return ENODEV;

	iter = xbps_get_array_iter_from_dict(d, "packages");
	if (iter == NULL) {
		rv = ENOENT;
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		printf("Checking %s-%s ...\n", pkgname, version);
		if ((rv = xbps_check_pkg_integrity(pkgname)) != 0)
			nbrokenpkgs++;
		npkgs++;
		printf("\033[1A\033[K");
	}
	printf("%zu package%s processed: %zu broken.\n", npkgs,
	    npkgs == 1 ? "" : "s", nbrokenpkgs);

out:
	if (iter)
		prop_object_iterator_release(iter);

	xbps_regpkgdb_dictionary_release();

	return rv;
}

int
xbps_check_pkg_integrity(const char *pkgname)
{
	prop_dictionary_t pkgd, propsd = NULL, filesd = NULL;
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *file, *sha256, *reqpkg;
	char *path;
	int rv = 0;
	bool broken = false, files_broken = false;

	assert(pkgname != NULL);

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL) {
		printf("Package %s is not installed.\n", pkgname);
		return 0;
	}
	prop_object_release(pkgd);

	/*
	 * Check for props.plist metadata file.
	 */
	propsd = xbps_get_pkg_dict_from_metadata_plist(pkgname, XBPS_PKGPROPS);
	if (prop_object_type(propsd) != PROP_TYPE_DICTIONARY) {
		fprintf(stderr,
		    "E: %s: unexistent %s or invalid metadata file.\n", pkgname,
		    XBPS_PKGPROPS);
		rv = errno;
		goto out;
	} else if (prop_dictionary_count(propsd) == 0) {
		fprintf(stderr,
		    "E: %s: incomplete %s metadata file.\n", pkgname,
		    XBPS_PKGPROPS);
		rv = EINVAL;
		goto out;
	}

	/*
	 * Check for files.plist metadata file.
	 */
	filesd = xbps_get_pkg_dict_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (prop_object_type(filesd) != PROP_TYPE_DICTIONARY) {
		fprintf(stderr,
		    "E: %s: unexistent %s or invalid metadata file.\n", pkgname,
		    XBPS_PKGPROPS);
		rv = ENOENT;
		goto out;
	} else if (prop_dictionary_count(filesd) == 0) {
		fprintf(stderr,
		    "E: %s: incomplete %s metadata file.\n", pkgname,
		    XBPS_PKGFILES);
		rv = EINVAL;
		goto out;
	}

	/*
	 * Check for missing files and its hash.
	 */
	array = prop_dictionary_get(filesd, "files");
	if ((prop_object_type(array) == PROP_TYPE_ARRAY) &&
	     prop_array_count(array) > 0) {
		iter = xbps_get_array_iter_from_dict(filesd, "files");
		if (iter == NULL) {
			rv = ENOMEM;
			goto out;
		}
		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			path = xbps_xasprintf("%s/%s",
			    xbps_get_rootdir(), file);
			if (path == NULL) {
				prop_object_iterator_release(iter);
				rv = errno;
				goto out;
			}
                        prop_dictionary_get_cstring_nocopy(obj,
                            "sha256", &sha256);
			rv = xbps_check_file_hash(path, sha256);
			switch (rv) {
			case 0:
				break;
			case ENOENT:
				fprintf(stderr, "%s: unexistent file %s.\n",
				    pkgname, file);
				files_broken = true;
				break;
			case ERANGE:
                                fprintf(stderr, "%s: hash mismatch for %s.\n",
				    pkgname, file);
				files_broken = true;
				break;
			default:
				fprintf(stderr,
				    "%s: unexpected error for %s (%s)\n",
				    pkgname, file, strerror(rv));
				break;
			}
			free(path);
                }
                prop_object_iterator_release(iter);
		if (files_broken) {
			broken = true;
			printf("%s: files check FAILED.\n", pkgname);
		}
	}

	/*
	 * Check for missing configuration files.
	 */
	array = prop_dictionary_get(filesd, "conf_files");
	if (array && prop_object_type(array) == PROP_TYPE_ARRAY &&
	    prop_array_count(array) > 0) {
		iter = xbps_get_array_iter_from_dict(filesd, "conf_files");
		if (iter == NULL) {
			rv = ENOMEM;
			goto out;
		}
		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			path = xbps_xasprintf("%s/%s",
			    xbps_get_rootdir(), file);
			if (path == NULL) {
				prop_object_iterator_release(iter);
				rv = ENOMEM;
				goto out;
			}
			if ((rv = access(path, R_OK)) == -1) {
				if (errno == ENOENT) {
					fprintf(stderr,
					    "%s: unexistent file %s\n",
					    pkgname, file);
					broken = true;
				} else
					fprintf(stderr,
					    "%s: unexpected error for "
					    "%s (%s)\n", pkgname, file,
					    strerror(errno));
			}
			free(path);
		}
		prop_object_iterator_release(iter);
		if (rv != 0)
			printf("%s: configuration files check FAILED.\n",
			    pkgname);
	}

	/*
	 * Check for missing run time dependencies.
	 */
	if (xbps_pkg_has_rundeps(propsd)) {
		iter = xbps_get_array_iter_from_dict(propsd, "run_depends");
		if (iter == NULL) {
			rv = ENOMEM;
			goto out;
		}
		while ((obj = prop_object_iterator_next(iter))) {
			reqpkg = prop_string_cstring_nocopy(obj);
			if (reqpkg == NULL) {
				prop_object_iterator_release(iter);
				rv = EINVAL;
				goto out;
			}
			rv = xbps_check_is_installed_pkg_by_pattern(reqpkg);
			if (rv <= 0) {
				printf("%s: dependency not satisfied: %s\n",
				    pkgname, reqpkg);
			}
			rv = 0;
		}
		prop_object_iterator_release(iter);
		if (rv == ENOENT) {
			printf("%s: run-time dependency check FAILED.\n",
			    pkgname);
			broken = true;
		}
	}

out:
	if (filesd)
		prop_object_release(filesd);
	if (propsd)
		prop_object_release(propsd);
	if (broken)
		rv = EINVAL;

	return rv;
}
