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
#include <fcntl.h>

#include <xbps.h>
#include "defs.h"

int
index_add(struct xbps_handle *xhp, int argc, char **argv, bool force)
{
	xbps_array_t array, pkg_files, pkg_links, pkg_cffiles;
	xbps_dictionary_t idx, idxfiles, idxpkgd, binpkgd, pkg_filesd, curpkgd;
	xbps_object_t obj, fileobj;
	struct xbps_repo *repo;
	struct stat st;
	uint64_t instsize;
	const char *arch, *desc;
	char *sha256, *pkgver, *opkgver, *oarch, *pkgname, *tmprepodir, *repodir;
	int rv = 0, ret = 0;
	bool flush = false, found = false;

	if ((tmprepodir = strdup(argv[0])) == NULL)
		return ENOMEM;

	/*
	 * Read the repository data or create index dictionaries otherwise.
	 */
	repodir = dirname(tmprepodir);

	repo = xbps_repo_open(xhp, repodir);
	if (repo && repo->idx) {
		xbps_repo_open_idxfiles(repo);
		idx = xbps_dictionary_copy(repo->idx);
		idxfiles = xbps_dictionary_copy(repo->idxfiles);
		xbps_repo_close(repo);
	} else {
		idx = xbps_dictionary_create();
		idxfiles = xbps_dictionary_create();
	}

	/*
	 * Process all packages specified in argv.
	 */
	for (int i = 0; i < argc; i++) {
		/*
		 * Read metadata props plist dictionary from binary package.
		 */
		binpkgd = xbps_get_pkg_plist_from_binpkg(argv[i],
		    "./props.plist");
		if (binpkgd == NULL) {
			fprintf(stderr, "failed to read %s metadata for `%s', skipping!\n", XBPS_PKGPROPS, argv[i]);
			continue;
		}
		xbps_dictionary_get_cstring_nocopy(binpkgd, "architecture", &arch);
		xbps_dictionary_get_cstring(binpkgd, "pkgver", &pkgver);
		if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
			fprintf(stderr, "index: ignoring %s, unmatched arch (%s)\n", pkgver, arch);
			xbps_object_release(binpkgd);
			continue;
		}
		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);
		/*
		 * Check if this package exists already in the index, but first
		 * checking the version. If current package version is greater
		 * than current registered package, update the index; otherwise
		 * pass to the next one.
		 */
		curpkgd = xbps_dictionary_get(idx, pkgname);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				free(pkgver);
				free(pkgname);
				return errno;
			}
		} else if (!force) {
			/* Only check version if !force */
			xbps_dictionary_get_cstring(curpkgd, "pkgver", &opkgver);
			xbps_dictionary_get_cstring(curpkgd, "architecture", &oarch);
			ret = xbps_cmpver(pkgver, opkgver);
			if (ret <= 0) {
				/* Same version or index version greater */
				fprintf(stderr, "index: skipping `%s' (%s), already registered.\n", pkgver, arch);
				xbps_object_release(binpkgd);
				free(opkgver);
				free(oarch);
				free(pkgver);
				free(pkgname);
				continue;
			}
			/*
			 * Current package version is greater than
			 * index version.
			 */
			xbps_dictionary_remove(idx, pkgname);
			xbps_dictionary_remove(idxfiles, opkgver);
			printf("index: removed obsolete entry `%s' (%s).\n", opkgver, oarch);
			free(opkgver);
			free(oarch);
		}
		idxpkgd = xbps_dictionary_create();
		assert(idxpkgd);
		/*
		 * Only copy relevant objects from binpkg:
		 * 	- architecture
		 * 	- pkgver
		 * 	- short_desc
		 * 	- installed_size
		 * 	- run_depends
		 * 	- provides
		 * 	- replaces
		 * 	- shlib-requires
		 */
		if (!xbps_dictionary_set_cstring(idxpkgd, "architecture", arch)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		if (!xbps_dictionary_set_cstring(idxpkgd, "pkgver", pkgver)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		xbps_dictionary_get_cstring_nocopy(binpkgd, "short_desc", &desc);
		if (!xbps_dictionary_set_cstring(idxpkgd, "short_desc", desc)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		xbps_dictionary_get_uint64(binpkgd, "installed_size", &instsize);
		if (!xbps_dictionary_set_uint64(idxpkgd, "installed_size", instsize)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		array = xbps_dictionary_get(binpkgd, "run_depends");
		if (xbps_array_count(array) && !xbps_dictionary_set(idxpkgd, "run_depends", array)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		array = xbps_dictionary_get(binpkgd, "provides");
		if (xbps_array_count(array) && !xbps_dictionary_set(idxpkgd, "provides", array)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		array = xbps_dictionary_get(binpkgd, "replaces");
		if (xbps_array_count(array) && !xbps_dictionary_set(idxpkgd, "replaces", array)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		array = xbps_dictionary_get(binpkgd, "shlib-requires");
		if (xbps_array_count(array) && !xbps_dictionary_set(idxpkgd, "shlib-requires", array)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		/*
		 * Add additional objects for repository ops:
		 * 	- filename-size
		 * 	- filename-sha256
		 */
		if ((sha256 = xbps_file_hash(argv[i])) == NULL) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		if (!xbps_dictionary_set_cstring(idxpkgd, "filename-sha256", sha256)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		if (stat(argv[i], &st) == -1) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		if (!xbps_dictionary_set_uint64(idxpkgd, "filename-size", (uint64_t)st.st_size)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		/*
		 * Add new pkg dictionary into the index.
		 */
		if (!xbps_dictionary_set(idx, pkgname, idxpkgd)) {
			free(pkgname);
			return EINVAL;
		}
		flush = true;
		printf("index: added `%s' (%s).\n", pkgver, arch);
		xbps_object_release(idxpkgd);
		xbps_object_release(binpkgd);
		free(pkgname);
		/*
		 * Add new pkg dictionary into the index-files.
		 */
		found = false;
		pkg_filesd = xbps_get_pkg_plist_from_binpkg(argv[i], "./files.plist");
		if (pkg_filesd == NULL) {
			free(pkgver);
			return EINVAL;
		}

		pkg_cffiles = xbps_dictionary_get(pkg_filesd, "conf_files");
		if (xbps_array_count(pkg_cffiles))
			found = true;
		else
			pkg_cffiles = NULL;

		pkg_files = xbps_dictionary_get(pkg_filesd, "files");
		if (xbps_array_count(pkg_files))
			found = true;
		else
			pkg_files = NULL;

		pkg_links = xbps_dictionary_get(pkg_filesd, "links");
		if (xbps_array_count(pkg_links))
			found = true;
		else
			pkg_links = NULL;

		/* If pkg does not contain any file, ignore it */
		if (!found) {
			xbps_object_release(pkg_filesd);
			free(pkgver);
			continue;
		}
		/* create pkg files array */
		array = xbps_array_create();
		assert(array);

		/* add conf_files in pkg files array */
		if (pkg_cffiles != NULL) {
			for (unsigned int x = 0; x < xbps_array_count(pkg_cffiles); x++) {
				obj = xbps_array_get(pkg_cffiles, x);
				fileobj = xbps_dictionary_get(obj, "file");
				xbps_array_add(array, fileobj);
			}
		}
		/* add files array in pkg files array */
		if (pkg_files != NULL) {
			for (unsigned int x = 0; x < xbps_array_count(pkg_files); x++) {
				obj = xbps_array_get(pkg_files, x);
				fileobj = xbps_dictionary_get(obj, "file");
				xbps_array_add(array, fileobj);
			}
		}
		/* add links array in pkg files array */
		if (pkg_links != NULL) {
			for (unsigned int x = 0; x < xbps_array_count(pkg_links); x++) {
				obj = xbps_array_get(pkg_links, x);
				fileobj = xbps_dictionary_get(obj, "file");
				xbps_array_add(array, fileobj);
			}
		}
		/* add pkg files array into index-files */
		xbps_dictionary_set(idxfiles, pkgver, array);
		xbps_object_release(array);
		xbps_object_release(pkg_filesd);
		free(pkgver);
	}
	/*
	 * Generate repository data files.
	 */
	if (flush) {
		if (!repodata_flush(xhp, repodir, idx, idxfiles, NULL)) {
			fprintf(stderr, "failed to write repodata: %s\n", strerror(errno));
			return -1;
		}
	}
	printf("index: %u packages registered.\n", xbps_dictionary_count(idx));
	printf("index-files: %u packages registered.\n", xbps_dictionary_count(idxfiles));

	return rv;
}
