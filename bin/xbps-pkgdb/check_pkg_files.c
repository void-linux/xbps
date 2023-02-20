/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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
 * 	o Check the mode for all installed files, except configuration files.
 *
 * 	o Check the user for all installed files, except configuration files.
 *
 * 	o Check the group for all installed files, except configuration files.
 *
 * Return 0 if test ran successfully, 1 otherwise and -1 on error.
 */

int
check_pkg_files(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_array_t array;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_dictionary_t pkg_filesd = arg;
	const char *file = NULL, *sha256 = NULL, *user = NULL, *group = NULL;
	char *path;
	bool mutable, test_broken = false, noexist = false;
	int rv = 0, errors = 0;
	mode_t mode = 0;
	struct idtree *idt = NULL;

	array = xbps_dictionary_get(pkg_filesd, "files");
	if (array != NULL && xbps_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(pkg_filesd, "files");
		if (iter == NULL) {
			errors++;
			goto out;
		}

		while ((obj = xbps_object_iterator_next(iter))) {
			noexist = mutable = false;

			xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
			/* skip noextract files */
			if (xhp->noextract && xbps_patterns_match(xhp->noextract, file))
				continue;
			path = xbps_xasprintf("%s/%s", xhp->rootdir, file);

			xbps_dictionary_get_bool(obj, "mutable", &mutable);

			/* check sha256 */
			xbps_dictionary_get_cstring_nocopy(obj, "sha256", &sha256);
			rv = xbps_file_sha256_check(path, sha256);
			switch (rv) {
			case 0:
				break;
			case ENOENT:
				xbps_error_printf("%s: unexistent file %s.\n", pkgname, file);
				test_broken = noexist = true;
				break;
			case ERANGE:
				if (!mutable) {
					xbps_error_printf("%s: hash mismatch for %s.\n", pkgname, file);
					test_broken = true;
				}
				break;
			default:
				xbps_error_printf("%s: can't check `%s' (%s)\n", pkgname, file, strerror(rv));
				break;
			}

			if (noexist) {
				free(path);
				continue;
			}

			/* check mode */
			mode = 0;
			if (xbps_dictionary_get_uint32(obj, "mode", &mode)) {
				rv = file_mode_check(path, mode);
				switch (rv) {
				case 0:
					break;
				case ERANGE:
					if (!mutable) {
						xbps_error_printf("%s: mode mismatch for %s.\n", pkgname, file);
						test_broken = true;
					}
					break;
				default:
					xbps_error_printf("%s: can't check `%s' (%s)\n", pkgname, file, strerror(-rv));
					break;
				}
			}

			/* check user */
			user = NULL;
			xbps_dictionary_get_cstring_nocopy(obj, "user", &user);
			if (user == NULL)
				user = "root";
			rv = file_user_check(idt, path, user);
			switch (rv) {
			case 0:
				break;
			case ERANGE:
				if (!mutable) {
					xbps_error_printf("%s: user mismatch for %s.\n", pkgname, file);
					test_broken = true;
				}
				break;
			default:
				xbps_error_printf("%s: can't check `%s' (%s)\n", pkgname, file, strerror(-rv));
				break;
			}

			/* check group */
			group = NULL;
			xbps_dictionary_get_cstring_nocopy(obj, "group", &group);
			if (group == NULL)
			group = "root";
			rv = file_group_check(idt, path, group);
			switch (rv) {
			case 0:
				break;
			case ERANGE:
				if (!mutable) {
					xbps_error_printf("%s: group mismatch for %s.\n", pkgname, file);
					test_broken = true;
				}
				break;
			default:
				xbps_error_printf("%s: can't check `%s' (%s)\n", pkgname, file, strerror(-rv));
				break;
			}

			free(path);
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
		if (iter == NULL) {
			errors++;
			goto out;
		}

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

out:
	idtree_free(idt);
	return errors ? -1 : 0;
}
