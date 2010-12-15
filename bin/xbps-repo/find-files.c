/*-
 * Copyright (c) 2010 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"

static int
match_files_by_pattern(prop_dictionary_t pkg_filesd, prop_dictionary_keysym_t key,
		       const char *pattern, const char *pkgver)
{
	prop_object_iterator_t iter;
	prop_array_t array;
	prop_object_t obj;
	const char *keyname, *filestr, *typestr;

	keyname = prop_dictionary_keysym_cstring_nocopy(key);
	array = prop_dictionary_get_keysym(pkg_filesd, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY)
		return 0;

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
		if ((strcmp(filestr, pattern) == 0) ||
		    (strstr(filestr, pattern)) ||
		    (xbps_pkgpattern_match(filestr, __UNCONST(pattern)) == 1))
			printf(" %s: %s (%s)\n", pkgver, filestr, typestr);
	}
	prop_object_iterator_release(iter);
	return 0;
}

static int
find_files_in_package(struct repository_pool_index *rpi, void *arg, bool *done)
{
	prop_dictionary_t pkg_filesd;
	prop_array_t repo_pkgs, files_keys;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname, *pkgver, *pattern = arg;
	char *url;
	int rv = 0;
	unsigned int i, count;

	(void)done;

	repo_pkgs = prop_dictionary_get(rpi->rpi_repod, "packages");
	if (repo_pkgs == NULL)
		return -1;

	iter = prop_array_iterator(repo_pkgs);
	if (iter == NULL)
		return -1;

	printf("Looking in repository '%s', please wait...\n", rpi->rpi_uri);
	while ((obj = prop_object_iterator_next(iter))) {
		url = xbps_repository_get_path_from_pkg_dict(obj, rpi->rpi_uri);
		if (url == NULL) {
			rv = -1;
			break;
		}
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		pkg_filesd = xbps_repository_get_pkg_plist_dict_from_url(url,
		    XBPS_PKGFILES);
		free(url);
		if (pkg_filesd == NULL) {
			fprintf(stderr, "E: couldn't get '%s' from '%s' "
			    "in repo '%s: %s'\n", XBPS_PKGFILES, pkgname, rpi->rpi_uri,
			    strerror(errno));
			rv = -1;
			break;
		}
		files_keys = prop_dictionary_all_keys(pkg_filesd);
		count = prop_array_count(files_keys);
		for (i = 0; i < count; i++) {
			rv = match_files_by_pattern(pkg_filesd,
			    prop_array_get(files_keys, i), pattern, pkgver);
			if (rv == -1)
				break;
		}
		prop_object_release(pkg_filesd);
		if (rv == -1)
			break;
	}
	prop_object_iterator_release(iter);
	return rv;
}

int
repo_find_files_in_packages(const char *pattern)
{
	return xbps_repository_pool_foreach(find_files_in_package,
	    __UNCONST(pattern));
}
