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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "xbps_api_impl.h"

static prop_array_t
merge_filelist(prop_dictionary_t d)
{
	prop_array_t a, result;
	prop_dictionary_t filed;
	size_t i;

	result = prop_array_create();
	assert(result);

	if ((a = prop_dictionary_get(d, "files"))) {
		for (i = 0; i < prop_array_count(a); i++) {
			filed = prop_array_get(a, i);
			prop_array_add(result, filed);
		}
	}
	if ((a = prop_dictionary_get(d, "links"))) {
		for (i = 0; i < prop_array_count(a); i++) {
			filed = prop_array_get(a, i);
			prop_array_add(result, filed);
		}
	}
	if ((a = prop_dictionary_get(d, "conf_files"))) {
		for (i = 0; i < prop_array_count(a); i++) {
			filed = prop_array_get(a, i);
			prop_array_add(result, filed);
		}
	}
	if ((a = prop_dictionary_get(d, "dirs"))) {
		for (i = 0; i < prop_array_count(a); i++) {
			filed = prop_array_get(a, i);
			prop_array_add(result, filed);
		}
	}

	return result;
}

prop_array_t
xbps_find_pkg_obsoletes(struct xbps_handle *xhp,
			prop_dictionary_t instd,
			prop_dictionary_t newd)
{
	prop_array_t instfiles, newfiles, obsoletes;
	prop_object_t obj, obj2;
	prop_string_t oldstr, newstr;
	size_t i, x;
	const char *oldhash;
	char *file;
	int rv = 0;
	bool found;

	assert(prop_object_type(instd) == PROP_TYPE_DICTIONARY);
	assert(prop_object_type(newd) == PROP_TYPE_DICTIONARY);

	obsoletes = prop_array_create();
	assert(obsoletes);

	instfiles = merge_filelist(instd);
	if (prop_array_count(instfiles) == 0) {
		/* nothing to check if current pkg does not own any file */
		prop_object_release(instfiles);
		return obsoletes;
	}
	newfiles = merge_filelist(newd);

	/*
	 * Iterate over files list from installed package.
	 */
	for (i = 0; i < prop_array_count(instfiles); i++) {
		found = false;
		obj = prop_array_get(instfiles, i);
		if (prop_object_type(obj) != PROP_TYPE_DICTIONARY) {
			/* ignore unexistent files */
			continue;
		}
		oldstr = prop_dictionary_get(obj, "file");
		if (oldstr == NULL)
			continue;

		file = xbps_xasprintf(".%s",
		    prop_string_cstring_nocopy(oldstr));

		oldhash = NULL;
		prop_dictionary_get_cstring_nocopy(obj,
		    "sha256", &oldhash);
		if (oldhash) {
			rv = xbps_file_hash_check(file, oldhash);
			if (rv == ENOENT || rv == ERANGE) {
				/*
				 * Skip unexistent and files that do not
				 * match the hash.
				 */
				free(file);
				continue;
			}
		}
		/*
		 * Check if current file is available in new pkg filelist.
		 */
		for (x = 0; x < prop_array_count(newfiles); x++) {
			obj2 = prop_array_get(newfiles, x);
			newstr = prop_dictionary_get(obj2, "file");
			assert(newstr);
			/*
			 * Skip files with same path.
			 */
			if (prop_string_equals(oldstr, newstr)) {
				found = true;
				break;
			}
		}
		if (found) {
			free(file);
			continue;
		}
		/*
		 * Do not add required symlinks for the
		 * system transition to /usr.
		 */
		if ((strcmp(file, "./bin") == 0) ||
		    (strcmp(file, "./bin/") == 0) ||
		    (strcmp(file, "./sbin") == 0) ||
		    (strcmp(file, "./sbin/") == 0) ||
		    (strcmp(file, "./lib") == 0) ||
		    (strcmp(file, "./lib/") == 0) ||
		    (strcmp(file, "./lib64/") == 0) ||
		    (strcmp(file, "./lib64") == 0)) {
			free(file);
			continue;
		}
		/*
		 * Obsolete found, add onto the array.
		 */
		xbps_dbg_printf(xhp, "found obsolete: %s\n", file);
		prop_array_add_cstring(obsoletes, file);
		free(file);
	}
	prop_object_release(instfiles);
	prop_object_release(newfiles);

	return obsoletes;
}
