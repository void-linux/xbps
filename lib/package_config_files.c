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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

/*
 * Returns 1 if entry is a configuration file, 0 if don't or -1 on error.
 */
int HIDDEN
xbps_entry_is_a_conf_file(prop_dictionary_t propsd,
			  const char *entry_pname)
{
	prop_object_t obj;
	prop_object_iterator_t iter;
	char *cffile;
	int rv = 0;

	assert(propsd != NULL);
	assert(entry_pname != NULL);

	if (!prop_dictionary_get(propsd, "conf_files"))
		return 0;

	iter = xbps_get_array_iter_from_dict(propsd, "conf_files");
	if (iter == NULL)
		return -1;

	while ((obj = prop_object_iterator_next(iter))) {
		cffile = xbps_xasprintf(".%s",
		    prop_string_cstring_nocopy(obj));
		if (cffile == NULL) {
			rv = -1;
			goto out;
		}
		if (strcmp(cffile, entry_pname) == 0) {
			rv = 1;
			free(cffile);
			break;
		}
		free(cffile);
	}

out:
	prop_object_iterator_release(iter);
	return rv;
}

/*
 * Returns 1 if entry should be installed, 0 if don't or -1 on error.
 */
int HIDDEN
xbps_entry_install_conf_file(prop_dictionary_t filesd,
			     struct archive_entry *entry,
			     const char *entry_pname,
			     const char *pkgname,
			     const char *version)
{
	prop_dictionary_t forigd;
	prop_object_t obj, obj2;
	prop_object_iterator_t iter, iter2;
	const char *cffile, *sha256_new = NULL;
	char *buf, *sha256_cur = NULL, *sha256_orig = NULL;
	int rv = 0;

	assert(filesd != NULL);
	assert(entry != NULL);
	assert(pkgname != NULL);

	iter = xbps_get_array_iter_from_dict(filesd, "conf_files");
	if (iter == NULL)
		return -1;

	/*
	 * Get original hash for the file from current
	 * installed package.
	 */
	xbps_dbg_printf("%s: processing conf_file %s\n",
	    pkgname, entry_pname);

	forigd = xbps_get_pkg_dict_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (forigd == NULL) {
		xbps_dbg_printf("%s: conf_file %s not currently installed\n",
		    pkgname, entry_pname);
		rv = 1;
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
				rv = -1;
				goto out;
			}
			if (strcmp(entry_pname, buf) == 0) {
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
		xbps_dbg_printf("%s: conf_file %s unknown orig sha256\n",
		    pkgname, entry_pname);
		rv = 1;
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
			return -1;
		}
		if (strcmp(entry_pname, buf)) {
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
				xbps_dbg_printf("%s: conf_file %s not "
				    "installed\n", pkgname, entry_pname);
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
		 * Install new file.
		 */
		if ((strcmp(sha256_orig, sha256_cur) == 0) &&
		    (strcmp(sha256_orig, sha256_new) == 0) &&
		    (strcmp(sha256_cur, sha256_new) == 0)) {
			xbps_dbg_printf("%s: conf_file %s orig = X,"
			    "cur = X, new = X\n", pkgname, entry_pname);
			rv = 1;
			break;
		/*
		 * Orig = X, Curr = X, New = Y
		 *
		 * Install new file.
		 */
		} else if ((strcmp(sha256_orig, sha256_cur) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_cur, sha256_new))) {
			printf("Updating configuration file `%s' "
			    "with new version.\n", cffile);
			rv = 1;
			break;
		/*
		 * Orig = X, Curr = Y, New = X
		 *
		 * Keep current file as is.
		 */
		} else if ((strcmp(sha256_orig, sha256_new) == 0) &&
			   (strcmp(sha256_cur, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			printf("Keeping modified configuration file "
			    "`%s'.\n", cffile);
			rv = 0;
			break;
		/*
		 * Orig = X, Curr = Y, New = Y
		 *
		 * Install new file.
		 */
		} else if ((strcmp(sha256_cur, sha256_new) == 0) &&
			   (strcmp(sha256_orig, sha256_new)) &&
			   (strcmp(sha256_orig, sha256_cur))) {
			xbps_dbg_printf("%s: conf_file %s orig = X,"
			    "cur = Y, new = Y\n", pkgname, entry_pname);
			rv = 1;
			break;
		/*
		 * Orig = X, Curr = Y, New = Z
		 *
		 * Install new file as file.new-<pkg_version>
		 */
		} else  if ((strcmp(sha256_orig, sha256_cur)) &&
			    (strcmp(sha256_cur, sha256_new)) &&
			    (strcmp(sha256_orig, sha256_new))) {
			buf = xbps_xasprintf(".%s.new-%s",
			    cffile, version);
			if (buf == NULL) {
				rv = -1;
				break;
			}
			printf("Keeping modified configuration file "
			    "`%s'.\n", cffile);
			printf("Installing new configuration file as "
			    "`%s.new-%s'\n", cffile, version);
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

	prop_object_iterator_release(iter);

	xbps_dbg_printf("%s: conf_file %s returned %d\n",
	    pkgname, entry_pname, rv);

	return rv;
}
