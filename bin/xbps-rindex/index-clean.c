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

static xbps_dictionary_t dest;

struct CleanerCbInfo {
	const char *repourl;
	bool hashcheck;
};
static int
idx_cleaner_cb(struct xbps_handle *xhp,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	struct CleanerCbInfo *info = arg;
	const char *arch, *pkgver, *sha256;
	char *filen, *pkgname;

	xbps_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

	xbps_dbg_printf(xhp, "%s: checking %s [%s] ...\n", info->repourl, pkgver, arch);

	filen = xbps_xasprintf("%s/%s.%s.xbps", info->repourl, pkgver, arch);
	if (access(filen, R_OK) == -1) {
		/*
		 * File cannot be read, might be permissions,
		 * broken or simply unexistent; either way, remove it.
		 */
		pkgname = xbps_pkg_name(pkgver);
		if (pkgname == NULL)
			goto out;
		xbps_dictionary_remove(dest, pkgname);
		free(pkgname);
		printf("index: removed pkg %s\n", pkgver);
	} else if (info->hashcheck) {
		/*
		 * File can be read; check its hash.
		 */
		xbps_dictionary_get_cstring_nocopy(obj,
				"filename-sha256", &sha256);
		if (xbps_file_hash_check(filen, sha256) != 0) {
			pkgname = xbps_pkg_name(pkgver);
			if (pkgname == NULL)
				goto out;
			xbps_dictionary_remove(dest, pkgname);
			free(pkgname);
			printf("index: removed pkg %s\n", pkgver);
		}
	}
out:
	free(filen);
	return 0;
}

static int
cleanup_repo(struct xbps_handle *xhp, const char *repodir, struct xbps_repo *repo,
		const char *reponame, bool hashcheck) {
	int rv = 0;
	xbps_array_t allkeys;
	struct CleanerCbInfo info = {
		.hashcheck = hashcheck,
		.repourl = repodir
	};
	/*
	 * First pass: find out obsolete entries on index and index-files.
	 */
	dest = xbps_dictionary_copy_mutable(repo->idx);
	allkeys = xbps_dictionary_all_keys(dest);
	(void)xbps_array_foreach_cb_multi(xhp, allkeys, repo->idx, idx_cleaner_cb, &info);
	xbps_object_release(allkeys);

	if(strcmp("stagedata", reponame) == 0 && xbps_dictionary_count(dest) == 0) {
		char *stagefile = xbps_repo_path_with_name(xhp, repodir, "stagedata");
		unlink(stagefile);
		free(stagefile);
	}
	if (!xbps_dictionary_equals(dest, repo->idx)) {
		if (!repodata_flush(xhp, repodir, reponame, dest, repo->idxmeta)) {
			rv = errno;
			fprintf(stderr, "failed to write repodata: %s\n",
			    strerror(errno));
			return rv;
		}
	}
	if(strcmp("stagedata", reponame) == 0)
		printf("stage: %u packages registered.\n", xbps_dictionary_count(dest));
	else
		printf("index: %u packages registered.\n", xbps_dictionary_count(dest));
	return rv;
}

/*
 * Removes stalled pkg entries in repository's XBPS_REPOIDX file, if any
 * binary package cannot be read (unavailable, not enough perms, etc).
 */
int
index_clean(struct xbps_handle *xhp, const char *repodir, const bool hashcheck)
{
	struct xbps_repo *repo, *stage;
	char *rlockfname = NULL;
	int rv = 0, rlockfd = -1;

	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		rv = errno;
		fprintf(stderr, "%s: cannot lock repository: %s\n",
		    _XBPS_RINDEX, strerror(rv));
		return rv;
	}
	repo = xbps_repo_public_open(xhp, repodir);
	if (repo == NULL) {
		rv = errno;
		if (rv == ENOENT) {
			xbps_repo_unlock(rlockfd, rlockfname);
			return 0;
		}
		fprintf(stderr, "%s: cannot read repository data: %s\n",
		    _XBPS_RINDEX, strerror(errno));
		xbps_repo_unlock(rlockfd, rlockfname);
		return rv;
	}
	stage = xbps_repo_stage_open(xhp, repodir);
	if (repo->idx == NULL || (stage && stage->idx == NULL)) {
		fprintf(stderr, "%s: incomplete repository data file!\n", _XBPS_RINDEX);
		rv = EINVAL;
		goto out;
	}
	printf("Cleaning `%s' index, please wait...\n", repodir);

	if((rv = cleanup_repo(xhp, repodir, repo, "repodata", hashcheck)))
		goto out;
	if(stage) {
		cleanup_repo(xhp, repodir, stage, "stagedata", hashcheck);
	}

out:
	xbps_repo_close(repo);
	if(stage)
		xbps_repo_close(stage);
	xbps_repo_unlock(rlockfd, rlockfname);

	return rv;
}
