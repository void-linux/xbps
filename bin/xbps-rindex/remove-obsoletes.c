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

#include <xbps.h>
#include "defs.h"

static int
remove_pkg(const char *repodir, const char *file)
{
	char *filepath;
	int rv;

	filepath = xbps_xasprintf("%s/%s", repodir, file);
	if (remove(filepath) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			fprintf(stderr, "xbps-rindex: failed to remove "
			    "package `%s': %s\n", file,
			    strerror(rv));
			free(filepath);
			return rv;
		}
	}
	free(filepath);

	return 0;
}

static int
cleaner_cb(struct xbps_handle *xhp, xbps_object_t obj, const char *key _unused, void *arg, bool *done _unused)
{
	xbps_dictionary_t pkgd;
	struct xbps_repo *repo = arg;
	const char *binpkg, *pkgver, *arch;
	int rv;

	binpkg = xbps_string_cstring_nocopy(obj);
	pkgd = xbps_get_pkg_plist_from_binpkg(binpkg, "./props.plist");
	if (pkgd == NULL) {
		rv = remove_pkg(repo->uri, binpkg);
		if (rv != 0) {
			xbps_object_release(pkgd);
			return 0;
		}
		printf("Removed broken package `%s'.\n", binpkg);
	}
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch);
	/* ignore pkgs from other archs */
	if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
		xbps_object_release(pkgd);
		return 0;
	}
	printf("checking %s (%s)\n", pkgver, binpkg);
	/*
	 * If binpkg is not registered in index, remove binpkg.
	 */
	if (!xbps_repo_get_pkg(repo, pkgver)) {
		rv = remove_pkg(repo->uri, binpkg);
		if (rv != 0) {
			xbps_object_release(pkgd);
			return 0;
		}
		printf("Removed obsolete package `%s'.\n", binpkg);
	}
	xbps_object_release(pkgd);

	return 0;
}

int
remove_obsoletes(struct xbps_handle *xhp, const char *repodir)
{
	xbps_array_t array = NULL;
	struct xbps_repo *repo;
	DIR *dirp;
	struct dirent *dp;
	char *ext;
	int rv = 0;

	repo = xbps_repo_open(xhp, repodir);
	if (repo == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "xbps-rindex: cannot read repository data: %s\n",
			    strerror(errno));
			return -1;
		}
		return 0;
	}
	if ((repo->idx = xbps_repo_get_plist(repo, XBPS_PKGINDEX)) == NULL) {
		xbps_repo_close(repo);
		return -1;
	}
	if (chdir(repodir) == -1) {
		fprintf(stderr, "xbps-rindex: cannot chdir to %s: %s\n",
		    repodir, strerror(errno));
		return errno;
	}
	if ((dirp = opendir(repodir)) == NULL) {
		fprintf(stderr, "xbps-rindex: failed to open %s: %s\n",
		    repodir, strerror(errno));
		return errno;
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

	rv = xbps_array_foreach_cb(xhp, array, NULL, cleaner_cb, repo);
	xbps_repo_close(repo);
	xbps_object_release(array);

	return rv;
}
