/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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

prop_array_t
xbps_find_pkg_obsoletes(struct xbps_handle *xhp,
			prop_dictionary_t instd,
			prop_dictionary_t newd)
{
	prop_array_t array, array2, obsoletes;
	prop_object_t obj, obj2;
	prop_string_t oldstr, newstr;
	size_t i, x;
	const char *array_str = "files";
	const char *oldhash;
	char *file;
	int rv = 0;
	bool found, dodirs, dolinks, docffiles;

	dodirs = dolinks = docffiles = false;

	assert(prop_object_type(instd) == PROP_TYPE_DICTIONARY);
	assert(prop_object_type(newd) == PROP_TYPE_DICTIONARY);

	obsoletes = prop_array_create();
	assert(obsoletes);

again:
	array = prop_dictionary_get(instd, array_str);
	if (array == NULL || prop_array_count(array) == 0)
		goto out1;

	/*
	 * Iterate over files list from installed package.
	 */
	for (i = 0; i < prop_array_count(array); i++) {
		found = false;
		obj = prop_array_get(array, i);
		oldstr = prop_dictionary_get(obj, "file");
		assert(oldstr);

		file = xbps_xasprintf(".%s",
		    prop_string_cstring_nocopy(oldstr));

		if ((strcmp(array_str, "files") == 0) ||
		    (strcmp(array_str, "conf_files") == 0)) {
			prop_dictionary_get_cstring_nocopy(obj,
			    "sha256", &oldhash);
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
		array2 = prop_dictionary_get(newd, array_str);
		if (array2 && prop_array_count(array2)) {
			for (x = 0; x < prop_array_count(array2); x++) {
				obj2 = prop_array_get(array2, x);
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
		xbps_dbg_printf(xhp, "found obsolete: %s (%s)\n",
		    file, array_str);

		prop_array_add_cstring(obsoletes, file);
		free(file);
	}
out1:
	if (!dolinks) {
		dolinks = true;
		array_str = "links";
		goto again;
	} else if (!docffiles) {
		docffiles = true;
		array_str = "conf_files";
		goto again;
	} else if (!dodirs) {
		dodirs = true;
		array_str = "dirs";
		goto again;
	}

	return obsoletes;
}
