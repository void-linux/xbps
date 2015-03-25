/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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

struct cbdata {
	xbps_array_t result;
	xbps_dictionary_t idx;
	pthread_mutex_t mtx;
	const char *repourl;
};

static int
idx_cleaner_cb(struct xbps_handle *xhp,
		xbps_object_t obj,
		const char *key _unused,
		void *arg,
		bool *done _unused)
{
	struct cbdata *cbd = arg;
	const char *arch, *pkgver, *sha256;
	char *filen;

	xbps_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(obj, "repository", &cbd->repourl);

	xbps_dbg_printf(xhp, "%s: checking %s [%s] ...", pkgver, arch);

	filen = xbps_xasprintf("%s/%s.%s.xbps", cbd->repourl, pkgver, arch);
	if (access(filen, R_OK) == -1) {
		/*
		 * File cannot be read, might be permissions,
		 * broken or simply unexistent; either way, remove it.
		 */
		xbps_array_add_cstring_nocopy(cbd->result, pkgver);
	} else {
		/*
		 * File can be read; check its hash.
		 */
		xbps_dictionary_get_cstring_nocopy(obj,
				"filename-sha256", &sha256);
		if (xbps_file_hash_check(filen, sha256) != 0) {
			pthread_mutex_lock(&cbd->mtx);
			xbps_array_add_cstring_nocopy(cbd->result, pkgver);
			pthread_mutex_unlock(&cbd->mtx);
		}
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
	xbps_array_t allkeys;
	xbps_dictionary_t idx = NULL, idxmeta = NULL;
	struct xbps_repo *repo;
	struct cbdata cbd;
	char *rlockfname = NULL;
	int rv = 0, rlockfd = -1;
	bool flush = false;

	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		rv = errno;
		fprintf(stderr, "%s: cannot lock repository: %s\n",
		    _XBPS_RINDEX, strerror(rv));
		return rv;
	}
	repo = xbps_repo_open(xhp, repodir);
	if (repo == NULL) {
		rv = errno;
		if (rv == ENOENT)
			return 0;

		fprintf(stderr, "%s: cannot read repository data: %s\n",
		    _XBPS_RINDEX, strerror(errno));
		return rv;
	}
	idx = xbps_dictionary_copy(repo->idx);
	idxmeta = xbps_dictionary_copy(repo->idxmeta);
	if (idx == NULL) {
		fprintf(stderr, "%s: incomplete repository data file!\n", _XBPS_RINDEX);
		rv = EINVAL;
		goto out;
	}
	printf("Cleaning `%s' index, please wait...\n", repodir);

	/*
	 * First pass: find out obsolete entries on index and index-files.
	 */
	cbd.repourl = repodir;
	cbd.result = xbps_array_create();
	pthread_mutex_init(&cbd.mtx, NULL);

	allkeys = xbps_dictionary_all_keys(idx);
	(void)xbps_array_foreach_cb_multi(xhp, allkeys, idx, idx_cleaner_cb, &cbd);
	for (unsigned int x = 0; x < xbps_array_count(cbd.result); x++) {
		char *keyname = NULL, *pkgname = NULL;

		xbps_array_get_cstring(cbd.result, x, &keyname);
		printf("index: removed entry %s\n", keyname);
		pkgname = xbps_pkg_name(keyname);
		assert(pkgname);
		xbps_dictionary_remove(idx, pkgname);
		free(pkgname);
		free(keyname);
		flush = true;
	}
	pthread_mutex_destroy(&cbd.mtx);
	xbps_object_release(cbd.result);
	xbps_object_release(allkeys);

	if (flush) {
		if (!repodata_flush(xhp, repodir, idx, idxmeta)) {
			rv = errno;
			fprintf(stderr, "failed to write repodata: %s\n",
			    strerror(errno));
			goto out;
		}
	}
	printf("index: %u packages registered.\n",
			xbps_dictionary_count(idx));

out:
	xbps_repo_close(repo);
	xbps_repo_unlock(rlockfd, rlockfname);

	if (idx)
		xbps_object_release(idx);
	if (idxmeta)
		xbps_object_release(idxmeta);

	return rv;
}
