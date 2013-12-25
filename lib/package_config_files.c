/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/*
 * Returns true if entry is a configuration file, false otherwise.
 */
int HIDDEN
xbps_entry_is_a_conf_file(xbps_dictionary_t filesd,
			  const char *entry_pname)
{
	xbps_array_t array;
	xbps_dictionary_t d;
	const char *cffile;

	array = xbps_dictionary_get(filesd, "conf_files");
	if (xbps_array_count(array) == 0)
		return false;

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		d = xbps_array_get(array, i);
		xbps_dictionary_get_cstring_nocopy(d, "file", &cffile);
		if (strcmp(cffile, entry_pname) == 0)
			return true;
	}
	return false;
}

/*
 * Returns 1 if entry should be installed, 0 if don't or -1 on error.
 */
int HIDDEN
xbps_entry_install_conf_file(struct xbps_handle *xhp,
			     xbps_dictionary_t filesd,
			     struct archive_entry *entry,
			     const char *entry_pname,
			     const char *pkgver,
			     const char *pkgname)
{
	xbps_dictionary_t forigd;
	xbps_object_t obj, obj2;
	xbps_object_iterator_t iter, iter2;
	const char *cffile, *sha256_new = NULL;
	char *buf, *sha256_cur = NULL, *sha256_orig = NULL;
	int rv = 0;

	assert(xbps_object_type(filesd) == XBPS_TYPE_DICTIONARY);
	assert(entry != NULL);
	assert(entry_pname != NULL);
	assert(pkgver != NULL);

	iter = xbps_array_iter_from_dict(filesd, "conf_files");
	if (iter == NULL)
		return -1;

	/*
	 * Get original hash for the file from current
	 * installed package.
	 */
	xbps_dbg_printf(xhp, "%s: processing conf_file %s\n",
	    pkgver, entry_pname);

	forigd = xbps_pkgdb_get_pkg_metadata(xhp, pkgname);
	if (forigd == NULL) {
		xbps_dbg_printf(xhp, "%s: conf_file %s not currently "
		    "installed\n", pkgver, entry_pname);
		rv = 1;
		goto out;
	}

	iter2 = xbps_array_iter_from_dict(forigd, "conf_files");
	if (iter2 != NULL) {
		while ((obj2 = xbps_object_iterator_next(iter2))) {
			xbps_dictionary_get_cstring_nocopy(obj2,
			    "file", &cffile);
			buf = xbps_xasprintf(".%s", cffile);
			if (strcmp(entry_pname, buf) == 0) {
				xbps_dictionary_get_cstring(obj2, "sha256",
				    &sha256_orig);
				free(buf);
				break;
			}
			free(buf);
			buf = NULL;
		}
		xbps_object_iterator_release(iter2);
	}
	/*
	 * First case: original hash not found, install new file.
	 */
	if (sha256_orig == NULL) {
		xbps_dbg_printf(xhp, "%s: conf_file %s not installed\n",
		    pkgver, entry_pname);
		rv = 1;
		goto out;
	}

	/*
	 * Compare original, installed and new hash for current file.
	 */
	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_dictionary_get_cstring_nocopy(obj, "file", &cffile);
		buf = xbps_xasprintf(".%s", cffile);
		if (strcmp(entry_pname, buf)) {
			free(buf);
			buf = NULL;
			continue;
		}
		sha256_cur = xbps_file_hash(buf);
		free(buf);
		xbps_dictionary_get_cstring_nocopy(obj, "sha256", &sha256_new);
		if (sha256_cur == NULL) {
			if (errno == ENOENT) {
				/*
				 * File not installed, install new one.
				 */
				xbps_dbg_printf(xhp, "%s: conf_file %s not "
				    "installed\n", pkgver, entry_pname);
				rv = 1;
				break;
			} else {
				rv = -1;
				break;
			}
		}
		/*
		 * Orig = X, Curr = X, New = X
		 *
		 * Keep file as is (no changes).
		 */
		if ((strcmp(sha256_orig, sha256_cur) == 0) &&
		    (strcmp(sha256_orig, sha256_new) == 0) &&
		    (strcmp(sha256_cur, sha256_new) == 0)) {
			xbps_dbg_printf(xhp, "%s: conf_file %s orig = X, "
			    "cur = X, new = X\n", pkgver, entry_pname);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = X, New = Y
		 *
		 * Install new file (installed file hasn't been modified).
		 */
		} else if ((strcmp(sha256_orig, sha256_cur) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_cur, sha256_new))) {
			xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Updating configuration file `%s' provided "
			    "by `%s'.", cffile, pkgver);
			rv = 1;
			break;
		/*
		 * Orig = X, Curr = Y, New = X
		 *
		 * Keep installed file as is because it has been modified,
		 * but new package doesn't contain new changes compared
		 * to the original version.
		 */
		} else if ((strcmp(sha256_orig, sha256_new) == 0) &&
			   (strcmp(sha256_cur, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Keeping modified configuration file `%s'.",
			    cffile);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = Y, New = Y
		 *
		 * Keep file as is because changes made are compatible
		 * with new version.
		 */
		} else if ((strcmp(sha256_cur, sha256_new) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			xbps_dbg_printf(xhp, "%s: conf_file %s orig = X, "
			    "cur = Y, new = Y\n", pkgver, entry_pname);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = Y, New = Z
		 *
		 * Install new file as <file>.new-<version>
		 */
		} else  if ((strcmp(sha256_orig, sha256_cur)) &&
			    (strcmp(sha256_cur, sha256_new)) &&
			    (strcmp(sha256_orig, sha256_new))) {
			const char *version;

			version = xbps_pkg_version(pkgver);
			assert(version);
			buf = xbps_xasprintf(".%s.new-%s",
			    cffile, version);
			xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Installing new configuration file to "
			    "`%s.new-%s'.", cffile, version);
			archive_entry_set_pathname(entry, buf);
			free(buf);
			rv = 1;
			break;
		}
	}

out:
	if (sha256_orig)
		free(sha256_orig);
	if (sha256_cur)
		free(sha256_cur);

	xbps_object_iterator_release(iter);

	xbps_dbg_printf(xhp, "%s: conf_file %s returned %d\n",
	    pkgver, entry_pname, rv);

	return rv;
}
