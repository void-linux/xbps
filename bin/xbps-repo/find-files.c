/*-
 * Copyright (c) 2010-2012 Juan Romero Pardines.
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
#include <errno.h>

#include <xbps_api.h>
#include "defs.h"

struct ffdata {
	int npatterns;
	char **patterns;
	const char *repouri;
};

static void
match_files_by_pattern(struct xbps_handle *xhp,
		       prop_dictionary_t pkg_filesd,
		       struct ffdata *ffd)
{
	prop_array_t array;
	const char *filestr, *pkgver, *arch;
	size_t i;
	int x;

	prop_dictionary_get_cstring_nocopy(pkg_filesd, "architecture", &arch);
	if (!xbps_pkg_arch_match(xhp, arch, NULL))
		return;

	array = prop_dictionary_get(pkg_filesd, "files");
	for (i = 0; i < prop_array_count(array); i++) {
		prop_array_get_cstring_nocopy(array, i, &filestr);
		for (x = 1; x < ffd->npatterns; x++) {
			if ((xbps_pkgpattern_match(filestr, ffd->patterns[x])) ||
			    (strstr(filestr, ffd->patterns[x]))) {
				prop_dictionary_get_cstring_nocopy(pkg_filesd,
				    "pkgver", &pkgver);
				printf("%s: %s (%s)\n",
				    pkgver, filestr, ffd->repouri);
			}
		}
	}
}

static int
find_files_in_package(struct xbps_handle *xhp,
		      struct xbps_rpool_index *rpi,
		      void *arg,
		      bool *done)
{
	prop_array_t idxfiles;
	struct ffdata *ffd = arg;
	char *plist;
	unsigned int i;

	(void)done;

	if ((plist = xbps_pkg_index_files_plist(xhp, rpi->uri)) == NULL)
		return ENOMEM;

	if ((idxfiles = prop_array_internalize_from_zfile(plist)) == NULL) {
		free(plist);
		if (errno == ENOENT) {
			fprintf(stderr, "%s: index-files missing! "
			    "ignoring...\n", rpi->uri);
			return 0;
		}
		return errno;
	}
	free(plist);
	ffd->repouri = rpi->uri;

	for (i = 0; i < prop_array_count(idxfiles); i++)
		match_files_by_pattern(xhp, prop_array_get(idxfiles, i), ffd);

	prop_object_release(idxfiles);
	return 0;
}

int
repo_find_files_in_packages(struct xbps_handle *xhp,
			    int npatterns,
			    char **patterns)
{
	struct ffdata ffd;

	ffd.npatterns = npatterns;
	ffd.patterns = patterns;

	return xbps_rpool_foreach(xhp, find_files_in_package, &ffd);
}
