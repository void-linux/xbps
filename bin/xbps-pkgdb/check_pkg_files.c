/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following tasks are processed in that order:
 *
 * 	o Check for missing installed files.
 *
 * 	o Check the hash for all installed files, except
 * 	  configuration files (which is expected if they are modified).
 *
 * 	o Compares stored file modification time.
 *
 * Return 0 if test ran successfully, 1 otherwise and -1 on error.
 */
static bool
check_file_mtime(xbps_dictionary_t d, const char *pkg, const char *path)
{
	struct stat sb;
	uint64_t mtime = 0;
	const char *file = NULL;

	/* if obj is not there, skip silently */
	if (!xbps_dictionary_get_uint64(d, "mtime", &mtime))
		return false;

	/* if file is mutable, we don't care if it does not match */
	if (xbps_dictionary_get(d, "mutable"))
		return false;

	if (stat(path, &sb) == -1)
		return true;

	if ((uint64_t)sb.st_mtime != mtime) {
		xbps_dictionary_get_cstring_nocopy(d, "file", &file);
		xbps_error_printf("%s: %s mtime mismatch "
		    "(current: %ju, stored %ju)\n",
		    pkg, file, (uint64_t)sb.st_mtime, mtime);
		return true;
	}
	return false;
}

int
check_pkg_files(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_array_t array;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_dictionary_t pkg_filesd = arg;
	const char *file = NULL, *sha256 = NULL;
	char *path;
	bool mutable, test_broken = false;
	int rv = 0, errors = 0;

	array = xbps_dictionary_get(pkg_filesd, "files");
	if (array != NULL && xbps_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(pkg_filesd, "files");
		if (iter == NULL)
			return -1;

		while ((obj = xbps_object_iterator_next(iter))) {
			xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
			/* skip noextract files */
			if (xhp->noextract && xbps_patterns_match(xhp->noextract, file))
				continue;
			path = xbps_xasprintf("%s/%s", xhp->rootdir, file);
			xbps_dictionary_get_cstring_nocopy(obj,
				"sha256", &sha256);
			rv = xbps_file_sha256_check(path, sha256);
			switch (rv) {
			case 0:
				if (check_file_mtime(obj, pkgname, path)) {
					test_broken = true;
				}
				free(path);
				break;
			case ENOENT:
				xbps_error_printf("%s: unexistent file %s.\n",
				    pkgname, file);
				free(path);
				test_broken = true;
				break;
			case ERANGE:
				mutable = false;
				xbps_dictionary_get_bool(obj,
				    "mutable", &mutable);
				if (!mutable) {
					xbps_error_printf("%s: hash mismatch "
					    "for %s.\n", pkgname, file);
					test_broken = true;
				}
				free(path);
				break;
			default:
				xbps_error_printf(
				    "%s: can't check `%s' (%s)\n",
				    pkgname, file, strerror(rv));
				free(path);
				break;
			}
                }
                xbps_object_iterator_release(iter);
	}
	if (test_broken) {
		xbps_error_printf("%s: files check FAILED.\n", pkgname);
		test_broken = false;
		errors++;
	}

	/*
	 * Check for missing configuration files.
	 */
	array = xbps_dictionary_get(pkg_filesd, "conf_files");
	if (array != NULL && xbps_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(pkg_filesd, "conf_files");
		if (iter == NULL)
			return -1;

		while ((obj = xbps_object_iterator_next(iter))) {
			xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
			/* skip noextract files */
			if (xhp->noextract && xbps_patterns_match(xhp->noextract, file))
				continue;
			path = xbps_xasprintf("%s/%s", xhp->rootdir, file);
			if (access(path, R_OK) == -1) {
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
		xbps_object_iterator_release(iter);
	}
	if (test_broken) {
		xbps_error_printf("%s: conf files check FAILED.\n", pkgname);
		errors++;
	}

	return errors ? -1 : 0;
}
