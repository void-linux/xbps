/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
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

static xbps_array_t
merge_filelist(xbps_dictionary_t d)
{
	xbps_array_t a, result;
	xbps_dictionary_t filed;
	unsigned int i;

	result = xbps_array_create();
	assert(result);

	if ((a = xbps_dictionary_get(d, "files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_array_add(result, filed);
		}
	}
	if ((a = xbps_dictionary_get(d, "links"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_array_add(result, filed);
		}
	}
	if ((a = xbps_dictionary_get(d, "conf_files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_array_add(result, filed);
		}
	}
	if ((a = xbps_dictionary_get(d, "dirs"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_array_add(result, filed);
		}
	}

	return result;
}

xbps_array_t
xbps_find_pkg_obsoletes(struct xbps_handle *xhp,
			xbps_dictionary_t instd,
			xbps_dictionary_t newd)
{
	xbps_array_t instfiles, newfiles, obsoletes;
	xbps_object_t obj, obj2;
	xbps_string_t oldstr, newstr;
	/* These are symlinks in Void and must not be removed */
	const char *basesymlinks[] = {
		"./bin",
		"./sbin",
		"./lib",
		"./lib32",
		"./lib64",
		"./usr/lib32",
		"./usr/lib64",
		"./var/run",
	};
	const char *oldhash;
	char file[PATH_MAX];
	int rv = 0;
	bool found;

	assert(xbps_object_type(instd) == XBPS_TYPE_DICTIONARY);
	assert(xbps_object_type(newd) == XBPS_TYPE_DICTIONARY);

	obsoletes = xbps_array_create();
	assert(obsoletes);

	instfiles = merge_filelist(instd);
	if (xbps_array_count(instfiles) == 0) {
		/* nothing to check if current pkg does not own any file */
		xbps_object_release(instfiles);
		return obsoletes;
	}
	newfiles = merge_filelist(newd);

	/*
	 * Iterate over files list from installed package.
	 */
	for (unsigned int i = 0; i < xbps_array_count(instfiles); i++) {
		found = false;
		obj = xbps_array_get(instfiles, i);
		if (xbps_object_type(obj) != XBPS_TYPE_DICTIONARY) {
			/* ignore unexistent files */
			continue;
		}
		oldstr = xbps_dictionary_get(obj, "file");
		if (oldstr == NULL)
			continue;

		snprintf(file, sizeof(file), ".%s", xbps_string_cstring_nocopy(oldstr));

		oldhash = NULL;
		xbps_dictionary_get_cstring_nocopy(obj, "sha256", &oldhash);
		if (oldhash) {
			rv = xbps_file_hash_check(file, oldhash);
			if (rv == ENOENT || rv == ERANGE) {
				/*
				 * Skip unexistent and files that do not
				 * match the hash.
				 */
				continue;
			}
		}
		/*
		 * Check if current file is available in new pkg filelist.
		 */
		for (unsigned int x = 0; x < xbps_array_count(newfiles); x++) {
			obj2 = xbps_array_get(newfiles, x);
			newstr = xbps_dictionary_get(obj2, "file");
			assert(newstr);
			/*
			 * Skip files with same path.
			 */
			if (xbps_string_equals(oldstr, newstr)) {
				found = true;
				break;
			}
		}
		if (found) {
			continue;
		}
		/*
		 * Make sure to not remove any symlink of root directory.
		 */
		found = false;
		for (uint8_t x = 0; x < __arraycount(basesymlinks); x++) {
			if (strcmp(file, basesymlinks[x]) == 0) {
				found = true;
				xbps_dbg_printf(xhp, "[obsoletes] ignoring "
				    "%s removal\n", file);
				break;
			}
		}
		if (found) {
			continue;
		}
		/*
		 * Obsolete found, add onto the array.
		 */
		xbps_dbg_printf(xhp, "found obsolete: %s\n", file);
		xbps_array_add_cstring(obsoletes, file);
	}
	xbps_object_release(instfiles);
	xbps_object_release(newfiles);

	return obsoletes;
}
