/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"

int
remove_obsoletes(struct xbps_handle *xhp, const char *repodir)
{
	prop_dictionary_t pkgd;
	prop_array_t idx;
	DIR *dirp;
	struct dirent *dp;
	const char *pkgver, *arch;
	char *plist, *ext;
	int rv = 0;

	if ((plist = xbps_pkg_index_plist(xhp, repodir)) == NULL)
		return -1;

	idx = prop_array_internalize_from_zfile(plist);
	if (idx == NULL) {
		if (errno != ENOENT) {
			xbps_error_printf("xbps-repo: cannot read `%s': %s\n",
			    plist, strerror(errno));
			free(plist);
			return -1;
		} else {
			free(plist);
			return 0;
		}
	}
	if (chdir(repodir) == -1) {
		fprintf(stderr, "cannot chdir to %s: %s\n",
		    repodir, strerror(errno));
		prop_object_release(idx);
		return errno;
	}
	if ((dirp = opendir(repodir)) == NULL) {
		fprintf(stderr, "failed to open %s: %s\n",
		    repodir, strerror(errno));
		prop_object_release(idx);
		return errno;
	}
	while ((dp = readdir(dirp))) {
		if (strcmp(dp->d_name, "..") == 0)
			continue;
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".xbps"))
			continue;

		pkgd = xbps_dictionary_metadata_plist_by_url(dp->d_name,
		    "./props.plist");
		if (pkgd == NULL) {
			rv = remove_pkg(repodir, arch, dp->d_name);
			if (rv != 0) {
				fprintf(stderr, "index: failed to remove "
				    "package `%s': %s\n", dp->d_name,
				    strerror(rv));
				prop_object_release(pkgd);
				break;
			}
			printf("Removed broken package `%s'.\n", dp->d_name);
		}
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch);
		/*
		 * If binpkg is not registered in index, remove binpkg.
		 */
		if (!xbps_find_pkg_in_array_by_pkgver(xhp, idx, pkgver, arch)) {
			rv = remove_pkg(repodir, arch, dp->d_name);
			if (rv != 0) {
				fprintf(stderr, "index: failed to remove "
				    "package `%s': %s\n", dp->d_name,
				    strerror(rv));
				prop_object_release(pkgd);
				break;
			}
			printf("Removed obsolete package `%s'.\n", dp->d_name);
		}
		prop_object_release(pkgd);
	}
	(void)closedir(dirp);
	prop_object_release(idx);
	return rv;
}
