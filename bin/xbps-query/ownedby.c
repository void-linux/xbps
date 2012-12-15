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

struct ffdata {
	int npatterns;
	char **patterns;
	const char *repouri;
};

static void
match_files_by_pattern(prop_dictionary_t pkg_filesd,
		       prop_dictionary_keysym_t key,
		       struct ffdata *ffd,
		       const char *pkgver)
{
	prop_array_t array;
	prop_object_t obj;
	const char *keyname, *filestr, *typestr;
	unsigned int i;
	int x;

	keyname = prop_dictionary_keysym_cstring_nocopy(key);

	if (strcmp(keyname, "files") == 0)
		typestr = "regular file";
	else if (strcmp(keyname, "dirs") == 0)
		typestr = "directory";
	else if (strcmp(keyname, "links") == 0)
		typestr = "link";
	else if (strcmp(keyname, "conf_files") == 0)
		typestr = "configuration file";
	else
		return;

	array = prop_dictionary_get_keysym(pkg_filesd, key);
	for (i = 0; i < prop_array_count(array); i++) {
		obj = prop_array_get(array, i);
		prop_dictionary_get_cstring_nocopy(obj, "file", &filestr);
		for (x = 0; x < ffd->npatterns; x++) {
			if ((strcmp(filestr, ffd->patterns[x]) == 0) ||
			    (fnmatch(ffd->patterns[x], filestr, FNM_PERIOD)) == 0) {
				printf("%s: %s (%s)\n", pkgver,
				    filestr, typestr);
			}
		}
	}
}

static int
ownedby_pkgdb_cb(struct xbps_handle *xhp, prop_object_t obj, void *arg, bool *done)
{
	prop_dictionary_t pkgmetad;
	prop_array_t files_keys;
	struct ffdata *ffd = arg;
	unsigned int i;
	const char *pkgver;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	pkgmetad = xbps_pkgdb_get_pkg_metadata(xhp, pkgver);

	files_keys = prop_dictionary_all_keys(pkgmetad);
	for (i = 0; i < prop_array_count(files_keys); i++) {
		match_files_by_pattern(pkgmetad,
		    prop_array_get(files_keys, i), ffd, pkgver);
	}
	return 0;
}

int
ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	return xbps_pkgdb_foreach_cb(xhp, ownedby_pkgdb_cb, &ffd);
}

static void
repo_match_files_by_pattern(prop_dictionary_t pkgd,
			    struct ffdata *ffd)
{
	prop_array_t array;
	const char *filestr, *pkgver;
	size_t i;
	int x;

	array = prop_dictionary_get(pkgd, "files");
	for (i = 0; i < prop_array_count(array); i++) {
		prop_array_get_cstring_nocopy(array, i, &filestr);
		for (x = 0; x < ffd->npatterns; x++) {
			if ((strcmp(filestr, ffd->patterns[x]) == 0) ||
			    (fnmatch(ffd->patterns[x], filestr, FNM_PERIOD)) == 0) {
				prop_dictionary_get_cstring_nocopy(pkgd,
				    "pkgver", &pkgver);
				printf("%s: %s (%s)\n",
				    pkgver, filestr, ffd->repouri);
			}
		}
	}
}

static int
repo_ownedby_cb(struct xbps_rindex *rpi, void *arg, bool *done)
{
	prop_array_t allkeys;
	prop_dictionary_t pkgd, idxfiles;
	prop_dictionary_keysym_t ksym;
	struct ffdata *ffd = arg;
	char *plist;
	unsigned int i;

	(void)done;

	if ((plist = xbps_pkg_index_files_plist(rpi->xhp, rpi->uri)) == NULL)
		return ENOMEM;

	if ((idxfiles = prop_dictionary_internalize_from_zfile(plist)) == NULL) {
		if (errno == ENOENT) {
			fprintf(stderr, "%s: index-files missing! "
			    "ignoring...\n", rpi->uri);
			return 0;
		}
		return errno;
	}
	ffd->repouri = rpi->uri;
	allkeys = prop_dictionary_all_keys(idxfiles);

	for (i = 0; i < prop_array_count(allkeys); i++) {
		ksym = prop_array_get(allkeys, i);
		pkgd = prop_dictionary_get_keysym(idxfiles, ksym);
		repo_match_files_by_pattern(pkgd, ffd);
	}

	return 0;
}

int
repo_ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;
	int rv;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	if ((rv = xbps_rpool_sync(xhp, XBPS_PKGINDEX_FILES, NULL)) != 0) {
		fprintf(stderr, "xbps-query: failed to sync rindex "
		    "files: %s\n", strerror(rv));
		return rv;
	}
	return xbps_rpool_foreach(xhp, repo_ownedby_cb, &ffd);
}
