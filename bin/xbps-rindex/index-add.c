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

#include <xbps_api.h>
#include "defs.h"

/*
 * Adds a binary package into the index and removes old binary package
 * and entry when it's necessary.
 */
int
index_add(struct xbps_handle *xhp, int argc, char **argv)
{
	prop_array_t filespkgar, pkg_files, pkg_links, pkg_cffiles;
	prop_dictionary_t idx, idxfiles, newpkgd, newpkgfilesd, curpkgd;
	prop_dictionary_t filespkgd;
	prop_object_t obj, fileobj;
	struct stat st;
	const char *oldpkgver, *arch, *oldarch;
	char *pkgver, *pkgname, *sha256, *repodir, *buf;
	char *tmprepodir, *plist, *plistf;
	size_t x;
	int i, ret = 0;
	bool files_flush = false, found = false, flush = false;

	idx = idxfiles = newpkgd = newpkgfilesd = curpkgd = NULL;
	tmprepodir = plist = plistf = NULL;

	if ((tmprepodir = strdup(argv[0])) == NULL)
		return ENOMEM;

	repodir = dirname(tmprepodir);

	/* Internalize index or create it if doesn't exist */
	if ((plist = xbps_pkg_index_plist(xhp, repodir)) == NULL) {
		free(tmprepodir);
		return -1;
	}

	if ((idx = prop_dictionary_internalize_from_zfile(plist)) == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "index: cannot read `%s': %s\n",
			    plist, strerror(errno));
			return -1;
		} else {
			idx = prop_dictionary_create();
			assert(idx);
		}
	}
	/* Internalize index-files or create it if doesn't exist */
	if ((plistf = xbps_pkg_index_files_plist(xhp, repodir)) == NULL)
		return -1;

	free(tmprepodir);

	if ((idxfiles = prop_dictionary_internalize_from_zfile(plistf)) == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "index: cannot read `%s': %s\n",
			    plistf, strerror(errno));
			return -1;
		} else {
			idxfiles = prop_dictionary_create();
			assert(idx);
		}
	}

	/*
	 * Process all packages specified in argv.
	 */
	for (i = 0; i < argc; i++) {
		/*
		 * Read metadata props plist dictionary from binary package.
		 */
		newpkgd = xbps_get_pkg_plist_from_binpkg(argv[i],
		    "./props.plist");
		if (newpkgd == NULL) {
			fprintf(stderr, "failed to read %s metadata for `%s',"
			    " skipping!\n", XBPS_PKGPROPS, argv[i]);
			continue;
		}
		prop_dictionary_get_cstring_nocopy(newpkgd, "architecture",
		    &arch);
		prop_dictionary_get_cstring(newpkgd, "pkgver", &pkgver);
		if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
			fprintf(stderr, "index: ignoring %s, unmatched "
			    "arch (%s)\n", pkgver, arch);
			prop_object_release(newpkgd);
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
		curpkgd = prop_dictionary_get(idx, pkgname);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				free(pkgver);
				free(pkgname);
				return errno;
			}
		} else {
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "pkgver", &oldpkgver);
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "architecture", &oldarch);
			ret = xbps_cmpver(pkgver, oldpkgver);
			if (ret <= 0) {
				/* Same version or index version greater */
				fprintf(stderr, "index: skipping `%s' "
				    "(%s), already registered.\n",
				    pkgver, arch);
				prop_object_release(newpkgd);
				free(pkgver);
				free(pkgname);
				continue;
			}
			/*
			 * Current package version is greater than
			 * index version.
			 */
			buf = xbps_xasprintf("`%s' (%s)", oldpkgver, oldarch);
			prop_dictionary_remove(idx, pkgname);
			printf("index: removed obsolete entry %s.\n", buf);
			free(buf);
		}
		/*
		 * We have the dictionary now, add the required
		 * objects for the index.
		 */
		if ((sha256 = xbps_file_hash(argv[i])) == NULL) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		if (!prop_dictionary_set_cstring(newpkgd, "filename-sha256",
		    sha256)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}

		free(sha256);
		if (stat(argv[i], &st) == -1) {
			free(pkgver);
			free(pkgname);
			return errno;
		}

		if (!prop_dictionary_set_uint64(newpkgd, "filename-size",
		    (uint64_t)st.st_size)) {
			free(pkgver);
			free(pkgname);
			return errno;
		}
		/*
		 * Remove obsolete package objects.
		 */
		prop_dictionary_remove(newpkgd, "archive-compression-type");
		prop_dictionary_remove(newpkgd, "build-date");
		prop_dictionary_remove(newpkgd, "build_date");
		prop_dictionary_remove(newpkgd, "conf_files");
		prop_dictionary_remove(newpkgd, "filename");
		prop_dictionary_remove(newpkgd, "homepage");
		prop_dictionary_remove(newpkgd, "license");
		prop_dictionary_remove(newpkgd, "maintainer");
		prop_dictionary_remove(newpkgd, "packaged-with");
		prop_dictionary_remove(newpkgd, "source-revisions");
		prop_dictionary_remove(newpkgd, "long_desc");
		prop_dictionary_remove(newpkgd, "pkgname");
		prop_dictionary_remove(newpkgd, "version");
		/*
		 * Add new pkg dictionary into the index.
		 */
		if (!prop_dictionary_set(idx, pkgname, newpkgd)) {
			free(pkgname);
			return EINVAL;
		}

		flush = true;
		printf("index: added `%s' (%s).\n", pkgver, arch);
		/*
		 * Add new pkg dictionary into the index-files.
		 */
		found = false;
		newpkgfilesd = xbps_get_pkg_plist_from_binpkg(argv[i],
				"./files.plist");
		if (newpkgfilesd == NULL) {
			free(pkgver);
			free(pkgname);
			return EINVAL;
		}

		/* Find out if binary pkg stored in index contain any file */
		pkg_cffiles = prop_dictionary_get(newpkgfilesd, "conf_files");
		if (pkg_cffiles != NULL && prop_array_count(pkg_cffiles))
			found = true;
		else
			pkg_cffiles = NULL;

		pkg_files = prop_dictionary_get(newpkgfilesd, "files");
		if (pkg_files != NULL && prop_array_count(pkg_files))
			found = true;
		else
			pkg_files = NULL;

		pkg_links = prop_dictionary_get(newpkgfilesd, "links");
		if (pkg_links != NULL && prop_array_count(pkg_links))
			found = true;
		else
			pkg_links = NULL;

		/* If pkg does not contain any file, ignore it */
		if (!found) {
			prop_object_release(newpkgfilesd);
			prop_object_release(newpkgd);
			free(pkgver);
			free(pkgname);
			continue;
		}
		/* create pkg files array */
		filespkgar = prop_array_create();
		assert(filespkgar);

		/* add conf_files in pkg files array */
		if (pkg_cffiles != NULL) {
			for (x = 0; x < prop_array_count(pkg_cffiles); x++) {
				obj = prop_array_get(pkg_cffiles, x);
				fileobj = prop_dictionary_get(obj, "file");
				prop_array_add(filespkgar, fileobj);
			}
		}
		/* add files array in pkg array */
		if (pkg_files != NULL) {
			for (x = 0; x < prop_array_count(pkg_files); x++) {
				obj = prop_array_get(pkg_files, x);
				fileobj = prop_dictionary_get(obj, "file");
				prop_array_add(filespkgar, fileobj);
			}
		}
		/* add links array in pkgd */
		if (pkg_links != NULL) {
			for (x = 0; x < prop_array_count(pkg_links); x++) {
				obj = prop_array_get(pkg_links, x);
				fileobj = prop_dictionary_get(obj, "file");
				prop_array_add(filespkgar, fileobj);
			}
		}
		prop_object_release(newpkgfilesd);

		/* create pkg dictionary */
		filespkgd = prop_dictionary_create();
		assert(filespkgd);

		/* add pkg files array into pkg dictionary */
		prop_dictionary_set(filespkgd, "files", filespkgar);
		prop_object_release(filespkgar);

		/* set pkgver obj into pkg dictionary */
		prop_dictionary_set_cstring(filespkgd, "pkgver", pkgver);

		/* add pkg dictionary into index-files */
		prop_dictionary_set(idxfiles, pkgname, filespkgd);
		prop_object_release(filespkgd);

		printf("index-files: added `%s' (%s)\n", pkgver, arch);
		files_flush = true;
		prop_object_release(newpkgd);
		free(pkgver);
		free(pkgname);
	}

	if (flush && !prop_dictionary_externalize_to_zfile(idx, plist)) {
		fprintf(stderr, "index: failed to externalize plist: %s\n",
		    strerror(errno));
		return -1;
	}
	if (files_flush &&
	    !prop_dictionary_externalize_to_zfile(idxfiles, plistf)) {
		fprintf(stderr, "index-files: failed to externalize "
		    "plist: %s\n", strerror(errno));
		return -1;
	}
	printf("index: %u packages registered.\n",
	    prop_dictionary_count(idx));
	printf("index-files: %u packages registered.\n",
	    prop_dictionary_count(idxfiles));

	return 0;
}
