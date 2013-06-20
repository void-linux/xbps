/*-
 * Copyright (c) 2010-2013 Juan Romero Pardines.
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
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

struct ffdata {
	int npatterns;
	char **patterns;
	const char *repouri;
};

static void
match_files_by_pattern(xbps_dictionary_t pkg_filesd,
		       xbps_dictionary_keysym_t key,
		       struct ffdata *ffd,
		       const char *pkgver)
{
	xbps_array_t array;
	xbps_object_t obj;
	const char *keyname, *filestr, *typestr;
	unsigned int i;
	int x;

	keyname = xbps_dictionary_keysym_cstring_nocopy(key);

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

	array = xbps_dictionary_get_keysym(pkg_filesd, key);
	for (i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		xbps_dictionary_get_cstring_nocopy(obj, "file", &filestr);
		if (filestr == NULL)
			continue;
		for (x = 0; x < ffd->npatterns; x++) {
			if ((fnmatch(ffd->patterns[x], filestr, FNM_PERIOD)) == 0) {
				printf("%s: %s (%s)\n", pkgver,
				    filestr, typestr);
			}
		}
	}
}

static int
ownedby_pkgdb_cb(struct xbps_handle *xhp, xbps_object_t obj, void *arg, bool *done)
{
	xbps_dictionary_t pkgmetad;
	xbps_array_t files_keys;
	struct ffdata *ffd = arg;
	unsigned int i;
	const char *pkgver;

	(void)done;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	pkgmetad = xbps_pkgdb_get_pkg_metadata(xhp, pkgver);
	assert(pkgmetad);

	files_keys = xbps_dictionary_all_keys(pkgmetad);
	for (i = 0; i < xbps_array_count(files_keys); i++) {
		match_files_by_pattern(pkgmetad,
		    xbps_array_get(files_keys, i), ffd, pkgver);
	}
	return 0;
}

int
ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;
	char *rfile;
	int i;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	for (i = 0; i < npatterns; i++) {
		rfile = realpath(patterns[i], NULL);
		if (rfile)
			patterns[i] = rfile;
	}
	return xbps_pkgdb_foreach_cb(xhp, ownedby_pkgdb_cb, &ffd);
}

static void
repo_match_files_by_pattern(xbps_array_t files,
			const char *pkgver,
			struct ffdata *ffd)
{
	const char *filestr;
	unsigned int i;
	int x;

	for (i = 0; i < xbps_array_count(files); i++) {
		xbps_array_get_cstring_nocopy(files, i, &filestr);
		for (x = 0; x < ffd->npatterns; x++) {
			if ((fnmatch(ffd->patterns[x], filestr, FNM_PERIOD)) == 0) {
				printf("%s: %s (%s)\n",
				    pkgver, filestr, ffd->repouri);
			}
		}
	}
}

static int
repo_ownedby_cb(struct xbps_repo *repo, void *arg, bool *done)
{
	xbps_array_t allkeys, pkgar;
	xbps_dictionary_t filesd;
	xbps_dictionary_keysym_t ksym;
	struct ffdata *ffd = arg;
	const char *pkgver;
	unsigned int i;

	(void)done;

	filesd = xbps_repo_get_plist(repo, XBPS_PKGINDEX_FILES);
	ffd->repouri = repo->uri;
	allkeys = xbps_dictionary_all_keys(filesd);

	for (i = 0; i < xbps_array_count(allkeys); i++) {
		ksym = xbps_array_get(allkeys, i);
		pkgar = xbps_dictionary_get_keysym(filesd, ksym);
		pkgver = xbps_dictionary_keysym_cstring_nocopy(ksym);
		repo_match_files_by_pattern(pkgar, pkgver, ffd);
	}

	return 0;
}

int
repo_ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;
	char *rfile;
	int i;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	for (i = 0; i < npatterns; i++) {
		rfile = realpath(patterns[i], NULL);
		if (rfile)
			patterns[i] = rfile;
	}
	return xbps_rpool_foreach(xhp, repo_ownedby_cb, &ffd);
}
