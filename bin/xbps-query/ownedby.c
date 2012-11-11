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
		for (i = 0; i < npatterns; i++) {
			if ((xbps_pkgpattern_match(filestr, patterns[i])) ||
			    (strcmp(filestr, patterns[i]) == 0))
				printf("%s: %s (%s)\n", pkgname, filestr, typestr);
		}
	}
	prop_object_iterator_release(iter);

	return 0;
}

int
ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	prop_dictionary_t pkg_filesd;
	prop_array_t files_keys;
	DIR *dirp;
	struct dirent *dp;
	char *path;
	int rv = 0;
	unsigned int i, count;

	path = xbps_xasprintf("%s/metadata", xhp->metadir);
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

struct ffdata {
	int npatterns;
	char **patterns;
	const char *repouri;
};

static void
repo_match_files_by_pattern(struct xbps_handle *xhp,
			    prop_dictionary_t pkg_filesd,
			    struct ffdata *ffd)
{
	prop_array_t array;
	const char *filestr, *pkgver, *arch;
	size_t i;
	int x;

	prop_dictionary_get_cstring_nocopy(pkg_filesd, "architecture", &arch);
	if (!xbps_pkg_arch_match(xhp, arch, NULL))
		return;

	array = prop_dictionary_get(pkg_filesd, "files");
	for (i = 0; i < prop_array_count(array); i++) {
		prop_array_get_cstring_nocopy(array, i, &filestr);
		for (x = 0; x < ffd->npatterns; x++) {
			if ((xbps_pkgpattern_match(filestr, ffd->patterns[x])) ||
			    (strcmp(filestr, ffd->patterns[x]) == 0)) {
				prop_dictionary_get_cstring_nocopy(pkg_filesd,
				    "pkgver", &pkgver);
				printf("%s: %s (%s)\n",
				    pkgver, filestr, ffd->repouri);
			}
		}
	}
}

static int
repo_ownedby_cb(struct xbps_handle *xhp,
		struct xbps_rpool_index *rpi,
		void *arg,
		bool *done)
{
	prop_array_t idxfiles;
	struct ffdata *ffd = arg;
	char *plist;
	unsigned int i;

	(void)done;

	if ((plist = xbps_pkg_index_files_plist(xhp, rpi->uri)) == NULL)
		return ENOMEM;

	if ((idxfiles = prop_array_internalize_from_zfile(plist)) == NULL) {
		free(plist);
		if (errno == ENOENT) {
			fprintf(stderr, "%s: index-files missing! "
			    "ignoring...\n", rpi->uri);
			return 0;
		}
		return errno;
	}
	free(plist);
	ffd->repouri = rpi->uri;

	for (i = 0; i < prop_array_count(idxfiles); i++)
		repo_match_files_by_pattern(xhp,
		    prop_array_get(idxfiles, i), ffd);

	prop_object_release(idxfiles);
	return 0;
}

int
repo_ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;
	int rv;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	if ((rv = xbps_rpool_sync(xhp, XBPS_PKGINDEX_FILES, NULL)) != 0)
		return rv;

	return xbps_rpool_foreach(xhp, repo_ownedby_cb, &ffd);
}
