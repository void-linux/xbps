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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbps.h>
#include "defs.h"

static int
remove_pkg(const char *repodir, const char *file)
{
	char *filepath, *sigpath, *sig2path;
	int rv = 0;

	filepath = xbps_xasprintf("%s/%s", repodir, file);
	sigpath = xbps_xasprintf("%s.sig", filepath);
	sig2path = xbps_xasprintf("%s.sig2", filepath);
	if (remove(filepath) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			xbps_error_printf("xbps-rindex: failed to remove "
			    "package `%s': %s\n", file, strerror(rv));
		}
	}
	if (remove(sigpath) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			xbps_error_printf("xbps-rindex: failed to remove "
			    "legacy package signature `%s': %s\n", sigpath, strerror(rv));
		}
	}
	if (remove(sig2path) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			xbps_error_printf("xbps-rindex: failed to remove "
			    "package signature `%s': %s\n", sig2path, strerror(rv));
		}
	}
	free(sigpath);
	free(sig2path);
	free(filepath);

	return rv;
}

static int
cleaner_cb(struct xbps_handle *xhp, xbps_object_t obj, const char *key UNUSED, void *arg, bool *done UNUSED)
{
	struct xbps_repo *repo = ((struct xbps_repo **)arg)[0], *stage = ((struct xbps_repo **)arg)[1];
	const char *binpkg;
	char *pkgver, *arch = NULL;
	int rv;

	binpkg = xbps_string_cstring_nocopy(obj);
	if (access(binpkg, R_OK) == -1) {
		if (errno == ENOENT) {
			if ((rv = remove_pkg(repo->uri, binpkg)) != 0)
				return 0;

			printf("Removed broken package `%s'.\n", binpkg);
		}
	}
	arch = xbps_binpkg_arch(binpkg);
	assert(arch);
	/* ignore pkgs from other archs */
	if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
		free(arch);
		return 0;
	}
	free(arch);

	pkgver = xbps_binpkg_pkgver(binpkg);
	assert(pkgver);
	if (xhp->flags & XBPS_FLAG_VERBOSE)
		printf("checking %s (%s)\n", pkgver, binpkg);
	/*
	 * If binpkg is not registered in index, remove binpkg.
	 */
	if (!xbps_repo_get_pkg(repo, pkgver) && !(stage && xbps_repo_get_pkg(stage, pkgver))) {
		if ((rv = remove_pkg(repo->uri, binpkg)) != 0) {
			free(pkgver);
			return 0;
		}
		printf("Removed obsolete package `%s'.\n", binpkg);
	}
	free(pkgver);
	return 0;
}

int
remove_obsoletes(struct xbps_handle *xhp, const char *repodir)
{
	xbps_array_t array = NULL;
	struct xbps_repo *repos[2], *repo, *stage;
	DIR *dirp;
	struct dirent *dp;
	char *ext;
	int rv = 0;

	repo = xbps_repo_public_open(xhp, repodir);
	if (repo == NULL) {
		if (errno != ENOENT) {
			xbps_error_printf("xbps-rindex: cannot read repository data: %s\n",
			    strerror(errno));
			return -1;
		}
		return 0;
	}
	stage = xbps_repo_stage_open(xhp, repodir);
	if (chdir(repodir) == -1) {
		xbps_error_printf("xbps-rindex: cannot chdir to %s: %s\n",
		    repodir, strerror(errno));
		rv = errno;
		goto out;
	}
	if ((dirp = opendir(".")) == NULL) {
		xbps_error_printf("xbps-rindex: failed to open %s: %s\n",
		    repodir, strerror(errno));
		rv = errno;
		goto out;
	}
	while ((dp = readdir(dirp))) {
		if (strcmp(dp->d_name, "..") == 0)
			continue;
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".xbps"))
			continue;
		if (array == NULL)
			array = xbps_array_create();

		xbps_array_add_cstring(array, dp->d_name);
	}
	(void)closedir(dirp);

	repos[0] = repo;
	repos[1] = stage;
	rv = xbps_array_foreach_cb_multi(xhp, array, NULL, cleaner_cb, repos);
out:
	xbps_repo_release(repo);
	xbps_repo_release(stage);
	xbps_object_release(array);

	return rv;
}
