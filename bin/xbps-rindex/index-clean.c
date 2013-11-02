/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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

#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>

#include <xbps.h>
#include "defs.h"

static int
idx_cleaner_cb(struct xbps_handle *xhp,
		xbps_object_t obj,
		const char *key _unused,
		void *arg,
		bool *done _unused)
{

	xbps_array_t result = arg;
	const char *arch, *pkgver, *sha256, *repoloc;
	char *filen;

	xbps_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);

	xbps_dbg_printf(xhp, "%s: checking %s [%s] ...", pkgver, arch);

	filen = xbps_xasprintf("%s/%s.%s.xbps", repoloc, pkgver, arch);
	if (access(filen, R_OK) == -1) {
		/*
		 * File cannot be read, might be permissions,
		 * broken or simply unexistent; either way, remove it.
		 */
		xbps_array_add_cstring_nocopy(result, pkgver);
	} else {
		/*
		 * File can be read; check its hash.
		 */
		xbps_dictionary_get_cstring_nocopy(obj,
				"filename-sha256", &sha256);
		if (xbps_file_hash_check(filen, sha256) != 0)
			xbps_array_add_cstring_nocopy(result, pkgver);
	}
	free(filen);
	return 0;
}

/*
 * Removes stalled pkg entries in repository's XBPS_REPOIDX file, if any
 * binary package cannot be read (unavailable, not enough perms, etc).
 */
int
index_clean(struct xbps_handle *xhp, const char *repodir)
{
	struct xbps_repo *repo;
	xbps_array_t result = NULL, allkeys;
	xbps_dictionary_t idx, idxfiles;
	const char *keyname;
	char *pkgname;
	int rv = 0;
	bool flush = false;

	repo = xbps_repo_open(xhp, repodir);
	if (repo == NULL) {
		if (errno == ENOENT)
			return 0;
		fprintf(stderr, "index: cannot read repository data: %s\n", strerror(errno));
		return -1;
	}
	xbps_repo_open_idxfiles(repo);
	idx = xbps_dictionary_copy(repo->idx);
	idxfiles = xbps_dictionary_copy(repo->idxfiles);
	xbps_repo_close(repo);
	if (idx == NULL || idxfiles == NULL) {
		fprintf(stderr, "incomplete repository data file!");
		return -1;
	}
	printf("Cleaning `%s' index, please wait...\n", repodir);

	result = xbps_array_create();
	allkeys = xbps_dictionary_all_keys(idx);
	rv = xbps_array_foreach_cb(xhp, allkeys, idx, idx_cleaner_cb, result);

	for (unsigned int x = 0; x < xbps_array_count(result); x++) {
		xbps_array_get_cstring_nocopy(result, x, &keyname);
		printf("index: removed entry %s\n", keyname);
		pkgname = xbps_pkg_name(keyname);
		xbps_dictionary_remove(idx, pkgname);
		xbps_dictionary_remove(idxfiles, keyname);
		free(pkgname);
		flush = true;
	}
	xbps_object_release(result);
	xbps_object_release(allkeys);

	if (flush) {
		if (!repodata_flush(xhp, repodir, idx, idxfiles, NULL)) {
			fprintf(stderr, "failed to write repodata: %s\n",
			    strerror(errno));
			return -1;
		}
	}
	printf("index: %u packages registered.\n",
			xbps_dictionary_count(idx));
	printf("index-files: %u packages registered.\n",
			xbps_dictionary_count(idxfiles));

	xbps_object_release(idx);
	xbps_object_release(idxfiles);

	return rv;
}
