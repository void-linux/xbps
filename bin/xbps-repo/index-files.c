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
#include <stdbool.h>
#include <errno.h>
#include <libgen.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

int
repo_index_files_clean(struct xbps_handle *xhp, const char *repodir)
{
	prop_object_t obj;
	prop_array_t idx, idxfiles, obsoletes;
	char *plist, *plistf, *plistf_lock, *pkgver, *str;
	const char *p, *arch, *ipkgver, *iarch;
	size_t x, i;
	int rv = 0, fdlock;
	bool flush = false;

	plist = plistf = plistf_lock = pkgver = str = NULL;
	idx = idxfiles = obsoletes = NULL;

	/* Internalize index-files.plist if found */
	if ((plistf = xbps_pkg_index_files_plist(xhp, repodir)) == NULL)
		return EINVAL;
	if ((idxfiles = prop_array_internalize_from_zfile(plistf)) == NULL) {
		free(plistf);
		return 0;
	}
	/* Acquire exclusive file lock */
	if ((fdlock = acquire_repo_lock(plistf, &plistf_lock)) == -1) {
		free(plistf);
		prop_object_release(idxfiles);
		return -1;
	}

	/* Internalize index.plist */
	if ((plist = xbps_pkg_index_plist(xhp, repodir)) == NULL) {
		rv = EINVAL;
		goto out;
	}
	if ((idx = prop_array_internalize_from_zfile(plist)) == NULL) {
		release_repo_lock(&plistf_lock, fdlock);
		rv = EINVAL;
		goto out;
	}
	printf("Cleaning `%s' index-files, please wait...\n", repodir);
	/*
	 * Iterate over index-files array to find obsolete entries.
	 */
	obsoletes = prop_array_create();
	assert(obsoletes);

	for (x = 0; x < prop_array_count(idxfiles); x++) {
		obj = prop_array_get(idxfiles, x);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &ipkgver);
		prop_dictionary_get_cstring_nocopy(obj, "architecture", &iarch);
		if (xbps_find_pkg_in_array_by_pkgver(xhp, idx, ipkgver, iarch)) {
			/* pkg found, do nothing */
			continue;
		}
		if ((str = xbps_xasprintf("%s,%s", ipkgver, iarch)) == NULL) {
			rv = ENOMEM;
			goto out;
		}
		if (!prop_array_add_cstring(obsoletes, str)) {
			free(str);
			rv = EINVAL;
			goto out;
		}
		free(str);
	}
	/*
	 * Iterate over the obsoletes and array and remove entries
	 * from index-files array.
	 */
	for (i = 0; i < prop_array_count(obsoletes); i++) {
		prop_array_get_cstring_nocopy(obsoletes, i, &p);
		pkgver = strdup(p);
		for (x = 0; x < strlen(p); x++) {
			if ((pkgver[x] = p[x]) == ',') {
				pkgver[x] = '\0';
				break;
			}
		}
		arch = strchr(p, ',') + 1;
		if (!xbps_remove_pkg_from_array_by_pkgver(
		    xhp, idxfiles, pkgver, arch)) {
			free(pkgver);
			rv = EINVAL;
			goto out;
		}
		printf("index-files: removed obsolete entry `%s' "
		    "(%s)\n", pkgver, arch);
		free(pkgver);
		flush = true;
	}
	/* Externalize index-files array to plist when necessary */
	if (flush && !prop_array_externalize_to_zfile(idxfiles, plistf))
		rv = errno;

	printf("index-files: %u packages registered.\n",
	    prop_array_count(idxfiles));

out:
	release_repo_lock(&plistf_lock, fdlock);

	if (obsoletes)
		prop_object_release(obsoletes);
	if (idx)
		prop_object_release(idx);
	if (idxfiles)
		prop_object_release(idxfiles);
	if (plist)
		free(plist);
	if (plistf)
		free(plistf);

	return rv;
}

int
repo_index_files_add(struct xbps_handle *xhp, int argc, char **argv)
{
	prop_array_t idxfiles = NULL;
	prop_object_t obj, fileobj;
	prop_dictionary_t pkgprops, pkg_filesd, pkgd;
	prop_array_t files, pkg_cffiles, pkg_files, pkg_links;
	const char *binpkg, *pkgver, *arch;
	char *plist, *repodir, *p, *plist_lock;
	size_t x;
	int i, fdlock = -1, rv = 0;
	bool found, flush;

	found = flush = false;
	plist = plist_lock = repodir = p = NULL;
	obj = fileobj = NULL;
	pkgprops = pkg_filesd = pkgd = NULL;
	files = NULL;

        if ((p = strdup(argv[1])) == NULL) {
		rv = ENOMEM;
		goto out;
	}
	repodir = dirname(p);
	if ((plist = xbps_pkg_index_files_plist(xhp, repodir)) == NULL) {
		rv = ENOMEM;
		goto out;
	}
	/* Acquire exclusive file lock or wait for it.
	 */
	if ((fdlock = acquire_repo_lock(plist, &plist_lock)) == -1) {
		free(p);
		free(plist);
		return -1;
	}
	/*
	 * Internalize index-files.plist if found and process argv.
	 */
	if ((idxfiles = prop_array_internalize_from_zfile(plist)) == NULL) {
		if (errno == ENOENT) {
			idxfiles = prop_array_create();
			assert(idxfiles);
		} else {
			rv = errno;
			goto out;
		}
	}

	for (i = 1; i < argc; i++) {
		found = false;
		pkgprops = xbps_dictionary_metadata_plist_by_url(argv[i],
				"./props.plist");
		if (pkgprops == NULL) {
			fprintf(stderr, "index-files: cannot internalize "
			    "%s props.plist: %s\n", argv[i], strerror(errno));
			continue;
		}
		prop_dictionary_get_cstring_nocopy(pkgprops,
		    "filename", &binpkg);
		prop_dictionary_get_cstring_nocopy(pkgprops,
		    "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(pkgprops,
		    "architecture", &arch);

		if (xbps_find_pkg_in_array_by_pkgver(xhp, idxfiles,
		    pkgver, arch))  {
			fprintf(stderr, "index-files: skipping `%s' (%s), "
			    "already registered.\n", pkgver, arch);
			prop_object_release(pkgprops);
			pkgprops = NULL;
			continue;
		}

		/* internalize files.plist from binary package archive */
		pkg_filesd = xbps_dictionary_metadata_plist_by_url(argv[i],
				"./files.plist");
		if (pkg_filesd == NULL) {
			prop_object_release(pkgprops);
			rv = EINVAL;
			goto out;
		}

		/* Find out if binary pkg stored in index contain any file */
		pkg_cffiles = prop_dictionary_get(pkg_filesd, "conf_files");
		if (pkg_cffiles != NULL && prop_array_count(pkg_cffiles))
			found = true;
		else
			pkg_cffiles = NULL;

		pkg_files = prop_dictionary_get(pkg_filesd, "files");
		if (pkg_files != NULL && prop_array_count(pkg_files))
			found = true;
		else
			pkg_files = NULL;

		pkg_links = prop_dictionary_get(pkg_filesd, "links");
		if (pkg_links != NULL && prop_array_count(pkg_links))
			found = true;
		else
			pkg_links = NULL;

		/* If pkg does not contain any file, ignore it */
		if (!found) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			continue;
		}
		/* create pkg dictionary */
		if ((pkgd = prop_dictionary_create()) == NULL) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			rv = EINVAL;
			goto out;
		}
		/* add pkgver and architecture objects into pkg dictionary */
		if (!prop_dictionary_set_cstring(pkgd, "architecture", arch)) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		if (!prop_dictionary_set_cstring(pkgd, "pkgver", pkgver)) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		/* add files array obj into pkg dictionary */
		if ((files = prop_array_create()) == NULL) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		if (!prop_dictionary_set(pkgd, "files", files)) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			prop_object_release(files);
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		/* add conf_files in pkgd */
		if (pkg_cffiles != NULL) {
			for (x = 0; x < prop_array_count(pkg_cffiles); x++) {
				obj = prop_array_get(pkg_cffiles, x);
				fileobj = prop_dictionary_get(obj, "file");
				if (!prop_array_add(files, fileobj)) {
					prop_object_release(pkgprops);
					prop_object_release(pkg_filesd);
					prop_object_release(files);
					prop_object_release(pkgd);
					rv = EINVAL;
					goto out;
				}
			}
		}
		/* add files array in pkgd */
		if (pkg_files != NULL) {
			for (x = 0; x < prop_array_count(pkg_files); x++) {
				obj = prop_array_get(pkg_files, x);
				fileobj = prop_dictionary_get(obj, "file");
				if (!prop_array_add(files, fileobj)) {
					prop_object_release(pkgprops);
					prop_object_release(pkg_filesd);
					prop_object_release(files);
					prop_object_release(pkgd);
					rv = EINVAL;
					goto out;
				}
			}
		}
		/* add links array in pkgd */
		if (pkg_links != NULL) {
			for (x = 0; x < prop_array_count(pkg_links); x++) {
				obj = prop_array_get(pkg_links, x);
				fileobj = prop_dictionary_get(obj, "file");
				if (!prop_array_add(files, fileobj)) {
					prop_object_release(pkgprops);
					prop_object_release(pkg_filesd);
					prop_object_release(files);
					prop_object_release(pkgd);
					rv = EINVAL;
					goto out;
				}
			}
		}
		/* add pkgd into the index-files array */
		if (!prop_array_add(idxfiles, pkgd)) {
			prop_object_release(pkgprops);
			prop_object_release(pkg_filesd);
			prop_object_release(files);
			prop_object_release(pkgd);
			rv = EINVAL;
			goto out;
		}
		flush = true;
		printf("index-files: added `%s' (%s)\n", pkgver, arch);
		prop_object_release(pkgprops);
		prop_object_release(pkg_filesd);
		prop_object_release(files);
		prop_object_release(pkgd);
		pkgprops = pkg_filesd = pkgd = NULL;
		files = NULL;
	}

	if (flush && !prop_array_externalize_to_zfile(idxfiles, plist)) {
		fprintf(stderr, "failed to externalize %s: %s\n",
		    plist, strerror(errno));
		rv = errno;
	}
	printf("index-files: %u packages registered.\n",
	    prop_array_count(idxfiles));

out:
	release_repo_lock(&plist_lock, fdlock);

	if (p)
		free(p);
	if (plist)
		free(plist);
	if (idxfiles)
		prop_object_release(idxfiles);

	return rv;
}
