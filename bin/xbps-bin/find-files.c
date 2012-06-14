/*-
 * Copyright (c) 2010-2012 Juan Romero Pardines.
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
#include <fnmatch.h>
#include <dirent.h>

#include <xbps_api.h>
#include "../xbps-repo/defs.h"
#include "defs.h"

static int
match_files_by_pattern(prop_dictionary_t pkg_filesd,
		       prop_dictionary_keysym_t key,
		       int npatterns,
		       char **patterns,
		       const char *pkgname)
{
	prop_object_iterator_t iter;
	prop_array_t array;
	prop_object_t obj;
	const char *keyname, *filestr, *typestr;
	int i;

	keyname = prop_dictionary_keysym_cstring_nocopy(key);
	array = prop_dictionary_get_keysym(pkg_filesd, key);

	if (strcmp(keyname, "files") == 0)
		typestr = "regular file";
	else if (strcmp(keyname, "dirs") == 0)
		typestr = "directory";
	else if (strcmp(keyname, "links") == 0)
		typestr = "link";
	else
		typestr = "configuration file";

	iter = prop_array_iterator(array);
	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "file", &filestr);
		for (i = 1; i < npatterns; i++) {
			if ((strcmp(filestr, patterns[i]) == 0) ||
			    (xbps_pkgpattern_match(filestr, patterns[i]) == 1))
				printf("%s: %s (%s)\n", pkgname, filestr, typestr);
		}
	}
	prop_object_iterator_release(iter);

	return 0;
}

int
find_files_in_packages(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	prop_dictionary_t pkg_filesd;
	prop_array_t files_keys;
	DIR *dirp;
	struct dirent *dp;
	char *path;
	int rv = 0;
	unsigned int i, count;

	path = xbps_xasprintf("%s/metadata", xhp->metadir);
	if (path == NULL)
		return -1;

	if ((dirp = opendir(path)) == NULL) {
		free(path);
		return -1;
	}

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		pkg_filesd = xbps_dictionary_from_metadata_plist(xhp,
		    dp->d_name, XBPS_PKGFILES);
		if (pkg_filesd == NULL) {
			if (errno == ENOENT)
				continue;
			rv = -1;
			break;
		}
		files_keys = prop_dictionary_all_keys(pkg_filesd);
		count = prop_array_count(files_keys);
		for (i = 0; i < count; i++) {
			rv = match_files_by_pattern(pkg_filesd,
			    prop_array_get(files_keys, i),
			    npatterns, patterns, dp->d_name);
			if (rv == -1)
				break;
		}
		prop_object_release(files_keys);
		prop_object_release(pkg_filesd);
		if (rv == -1)
			break;
	}
	(void)closedir(dirp);
	free(path);

	return rv;
}
