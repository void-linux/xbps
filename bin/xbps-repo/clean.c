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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <assert.h>

#include "defs.h"

int
cachedir_clean(struct xbps_handle *xhp)
{
	prop_dictionary_t pkg_propsd, repo_pkgd;
	DIR *dirp;
	struct dirent *dp;
	const char *pkgver, *rsha256;
	char *binpkg;
	int rv = 0;

	if ((dirp = opendir(xhp->cachedir)) == NULL)
		return 0;

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		/* only process xbps binary packages, ignore something else */
		if (!strstr(dp->d_name, ".xbps")) {
			printf("ignoring unknown file: %s\n", dp->d_name);
			continue;
		}
		/* Internalize props.plist dictionary from binary pkg */
		binpkg = xbps_xasprintf("%s/%s", xhp->cachedir, dp->d_name);
		assert(binpkg != NULL);
		pkg_propsd = xbps_dictionary_metadata_plist_by_url(binpkg,
		    "./props.plist");
		if (pkg_propsd == NULL) {
			xbps_error_printf("Failed to read from %s: %s\n",
			    dp->d_name, strerror(errno));
			free(binpkg);
			rv = errno;
			break;
		}
		prop_dictionary_get_cstring_nocopy(pkg_propsd, "pkgver", &pkgver);
		/*
		 * Remove binary pkg if it's not registered in any repository
		 * or if hash doesn't match.
		 */
		repo_pkgd = xbps_rpool_find_pkg_exact(xhp, pkgver);
		if (repo_pkgd) {
			prop_dictionary_get_cstring_nocopy(repo_pkgd,
			    "filename-sha256", &rsha256);
			if (xbps_file_hash_check(binpkg, rsha256) == ERANGE) {
				printf("Removed %s from cachedir (sha256 mismatch)\n",
				    dp->d_name);
				unlink(binpkg);
			}
			free(binpkg);
			continue;
		}
		printf("Removed %s from cachedir (obsolete)\n", dp->d_name);
		unlink(binpkg);
		free(binpkg);
	}
	closedir(dirp);
	return rv;
}
