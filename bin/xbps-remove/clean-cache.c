/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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
#include <glob.h>

#include <xbps.h>
#include "defs.h"

static int KEEP_CACHE;

bool
check_keep(char *optarg)
{
	assert(optarg);
	while (*optarg) {
		if (!isdigit(*optarg))
			return false;
		else
			optarg++;
	}

	return true;
}

static int
binpkgcmp(const void *binpkg_a, const void *binpkg_b)
{
	const char **binpkg1, **binpkg2;

	assert(binpkg_a);
	assert(binpkg_b);

	binpkg1 = (const char **)binpkg_a;
	binpkg2 = (const char **)binpkg_b;

	return xbps_cmpver(*binpkg1, *binpkg2);
}

bool
is_removable_pkg(struct xbps_handle *xhp, const char *binpkg, char *pkgver, int keep)
{

	char pkgname[XBPS_NAME_SIZE];
	xbps_string_t pattern = NULL;
	glob_t fake_results;
	int lenpkg, rv, len_realres, len_fakeres;
	const char *entry = NULL;
	char **real_results = NULL;
	bool match = false;

	lenpkg = rv = len_fakeres = len_realres = 0;

	if (xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		lenpkg = strlen(pkgname);
		/* Building pattern */
		pattern = xbps_string_create_cstring(pkgname);
		assert(pattern);
		xbps_string_append_cstring(pattern, "-*.xbps");

		rv = glob(xbps_string_cstring_nocopy(pattern), 0, NULL, &fake_results);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "[removable] error in glob() rv = %d - %s\n", rv, strerror(rv));
		}
		assert(rv == 0);
		assert((len_fakeres = fake_results.gl_pathc) > 0);
		for (int i = 0; i < len_fakeres; i++) {
			entry = fake_results.gl_pathv[i];
			if (isdigit(entry[lenpkg + 1])) {
				len_realres++;
				real_results = realloc(real_results, len_realres * sizeof(char *));
				assert(real_results);
				real_results[len_realres - 1] = calloc(strlen(entry) + 1, sizeof(char));
				strcpy(real_results[len_realres - 1], entry);
				assert(real_results[len_realres - 1]);
			}
		}
		/* Release resources */
		globfree(&fake_results);
		xbps_object_release(pattern);
		pattern = NULL;

		/* Sorting the real results */
		assert(len_realres > 0);
		qsort(real_results, len_realres, sizeof(char *), binpkgcmp);

		/* Cycle on the real results */
		xbps_dbg_printf(xhp, "[removable] keep = %d, real results lenght for %s = %d\n", keep, binpkg, len_realres);
		for (int i = 0; i < len_realres; i++) {
			entry = real_results[i];
			xbps_dbg_printf(xhp, "[removable] Result of %d = %s\n", i, entry);
			if (strcmp(binpkg, entry) == 0) {
				if (i < (len_realres - (keep + 1))) {
					xbps_dbg_printf(xhp, "[removable] %s at the position %d must be removed!\n\n", entry, i);
					match = true;
				}
				break;
			}
		}

		/* Release resources */
		for (int i = 0; i < len_realres; i++) {
			free(real_results[i]);
			real_results[i] = NULL;
		}
		free(real_results);
		real_results = NULL;
	}

	return match;
}

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
	if (repo_pkgd) {
		xbps_dictionary_get_cstring_nocopy(repo_pkgd,
			"filename-sha256", &rsha256);
		if (xbps_file_sha256_check(binpkg, rsha256) == 0) {
			/* hash matched */
			free(pkgver);
			pkgver = NULL;
			return 0;
		}
	}

	/* Remove obsolete packages according the 'keep' parameter value */
	if (is_removable_pkg(xhp, binpkg, pkgver, KEEP_CACHE)) {
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
		binpkgsig = NULL;
	}
	free(pkgver);
	pkgver = NULL;

	return 0;
}

int
clean_cachedir(struct xbps_handle *xhp, bool drun, int keep)
{
	xbps_array_t array = NULL;
	DIR *dirp;
	struct dirent *dp;
	char *ext;
	int rv = 0;

	KEEP_CACHE = keep;

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
