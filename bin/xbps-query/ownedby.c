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

#include <xbps.h>
#include "defs.h"

struct ffdata {
	int npatterns;
	char **patterns;
	const char *repouri;
	xbps_array_t allkeys;
	xbps_dictionary_t filesd;
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
	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		filestr = NULL;
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
ownedby_pkgdb_cb(struct xbps_handle *xhp,
		xbps_object_t obj,
		const char *obj_key _unused,
		void *arg,
		bool *done _unused)
{
	xbps_dictionary_t pkgmetad;
	xbps_array_t files_keys;
	struct ffdata *ffd = arg;
	const char *pkgver;

	(void)obj_key;
	(void)done;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

	pkgmetad = xbps_pkgdb_get_pkg_metadata(xhp, pkgver);
	assert(pkgmetad);

	files_keys = xbps_dictionary_all_keys(pkgmetad);
	for (unsigned int i = 0; i < xbps_array_count(files_keys); i++) {
		match_files_by_pattern(pkgmetad,
		    xbps_array_get(files_keys, i), ffd, pkgver);
	}
	xbps_object_release(pkgmetad);
	xbps_object_release(files_keys);

	return 0;
}

int
ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;
	char *rfile;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	for (int i = 0; i < npatterns; i++) {
		rfile = realpath(patterns[i], NULL);
		if (rfile)
			patterns[i] = rfile;
	}
	return xbps_pkgdb_foreach_cb(xhp, ownedby_pkgdb_cb, &ffd);
}

static int
repo_match_cb(struct xbps_handle *xhp _unused,
		xbps_object_t obj,
		const char *key,
		void *arg,
		bool *done _unused)
{
	struct ffdata *ffd = arg;
	const char *filestr;

	for (unsigned int i = 0; i < xbps_array_count(obj); i++) {
		xbps_array_get_cstring_nocopy(obj, i, &filestr);
		for (int x = 0; x < ffd->npatterns; x++) {
			if ((fnmatch(ffd->patterns[x], filestr, FNM_PERIOD)) == 0) {
				printf("%s: %s (%s)\n",
				    key, filestr, ffd->repouri);
			}
		}
	}

	return 0;
}

static int
repo_ownedby_cb(struct xbps_repo *repo, void *arg, bool *done _unused)
{
	xbps_array_t allkeys;
	xbps_dictionary_t filesd;
	struct ffdata *ffd = arg;
	int rv;

	filesd = xbps_repo_get_plist(repo, XBPS_PKGINDEX_FILES);
	if (filesd == NULL)
		return 0;

	ffd->repouri = repo->uri;
	allkeys = xbps_dictionary_all_keys(filesd);
	rv = xbps_array_foreach_cb(repo->xhp, allkeys, filesd, repo_match_cb, ffd);
	xbps_object_release(filesd);
	xbps_object_release(allkeys);

	return rv;
}

int
repo_ownedby(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct ffdata ffd;
	char *rfile;
	int rv;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	for (int i = 0; i < npatterns; i++) {
		rfile = realpath(patterns[i], NULL);
		if (rfile)
			patterns[i] = rfile;
	}
	rv = xbps_rpool_foreach(xhp, repo_ownedby_cb, &ffd);

	return rv;
}
