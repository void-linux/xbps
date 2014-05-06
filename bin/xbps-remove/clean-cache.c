/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>

#include <xbps.h>
#include "defs.h"

static int
cleaner_cb(struct xbps_handle *xhp, xbps_object_t obj,
		const char *key _unused, void *arg _unused,
		bool *done _unused)
{
	xbps_dictionary_t pkg_propsd, repo_pkgd;
	const char *binpkg, *pkgver, *arch, *rsha256;
	char *binpkgsig;
	int rv;

	/* Internalize props.plist dictionary from binary pkg */
	binpkg = xbps_string_cstring_nocopy(obj);
	pkg_propsd = xbps_get_pkg_plist_from_binpkg(binpkg, "./props.plist");
	if (pkg_propsd == NULL) {
		rv = errno;
		xbps_error_printf("Failed to read from %s: %s\n", binpkg, strerror(errno));
		return rv;
	}
	xbps_dictionary_get_cstring_nocopy(pkg_propsd, "architecture", &arch);
	xbps_dictionary_get_cstring_nocopy(pkg_propsd, "pkgver", &pkgver);
	if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
		xbps_dbg_printf(xhp, "%s: ignoring pkg with unmatched arch (%s)\n", pkgver, arch);
		xbps_object_release(pkg_propsd);
		return 0;
	}
	/*
	 * Remove binary pkg if it's not registered in any repository
	 * or if hash doesn't match.
	 */
	binpkgsig = xbps_xasprintf("%s.sig", binpkg);
	repo_pkgd = xbps_rpool_get_pkg(xhp, pkgver);
	if (repo_pkgd) {
		xbps_dictionary_get_cstring_nocopy(repo_pkgd,
		    "filename-sha256", &rsha256);
		if (xbps_file_hash_check(binpkg, rsha256) == ERANGE) {
			if (unlink(binpkg) == -1) {
				fprintf(stderr, "Failed to remove "
				    "`%s': %s\n", binpkg, strerror(errno));
			} else {
				printf("Removed %s from cachedir (sha256 mismatch)\n", binpkg);
			}
			if ((access(binpkgsig, R_OK) == 0) && (unlink(binpkgsig) == -1)) {
				fprintf(stderr, "Failed to remove "
				    "`%s': %s\n", binpkgsig, strerror(errno));
			}
		}
		xbps_object_release(pkg_propsd);
		free(binpkgsig);
		return 0;
	}
	if (unlink(binpkg) == -1) {
		fprintf(stderr, "Failed to remove `%s': %s\n",
		    binpkg, strerror(errno));
	} else {
		printf("Removed %s from cachedir (obsolete)\n", binpkg);
	}
	if ((access(binpkgsig, R_OK) == 0) && (unlink(binpkgsig) == -1)) {
		fprintf(stderr, "Failed to remove `%s': %s\n",
		    binpkgsig, strerror(errno));
	}
	xbps_object_release(pkg_propsd);
	free(binpkgsig);

	return 0;
}

int
clean_cachedir(struct xbps_handle *xhp)
{
	xbps_array_t array = NULL;
	DIR *dirp;
	struct dirent *dp;
	char *ext;
	int rv = 0;

	if (chdir(xhp->cachedir) == -1)
		return -1;

	if ((dirp = opendir(xhp->cachedir)) == NULL)
		return 0;

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		/* only process xbps binary packages, ignore something else */
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".xbps")) {
			xbps_dbg_printf(xhp, "ignoring unknown file: %s\n", dp->d_name);
			continue;
		}
		if (array == NULL)
			array = xbps_array_create();

		xbps_array_add_cstring(array, dp->d_name);
	}
	(void)closedir(dirp);

	rv = xbps_array_foreach_cb_multi(xhp, array, NULL, cleaner_cb, NULL);
	xbps_object_release(array);
	return rv;
}
