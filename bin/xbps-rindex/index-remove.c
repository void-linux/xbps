/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
 * Copyright (c) 2019-2020 Piotr Wójcik
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
#include <fcntl.h>

#include <xbps.h>
#include "defs.h"

static xbps_dictionary_t idx;

int
index_remove(struct xbps_handle *xhp, int args, int argmax, char **argv, const char *compression)
{
	struct xbps_repo *repo = NULL;
	char *tmprepodir = NULL;
	const char *repodir = NULL;
	char *rlockfname = NULL;
	int rv = 0, rlockfd = -1;

	assert(argv);

	if ((tmprepodir = strdup(argv[args])) == NULL)
		return ENOMEM;
	repodir = dirname(tmprepodir);
	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		rv = errno;
		fprintf(stderr, "%s: cannot lock repository: %s\n",
		    _XBPS_RINDEX, strerror(rv));
		goto earlyout;
	}
	repo = xbps_repo_public_open(xhp, repodir);
	if (repo == NULL) {
		rv = errno;
		fprintf(stderr, "%s: cannot read repository %s data: %s\n",
		    _XBPS_RINDEX, repodir, strerror(errno));
		goto earlyout;
	}
	if (repo->idx == NULL) {
		fprintf(stderr, "%s: incomplete repository data file!\n", _XBPS_RINDEX);
		rv = EINVAL;
		goto earlyout;
	}
	idx = xbps_dictionary_copy_mutable(repo->idx);

	for (int i = args; i < argmax; i++) {
		xbps_dictionary_t curpkgd = NULL;
		const char *pkg = argv[i], *opkgver = NULL;
		char *pkgver = NULL, *arch = NULL;
		char pkgname[XBPS_NAME_SIZE];

		/*
		 * Take package properties from passed path.
		 */
		assert(pkg);
		pkgver = xbps_binpkg_pkgver(pkg);
		if (!pkgver || !xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			fprintf(stderr, "%s: argument %s doesn't look like path to binary package\n", _XBPS_RINDEX, pkg);
			free(pkgver);
			goto out;
		}
		arch = xbps_binpkg_arch(pkg);
		if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
			fprintf(stderr, "%s: ignoring %s, unmatched arch (%s)\n", _XBPS_RINDEX, pkgver, arch);
			goto again;
		}

		/*
		 * Check if this package exists already in the index
		 */
		curpkgd = xbps_dictionary_get(idx, pkgname);
		if (!curpkgd) {
			xbps_dbg_printf(xhp, "Package %s isn't indexed in %s, skipping.\n", pkgname, repodir);
			goto again;
		}


		/*
		 * Unindex.
		 */
		xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &opkgver);
		if (opkgver) {
			printf("index: unindexing %s\n", opkgver);
		} else {
			printf("index: unindexing some version of %s\n", pkgname);
		}
		xbps_dictionary_remove(idx, pkgname);

again:
		free(arch);
		free(pkgver);
	}

	/*
	 * Generate repository data files.
	 */
	if (!xbps_dictionary_equals(idx, repo->idx)) {
		if (!xbps_repodata_flush(xhp, repodir, "repodata", idx, repo->idxmeta, compression)) {
			rv = errno;
			fprintf(stderr, "%s: failed to write repodata: %s\n",
			    _XBPS_RINDEX, strerror(errno));
			goto out;
		}
	}
	printf("index: %u packages in index.\n", xbps_dictionary_count(idx));


out:
	xbps_object_release(idx);

earlyout:
	if (repo)
		xbps_repo_close(repo);
	xbps_repo_unlock(rlockfd, rlockfname);
	free(tmprepodir);

	return rv;
}
