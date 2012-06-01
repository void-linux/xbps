/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <sys/stat.h>

#include <xbps_api.h>
#include "defs.h"

#ifndef __arraycount
#define __arraycount(a) (sizeof(a) / sizeof(*a))
#endif

static const char *archs[] = { "noarch", "i686", "x86_64" };

/*
 * Removes stalled pkg entries in repository's index.plist file, if any
 * binary package cannot be read (unavailable, not enough perms, etc).
 */
static int
remove_missing_binpkg_entries(const char *repodir)
{
	prop_array_t array;
	prop_dictionary_t pkgd;
	const char *filen, *pkgver, *arch;
	char *binpkg, *plist;
	size_t i;
	int rv = 0;
	bool found = false;

	plist = xbps_pkg_index_plist(repodir);
	if (plist == NULL)
		return -1;

	array = prop_array_internalize_from_zfile(plist);
	if (array == NULL) {
		if (errno != ENOENT) {
			xbps_error_printf("xbps-repo: cannot read `%s': %s\n",
			    plist, strerror(errno));
			exit(EXIT_FAILURE);
		} else {
			free(plist);
			return 0;
		}
	}

again:
	for (i = 0; i < prop_array_count(array); i++) {
		pkgd = prop_array_get(array, i);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(pkgd, "filename", &filen);
		prop_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch);
		binpkg = xbps_xasprintf("%s/%s/%s", repodir, arch, filen);
		if (binpkg == NULL) {
			errno = ENOMEM;
			rv = -1;
			break;
		}
		if (access(binpkg, R_OK) == -1) {
			printf("index: removed obsolete entry `%s' (%s)\n",
			    pkgver, arch);
			prop_array_remove(array, i);
			free(binpkg);
			found = true;
			goto again;
		}
		free(binpkg);
	}
	if (found) {
		if (!prop_array_externalize_to_zfile(array, plist))
			rv = errno;
	}
	free(plist);

	return rv;
}

static prop_array_t
repoidx_get(const char *pkgdir)
{
	prop_array_t array;
	char *plist;
	int rv;
	/*
	 * Remove entries in repositories index for unexistent
	 * packages, i.e dangling entries.
	 */
	if ((rv = remove_missing_binpkg_entries(pkgdir)) != 0)
		return NULL;

	plist = xbps_pkg_index_plist(pkgdir);
	if (plist == NULL)
		return NULL;

	array = prop_array_internalize_from_zfile(plist);
	free(plist);
	if (array == NULL)
		array = prop_array_create();

	return array;
}

static int
add_binpkg_to_index(prop_array_t idx,
		    const char *filedir,
		    const char *file)
{
	prop_dictionary_t newpkgd, curpkgd;
	struct stat st;
	const char *pkgname, *version, *regver, *oldfilen, *oldpkgver;
	const char *arch, *oldarch;
	char *sha256, *filen, *tmpfilen, *oldfilepath, *buf;
	int rv = 0;

	tmpfilen = strdup(file);
	if (tmpfilen == NULL)
		return errno;

	filen = basename(tmpfilen);
	if (strcmp(tmpfilen, filen) == 0) {
		rv = EINVAL;
		goto out;
	}

	newpkgd = xbps_dictionary_metadata_plist_by_url(file, XBPS_PKGPROPS);
	if (newpkgd == NULL) {
		xbps_error_printf("failed to read %s metadata for `%s',"
		    " skipping!\n", XBPS_PKGPROPS, file);
		goto out;
	}
	prop_dictionary_get_cstring_nocopy(newpkgd, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(newpkgd, "version", &version);
	prop_dictionary_get_cstring_nocopy(newpkgd, "architecture", &arch);
	/*
	 * Check if this package exists already in the index, but first
	 * checking the version. If current package version is greater
	 * than current registered package, update the index; otherwise
	 * pass to the next one.
	 */
	curpkgd = xbps_find_pkg_in_array_by_name(idx, pkgname, arch);
	if (curpkgd == NULL) {
		if (errno && errno != ENOENT) {
			prop_object_release(newpkgd);
			rv = errno;
			goto out;
		}
	} else {
		prop_dictionary_get_cstring_nocopy(curpkgd, "version", &regver);
		if (xbps_cmpver(version, regver) <= 0) {
			fprintf(stderr, "index: skipping `%s-%s' (%s), `%s-%s' already "
			    "registered.\n", pkgname, version,
			    arch, pkgname, regver);
			prop_object_release(newpkgd);
			rv = EEXIST;
			goto out;
		}
		/*
		 * Current binpkg is newer than the one registered
		 * in package index, remove outdated binpkg file
		 * and its dictionary from the pkg index.
		 */
		prop_dictionary_get_cstring_nocopy(curpkgd,
		    "filename", &oldfilen);
		prop_dictionary_get_cstring_nocopy(curpkgd,
		    "pkgver", &oldpkgver);
		prop_dictionary_get_cstring_nocopy(curpkgd,
		    "architecture", &oldarch);
		buf = strdup(oldpkgver);
		if (buf == NULL) {
			prop_object_release(newpkgd);
			rv = ENOMEM;
			goto out;
		}
		oldfilepath = xbps_xasprintf("%s/%s", filedir, oldfilen);
		if (oldfilepath == NULL) {
			rv = errno;
			prop_object_release(newpkgd);
			free(buf);
			goto out;
		}
		if (remove(oldfilepath) == -1) {
			rv = errno;
			xbps_error_printf("failed to remove old "
			    "package file `%s': %s\n", oldfilepath,
			    strerror(errno));
			free(oldfilepath);
			prop_object_release(newpkgd);
			free(buf);
			goto out;
		}
		free(oldfilepath);
		if (!xbps_remove_pkg_from_array_by_pkgver(idx, buf, oldarch)) {
			xbps_error_printf("failed to remove `%s' "
			    "from plist index: %s\n", buf, strerror(errno));
			prop_object_release(newpkgd);
			free(buf);
			goto out;
		}
		printf("index: removed obsolete entry/binpkg `%s' "
		    "(%s).\n", buf, arch);
		free(buf);
	}

	/*
	 * We have the dictionary now, add the required
	 * objects for the index.
	 */
	if (!prop_dictionary_set_cstring(newpkgd, "filename", filen)) {
		prop_object_release(newpkgd);
		rv = errno;
		goto out;
	}
	sha256 = xbps_file_hash(file);
	if (sha256 == NULL) {
		prop_object_release(newpkgd);
		rv = errno;
		goto out;
	}
	if (!prop_dictionary_set_cstring(newpkgd, "filename-sha256", sha256)) {
		prop_object_release(newpkgd);
		free(sha256);
		rv = errno;
		goto out;
	}
	free(sha256);
	if (stat(file, &st) == -1) {
		prop_object_release(newpkgd);
		rv = errno;
		goto out;
	}
	if (!prop_dictionary_set_uint64(newpkgd, "filename-size",
	    (uint64_t)st.st_size)) {
		prop_object_release(newpkgd);
		rv = errno;
		goto out;
	}
	/*
	 * Add dictionary into the index and update package count.
	 */
	if (!xbps_add_obj_to_array(idx, newpkgd)) {
		rv = EINVAL;
		goto out;
	}
	printf("index: added `%s-%s' (%s).\n", pkgname, version, arch);

out:
	if (tmpfilen)
		free(tmpfilen);

	return rv;
}

int
repo_genindex(const char *pkgdir)
{
	prop_array_t idx = NULL;
	struct dirent *dp;
	DIR *dirp;
	size_t i;
	char *curdir;
	char *binfile, *plist;
	int rv = 0;
	bool registered_newpkgs = false, foundpkg = false;

	/*
	 * Create or read existing package index plist file.
	 */
	idx = repoidx_get(pkgdir);
	if (idx == NULL)
		return errno;

	plist = xbps_pkg_index_plist(pkgdir);
	if (plist == NULL) {
		prop_object_release(idx);
		return errno;
	}

	for (i = 0; i < __arraycount(archs); i++) {
		curdir = xbps_xasprintf("%s/%s", pkgdir, archs[i]);
		assert(curdir != NULL);

		dirp = opendir(curdir);
		if (dirp == NULL) {
			if (errno == ENOENT) {
				free(curdir);
				continue;
			}
			xbps_error_printf("xbps-repo: cannot open `%s': %s\n",
			    curdir, strerror(errno));
			exit(EXIT_FAILURE);
		}
		while ((dp = readdir(dirp)) != NULL) {
			if ((strcmp(dp->d_name, ".") == 0) ||
			    (strcmp(dp->d_name, "..") == 0))
				continue;
			/* Ignore unknown files */
			if (strstr(dp->d_name, ".xbps") == NULL)
				continue;

			foundpkg = true;
			binfile = xbps_xasprintf("%s/%s", curdir, dp->d_name);
			if (binfile == NULL) {
				(void)closedir(dirp);
				rv = errno;
				goto out;
			}
			rv = add_binpkg_to_index(idx, curdir, binfile);
			free(binfile);
			if (rv == EEXIST) {
				rv = 0;
				continue;
			} else if (rv != 0) {
				(void)closedir(dirp);
				free(curdir);
				goto out;
			}
			registered_newpkgs = true;
		}
		(void)closedir(dirp);
		free(curdir);
	}

	if (foundpkg == false) {
		/* No packages were found in directory */
		rv = ENOENT;
	} else {
		/*
		 * Show total count registered packages.
		 */
		printf("index: %zu packages registered.\n",
		    (size_t)prop_array_count(idx));
		/*
		 * Don't write plist file if no packages were registered.
		 */
		if (registered_newpkgs == false)
			goto out;
		/*
		 * If any package was registered in package index, write
		 * plist file to storage.
		 */
		if (!prop_array_externalize_to_zfile(idx, plist))
			rv = errno;
	}
out:
	free(plist);
	prop_object_release(idx);

	return rv;
}
