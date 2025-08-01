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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbps.h>

#include "defs.h"
#include "xbps/xbps_dictionary.h"
#include "xbps/xbps_object.h"

static int
remove_pkg(const char *repodir, const char *file)
{
	char filepath[PATH_MAX];
	int r;

	r = snprintf(filepath, sizeof(filepath), "%s/%s", repodir, file);
	if (r < 0 || (size_t)r >= sizeof(filepath)) {
		r = -ENAMETOOLONG;
		goto err;
	}
	if (remove(filepath) == -1 && errno != ENOENT) {
		r = -errno;
		goto err;
	}
	return 0;
err:
	xbps_error_printf("failed to remove package file: %s: %s\n",
	    filepath, strerror(-r));
	return r;
}

static int
remove_sig(const char *repodir, const char *file, const char *suffix)
{
	char sigpath[PATH_MAX];
	int r;

	r = snprintf(sigpath, sizeof(sigpath), "%s/%s.%s", repodir, file, suffix);
	if (r < 0 || (size_t)r >= sizeof(sigpath)) {
		r = -ENAMETOOLONG;
		goto err;
	}
	if (remove(sigpath) == -1 && errno != ENOENT) {
		r = -errno;
		goto err;
	}
	return 0;
err:
	xbps_error_printf("failed to remove signature file: %s: %s\n",
	    sigpath, strerror(-r));
	return r;
}

static bool
index_match_pkgver(xbps_dictionary_t index, const char *pkgname, const char *pkgver)
{
	xbps_dictionary_t pkgd;
	const char *dict_pkgver;

	pkgd = xbps_dictionary_get(index, pkgname);
	if (!pkgd)
		return false;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &dict_pkgver);
	return strcmp(dict_pkgver, pkgver) == 0;
}

static int
cleaner_cb(struct xbps_handle *xhp UNUSED, xbps_object_t obj, const char *key UNUSED, void *arg, bool *done UNUSED)
{
	char pkgname[XBPS_NAME_SIZE];
	struct xbps_repo *repo = arg;
	const char *binpkg;
	char *pkgver;

	binpkg = xbps_string_cstring_nocopy(obj);
	pkgver = xbps_binpkg_pkgver(binpkg);
	if (!pkgver || !xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		xbps_warn_printf("%s: invalid pkgver in xbps filename\n", binpkg);
		return 0;
	}

	xbps_verbose_printf("checking %s (%s)\n", pkgver, binpkg);

	if (index_match_pkgver(repo->stage, pkgname, pkgver) ||
	    index_match_pkgver(repo->index, pkgname, pkgver)) {
		free(pkgver);
		return 0;
	}
	free(pkgver);

	remove_pkg(repo->uri, binpkg);
	remove_sig(repo->uri, binpkg, "sig");
	remove_sig(repo->uri, binpkg, "sig2");

	printf("Removed obsolete package `%s'.\n", binpkg);
	return 0;
}

static bool
match_suffix(const char *str, size_t len, const char *suffix, size_t suffixlen)
{
	if (len < suffixlen)
		return false;
	return strcmp(str+len-suffixlen, suffix) == 0;
}

int
remove_obsoletes(struct xbps_handle *xhp, const char *repodir)
{
	xbps_array_t array = NULL;
	struct xbps_repo *repo;
	DIR *dirp;
	struct dirent *dp;
	int r;
	char suffix[NAME_MAX];
	int suffixlen;

	repo = xbps_repo_open(xhp, repodir);
	if (repo == NULL) {
		if (errno != ENOENT)
			return EXIT_FAILURE;
		return EXIT_SUCCESS;
	}

	if ((dirp = opendir(repodir)) == NULL) {
		xbps_error_printf("xbps-rindex: failed to open %s: %s\n",
		    repodir, strerror(errno));
		xbps_repo_release(repo);
		return EXIT_FAILURE;
	}

	array = xbps_array_create();
	if (!array) {
		xbps_error_printf("failed to allocate array: %s\n", strerror(-errno));
		xbps_repo_release(repo);
		closedir(dirp);
		return EXIT_FAILURE;
	}

	suffixlen = snprintf(suffix, sizeof(suffix), ".%s.xbps",
	    xhp->target_arch ? xhp->target_arch : xhp->native_arch);
	if (suffixlen < 0 || (size_t)suffixlen >= sizeof(suffix)) {
		xbps_error_printf("failed to create package suffix: %s", strerror(ENAMETOOLONG));
		goto err;
	}

	while ((dp = readdir(dirp))) {
		size_t len;

		if (dp->d_name[0] == '.')
			continue;

		len = strlen(dp->d_name);
		if (!match_suffix(dp->d_name, len, suffix, suffixlen) &&
		    !match_suffix(dp->d_name, len, ".noarch.xbps", sizeof(".noarch.xbps") - 1))
			continue;

		if (!xbps_array_add_cstring(array, dp->d_name)) {
			xbps_error_printf("failed to add string to array: %s\n",
			    strerror(-errno));
			goto err;
		}
	}
	closedir(dirp);

	r = xbps_array_foreach_cb_multi(xhp, array, NULL, cleaner_cb, repo);

	xbps_repo_release(repo);
	xbps_object_release(array);

	if (r < 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
err:
	closedir(dirp);
	xbps_repo_release(repo);
	xbps_object_release(array);
	return EXIT_FAILURE;
}
