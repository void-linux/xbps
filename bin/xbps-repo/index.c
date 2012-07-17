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

/*
 * Removes stalled pkg entries in repository's index.plist file, if any
 * binary package cannot be read (unavailable, not enough perms, etc).
 */
int
repo_index_clean(struct xbps_handle *xhp, const char *repodir)
{
	prop_array_t array;
	prop_dictionary_t pkgd;
	const char *filen, *pkgver, *arch;
	char *binpkg, *plist, *plist_lock;
	size_t i, idx = 0;
	int fdlock, rv = 0;
	bool flush = false;

	if ((plist = xbps_pkg_index_plist(xhp, repodir)) == NULL)
		return -1;

	if ((fdlock = acquire_repo_lock(plist, &plist_lock)) == -1) {
		free(plist);
		return -1;
	}

	array = prop_array_internalize_from_zfile(plist);
	if (array == NULL) {
		if (errno != ENOENT) {
			xbps_error_printf("xbps-repo: cannot read `%s': %s\n",
			    plist, strerror(errno));
			free(plist);
			release_repo_lock(&plist_lock, fdlock);
			return -1;
		} else {
			release_repo_lock(&plist_lock, fdlock);
			free(plist);
			return 0;
		}
	}
	printf("Cleaning `%s' index, please wait...\n", repodir);

again:
	for (i = idx; i < prop_array_count(array); i++) {
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
			flush = true;
			idx = i;
			goto again;
		}
		free(binpkg);
	}
	if (flush && !prop_array_externalize_to_zfile(array, plist))
		rv = errno;

	free(plist);
	printf("index: %u packages registered.\n", prop_array_count(array));
	prop_object_release(array);
	release_repo_lock(&plist_lock, fdlock);

	return rv;
}

static int
remove_oldpkg(const char *repodir, const char *arch, const char *file)
{
	char *filepath;
	int rv;

	/* Remove real binpkg */
	filepath = xbps_xasprintf("%s/%s/%s", repodir, arch, file);
	assert(filepath);
	if (remove(filepath) == -1) {
		rv = errno;
		xbps_error_printf("failed to remove old binpkg `%s': %s\n",
		    file, strerror(rv));
		free(filepath);
		return rv;
	}
	free(filepath);

	/* Remove symlink to binpkg */
	filepath = xbps_xasprintf("%s/%s", repodir, file);
	assert(filepath);
	if (remove(filepath) == -1) {
		rv = errno;
		xbps_error_printf("failed to remove old binpkg `%s': %s\n",
		    file, strerror(rv));
		free(filepath);
		return rv;
	}
	free(filepath);

	return 0;
}

/*
 * Adds a binary package into the index and removes old binary package
 * and entry when it's necessary.
 */
int
repo_index_add(struct xbps_handle *xhp, int argc, char **argv)
{
	prop_array_t idx = NULL;
	prop_dictionary_t newpkgd = NULL, curpkgd;
	struct stat st;
	const char *pkgname, *version, *regver, *oldfilen, *oldpkgver;
	const char *arch, *oldarch;
	char *sha256, *filen, *repodir, *buf;
	char *tmpfilen = NULL, *tmprepodir = NULL, *plist = NULL;
	char *plist_lock = NULL;
	int i, ret = 0, rv = 0, fdlock = -1;
	bool flush = false;

	if ((tmprepodir = strdup(argv[1])) == NULL) {
		rv = ENOMEM;
		goto out;
	}
	repodir = dirname(tmprepodir);

	/* Internalize plist file or create it if doesn't exist */
	if ((plist = xbps_pkg_index_plist(xhp, repodir)) == NULL)
		return -1;

	/* Acquire exclusive file lock */
	if ((fdlock = acquire_repo_lock(plist, &plist_lock)) == -1) {
		rv = fdlock;
		goto out;
	}

	if ((idx = prop_array_internalize_from_zfile(plist)) == NULL) {
		if (errno != ENOENT) {
			xbps_error_printf("xbps-repo: cannot read `%s': %s\n",
			    plist, strerror(errno));
			rv = -1;
			goto out;
		} else {
			idx = prop_array_create();
			assert(idx);
		}
	}

	/*
	 * Process all packages specified in argv.
	 */
	for (i = 1; i < argc; i++) {
		if ((tmpfilen = strdup(argv[i])) == NULL) {
			rv = ENOMEM;
			goto out;
		}
		filen = basename(tmpfilen);
		/*
		 * Read metadata props plist dictionary from binary package.
		 */
		newpkgd = xbps_dictionary_metadata_plist_by_url(argv[i],
		    "./props.plist");
		if (newpkgd == NULL) {
			xbps_error_printf("failed to read %s metadata for `%s',"
			    " skipping!\n", XBPS_PKGPROPS, argv[i]);
			free(tmpfilen);
			filen = NULL;
			continue;
		}
		prop_dictionary_get_cstring_nocopy(newpkgd, "pkgname",
		    &pkgname);
		prop_dictionary_get_cstring_nocopy(newpkgd, "version",
		    &version);
		prop_dictionary_get_cstring_nocopy(newpkgd, "architecture",
		    &arch);
		/*
		 * Check if this package exists already in the index, but first
		 * checking the version. If current package version is greater
		 * than current registered package, update the index; otherwise
		 * pass to the next one.
		 */
		curpkgd =
		    xbps_find_pkg_in_array_by_name(xhp, idx, pkgname, arch);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				prop_object_release(newpkgd);
				free(tmpfilen);
				rv = errno;
				goto out;
			}
		} else {
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "version", &regver);
			ret = xbps_cmpver(version, regver);
			if (ret == 0) {
				/* Same version */
				fprintf(stderr, "index: skipping `%s-%s' "
				    "(%s), already registered.\n",
				    pkgname, version, arch);
				prop_object_release(newpkgd);
				free(tmpfilen);
				newpkgd = NULL;
				filen = NULL;
				continue;
			} else if (ret == -1) {
				/*
				 * Index version is greater, remove current
				 * package.
				 */
				rv = remove_oldpkg(repodir, arch, filen);
				if (rv != 0) {
					prop_object_release(newpkgd);
					free(tmpfilen);
					goto out;
				}
				buf = xbps_xasprintf("`%s-%s' (%s)",
				    pkgname, version, arch);
				assert(buf);
				printf("index: removed obsolete binpkg %s.\n",
				    buf);
				free(buf);
				prop_object_release(newpkgd);
				free(tmpfilen);
				newpkgd = NULL;
				filen = NULL;
				continue;
			}
			/*
			 * Current package version is greater than
			 * index version.
			 */
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "filename", &oldfilen);
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "pkgver", &oldpkgver);
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "architecture", &oldarch);

			if ((buf = strdup(oldpkgver)) == NULL) {
				prop_object_release(newpkgd);
				free(tmpfilen);
				rv = ENOMEM;
				goto out;
			}
			rv = remove_oldpkg(repodir, oldarch, oldfilen);
			if (rv != 0) {
				free(buf);
				prop_object_release(newpkgd);
				free(tmpfilen);
				goto out;
			}
			if (!xbps_remove_pkg_from_array_by_pkgver(xhp, idx,
			    buf, oldarch)) {
				xbps_error_printf("failed to remove `%s' "
				    "from plist index: %s\n", buf,
				    strerror(errno));
				rv = errno;
				free(buf);
				prop_object_release(newpkgd);
				free(tmpfilen);
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
			rv = errno;
			prop_object_release(newpkgd);
			free(tmpfilen);
			goto out;
		}
		if ((sha256 = xbps_file_hash(argv[i])) == NULL) {
			rv = errno;
			prop_object_release(newpkgd);
			free(tmpfilen);
			goto out;
		}
		if (!prop_dictionary_set_cstring(newpkgd, "filename-sha256",
		    sha256)) {
			free(sha256);
			prop_object_release(newpkgd);
			free(tmpfilen);
			rv = errno;
			goto out;
		}
		free(sha256);
		if (stat(argv[i], &st) == -1) {
			prop_object_release(newpkgd);
			free(tmpfilen);
			rv = errno;
			goto out;
		}
		if (!prop_dictionary_set_uint64(newpkgd, "filename-size",
		    (uint64_t)st.st_size)) {
			prop_object_release(newpkgd);
			free(tmpfilen);
			rv = errno;
			goto out;
		}
		/*
		 * Add new pkg dictionary into the index.
		 */
		if (!prop_array_add(idx, newpkgd)) {
			prop_object_release(newpkgd);
			free(tmpfilen);
			rv = EINVAL;
			goto out;
		}
		flush = true;
		printf("index: added `%s-%s' (%s).\n", pkgname, version, arch);
		free(tmpfilen);
		prop_object_release(newpkgd);
		newpkgd = NULL;
		sha256 = NULL;
		filen = NULL;
		oldfilen = oldarch = oldpkgver = NULL;
		pkgname = version = arch = NULL;
	}

	if (flush && !prop_array_externalize_to_zfile(idx, plist)) {
		xbps_error_printf("failed to externalize plist: %s\n",
		    strerror(errno));
		rv = errno;
	}
	printf("index: %u packages registered.\n", prop_array_count(idx));

out:
	release_repo_lock(&plist_lock, fdlock);

	if (tmprepodir)
		free(tmprepodir);
	if (plist)
		free(plist);
	if (idx)
		prop_object_release(idx);

	return rv;
}
