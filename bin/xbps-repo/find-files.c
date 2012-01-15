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

#include <xbps_api.h>
#include "defs.h"

struct ffdata {
	int npatterns;
	char **patterns;
};

static void
match_files_by_pattern(prop_dictionary_t pkg_filesd, struct ffdata *ffd)
{
	prop_object_iterator_t iter;
	prop_array_t array, allkeys;
	prop_object_t obj;
	prop_dictionary_keysym_t key;
	const char *keyname, *filestr, *typestr, *pkgver;
	size_t i;
	int x;

	allkeys = prop_dictionary_all_keys(pkg_filesd);
	for (i = 0; i < prop_array_count(allkeys); i++) {
		key = prop_array_get(allkeys, i);
		keyname = prop_dictionary_keysym_cstring_nocopy(key);
		array = prop_dictionary_get_keysym(pkg_filesd, key);
		if (prop_object_type(array) != PROP_TYPE_ARRAY)
			break;

		if (strcmp(keyname, "files") == 0)
			typestr = "regular file";
		else if (strcmp(keyname, "links") == 0)
			typestr = "link";
		else
			typestr = "configuration file";

		iter = prop_array_iterator(array);
		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &filestr);
			for (x = 1; x < ffd->npatterns; x++) {
				if ((strcmp(filestr, ffd->patterns[x]) == 0) ||
				    (strstr(filestr, ffd->patterns[x])) ||
				     (xbps_pkgpattern_match(filestr,
				      ffd->patterns[x]) == 1)) {
					prop_dictionary_get_cstring_nocopy(
					    pkg_filesd, "pkgver", &pkgver);
					printf(" %s: %s (%s)\n",
					    pkgver, filestr, typestr);
				}
			}
		}
		prop_object_iterator_release(iter);
	}
	prop_object_release(allkeys);
}

static int
find_files_in_package(struct repository_pool_index *rpi, void *arg, bool *done)
{
	prop_array_t idxfiles;
	struct ffdata *ffd = arg;
	char *plist;
	unsigned int i;

	(void)done;

	printf("Looking in repository '%s', please wait...\n", rpi->rpi_uri);
	plist = xbps_pkg_index_files_plist(rpi->rpi_uri);
	if (plist == NULL)
		return ENOMEM;

	idxfiles = prop_array_internalize_from_zfile(plist);
	if (idxfiles == NULL) {
		free(plist);
		return errno;
	}
	free(plist);

	for (i = 0; i < prop_array_count(idxfiles); i++)
		match_files_by_pattern(prop_array_get(idxfiles, i), ffd);

	prop_object_release(idxfiles);
	return 0;
}

int
repo_find_files_in_packages(int npatterns, char **patterns)
{
	struct ffdata *ffd;
	int rv;

	ffd = malloc(sizeof(*ffd));
	if (ffd == NULL)
		return ENOMEM;

	ffd->npatterns = npatterns;
	ffd->patterns = patterns;
	rv = xbps_repository_pool_foreach(find_files_in_package, ffd);
	free(ffd);
	return rv;
}
