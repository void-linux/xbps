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
#include <fcntl.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

int HIDDEN
xbps_config_file_from_archive_entry(prop_dictionary_t d,
				    struct archive_entry *entry,
				    const char *pkgname,
				    int *flags,
				    bool *skip)
{
	prop_dictionary_t forigd;
	prop_object_t obj, obj2;
	prop_object_iterator_t iter, iter2;
	const char *cffile, *sha256_new = NULL;
	char *buf, *sha256_cur = NULL, *sha256_orig = NULL;
	int rv = 0;
	bool install_new = false;

	if (d == NULL)
		return 0;

	iter = xbps_get_array_iter_from_dict(d, "conf_files");
	if (iter == NULL)
		return 0;

	/*
	 * Get original hash for the file from current
	 * installed package.
	 */
	forigd = xbps_get_pkg_dict_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (forigd == NULL) {
		install_new = true;
		goto out;
	}

	iter2 = xbps_get_array_iter_from_dict(forigd, "conf_files");
	if (iter2 != NULL) {
		while ((obj2 = prop_object_iterator_next(iter2))) {
			prop_dictionary_get_cstring_nocopy(obj2,
			    "file", &cffile);
			buf = xbps_xasprintf(".%s", cffile);
			if (buf == NULL) {
				prop_object_iterator_release(iter2);
				rv = errno;
				goto out;
			}
			if (strcmp(archive_entry_pathname(entry), buf) == 0) {
				prop_dictionary_get_cstring(obj2, "sha256",
				    &sha256_orig);
				free(buf);
				break;
			}
			free(buf);
			buf = NULL;
		}
		prop_object_iterator_release(iter2);
	}
	prop_object_release(forigd);
	/*
	 * First case: original hash not found, install new file.
	 */
	if (sha256_orig == NULL) {
		install_new = true;
		goto out;
	}

	/*
	 * Compare original, installed and new hash for current file.
	 */
	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "file", &cffile);
		buf = xbps_xasprintf(".%s", cffile);
		if (buf == NULL) {
			prop_object_iterator_release(iter);
			return errno;
		}
		if (strcmp(archive_entry_pathname(entry), buf)) {
			free(buf);
			buf = NULL;
			continue;
		}
		sha256_cur = xbps_get_file_hash(buf);
		free(buf);
		prop_dictionary_get_cstring_nocopy(obj, "sha256", &sha256_new);
		if (sha256_cur == NULL) {
			if (errno == ENOENT) {
				/*
				 * File not installed, install new one.
				 */
				install_new = true;
				break;
			} else {
				rv = errno;
				break;
			}
		}

		/*
		 * Orig = X, Curr = X, New = X
		 *
		 * Install new file.
		 */
		if ((strcmp(sha256_orig, sha256_cur) == 0) &&
		    (strcmp(sha256_orig, sha256_new) == 0) &&
		    (strcmp(sha256_cur, sha256_new) == 0)) {
			install_new = true;
			break;
		/*
		 * Orig = X, Curr = X, New = Y
		 *
		 * Install new file.
		 */
		} else if ((strcmp(sha256_orig, sha256_cur) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_cur, sha256_new))) {
			printf("Updating %s file with new version.\n",
			    cffile);
			install_new = true;
			break;
		/*
		 * Orig = X, Curr = Y, New = X
		 *
		 * Keep current file as is.
		 */
		} else if ((strcmp(sha256_orig, sha256_new) == 0) &&
			   (strcmp(sha256_cur, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			printf("Keeping modified file %s.\n", cffile);
			*skip = true;
			break;
		/*
		 * Orig = X, Curr = Y, New = Y
		 *
		 * Install new file.
		 */
		} else if ((strcmp(sha256_cur, sha256_new) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			install_new = true;
			break;
		/*
		 * Orig = X, Curr = Y, New = Z
		 *
		 * Install new file as file.new.
		 */
		} else  if ((strcmp(sha256_orig, sha256_cur)) &&
			    (strcmp(sha256_cur, sha256_new)) &&
			    (strcmp(sha256_orig, sha256_new))) {
			buf = xbps_xasprintf(".%s.new", cffile);
			if (buf == NULL) {
				rv = errno;
				break;
			}
			printf("Keeping modified file %s.\n", cffile);
			printf("Installing new version as %s.new.\n", cffile);
			install_new = true;
			archive_entry_set_pathname(entry, buf);
			free(buf);
			break;
		}
	}

out:
	if (install_new) {
		*flags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE;
		*flags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
	}
	if (sha256_orig)
		free(sha256_orig);
	if (sha256_cur)
		free(sha256_cur);

	prop_object_iterator_release(iter);

	return rv;
}
