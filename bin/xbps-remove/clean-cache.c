/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
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
		const char *key UNUSED, void *arg,
		bool *done UNUSED)
{
	xbps_dictionary_t repo_pkgd;
	const char *binpkg, *rsha256;
	char *binpkgsig, *pkgver, *arch;
	bool drun = false;

	/* Extract drun (dry-run) flag from arg*/
	if (arg != NULL)
		drun = *(bool*)arg;

	/* Internalize props.plist dictionary from binary pkg */
	binpkg = xbps_string_cstring_nocopy(obj);
	arch = xbps_binpkg_arch(binpkg);
	assert(arch);

	if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
		xbps_dbg_printf(xhp, "%s: ignoring binpkg with unmatched arch (%s)\n", binpkg, arch);
		free(arch);
		return 0;
	}
	free(arch);
	/*
	 * Remove binary pkg if it's not registered in any repository
	 * or if hash doesn't match.
	 */
	pkgver = xbps_binpkg_pkgver(binpkg);
	assert(pkgver);
	repo_pkgd = xbps_rpool_get_pkg(xhp, pkgver);
	free(pkgver);
	if (repo_pkgd) {
		xbps_dictionary_get_cstring_nocopy(repo_pkgd,
		    "filename-sha256", &rsha256);
		if (xbps_file_sha256_check(binpkg, rsha256) == 0) {
			/* hash matched */
			return 0;
		}
	}
	binpkgsig = xbps_xasprintf("%s.sig", binpkg);
	if (!drun && unlink(binpkg) == -1) {
		fprintf(stderr, "Failed to remove `%s': %s\n",
		    binpkg, strerror(errno));
	} else {
		printf("Removed %s from cachedir (obsolete)\n", binpkg);
	}
	if (!drun && unlink(binpkgsig) == -1) {
		if (errno != ENOENT) {
			fprintf(stderr, "Failed to remove `%s': %s\n",
			    binpkgsig, strerror(errno));
		}
	}
	free(binpkgsig);

	return 0;
}

int
clean_cachedir(struct xbps_handle *xhp, bool drun)
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

	array = xbps_array_create();
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
		xbps_array_add_cstring(array, dp->d_name);
	}
	(void)closedir(dirp);

	if (xbps_array_count(array)) {
		rv = xbps_array_foreach_cb_multi(xhp, array, NULL, cleaner_cb, (void*)&drun);
		xbps_object_release(array);
	}
	return rv;
}
