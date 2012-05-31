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

#include <xbps_api.h>
#include "defs.h"

struct index_files_data {
	prop_array_t idx;
	prop_array_t idxfiles;
	prop_array_t obsoletes;
	const char *pkgdir;
	const char *targetarch;
	bool flush;
	bool new;
};

static int
rmobsoletes_files_cb(prop_object_t obj, void *arg, bool *done)
{
	struct index_files_data *ifd = arg;
	const char *pkgver, *arch;
	char *str;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	if (xbps_find_pkg_in_array_by_pkgver(ifd->idx, pkgver, arch)) {
		/* pkg found, do nothing */
		return 0;
	}
	if ((str = xbps_xasprintf("%s,%s", pkgver, arch)) == NULL)
		return ENOMEM;

	if (!prop_array_add_cstring(ifd->obsoletes, str)) {
		free(str);
		return EINVAL;
	}
	free(str);
	ifd->flush = true;

	return 0;
}

static int
genindex_files_cb(prop_object_t obj, void *arg, bool *done)
{
	prop_object_t obj2, fileobj;
	prop_dictionary_t pkg_filesd, pkgd, regpkgd;
	prop_array_t array, files;
	struct index_files_data *ifd = arg;
	const char *binpkg, *pkgver, *rpkgver, *version, *arch;
	char *file, *pkgname, *pattern;
	bool found = false;
	size_t i;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "filename", &binpkg);
	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);

	if (ifd->new)
		goto start;

	pkgname = xbps_pkg_name(pkgver);
	if (pkgname == NULL)
		return ENOMEM;
	version = xbps_pkg_version(pkgver);
	if (version == NULL) {
		free(pkgname);
		return EINVAL;
	}
	pattern = xbps_xasprintf("%s>=0", pkgname);
	if (pattern == NULL) {
		free(pkgname);
		return ENOMEM;
	}
	free(pkgname);
	regpkgd = xbps_find_pkg_in_array_by_pattern(ifd->idxfiles, pattern, arch);
	if (regpkgd) {
		/*
		 * pkg already registered, check if same version
		 * is registered.
		 */
		prop_dictionary_get_cstring_nocopy(regpkgd, "pkgver", &rpkgver);
		if (strcmp(pkgver, rpkgver) == 0) {
			/* same pkg */
			xbps_warn_printf("skipping `%s', already registered.\n",
			    rpkgver);
			return 0;
		}
		/* pkgver does not match, remove it from index-files */
		if (!xbps_remove_pkg_from_array_by_pkgver(ifd->idxfiles,
							  rpkgver, arch))
			return EINVAL;
	}

start:
	file = xbps_xasprintf("%s/%s/%s", ifd->pkgdir, arch, binpkg);
	if (file == NULL)
		return ENOMEM;

	/* internalize files.plist from binary package archive */
	pkg_filesd = xbps_dictionary_metadata_plist_by_url(file, XBPS_PKGFILES);
	if (pkg_filesd == NULL) {
		free(file);
		return EINVAL;
	}
	free(file);

	/* create pkg dictionary */
	if ((pkgd = prop_dictionary_create()) == NULL) {
		prop_object_release(pkg_filesd);
		return ENOMEM;
	}
	/* add pkgver and architecture objects into pkg dictionary */
	if (!prop_dictionary_set_cstring(pkgd, "architecture", arch)) {
		prop_object_release(pkg_filesd);
		prop_object_release(pkgd);
		return EINVAL;
	}
	if (!prop_dictionary_set_cstring(pkgd, "pkgver", pkgver)) {
		prop_object_release(pkg_filesd);
		prop_object_release(pkgd);
		return EINVAL;
	}
	/* add files array obj into pkg dictionary */
	if ((files = prop_array_create()) == NULL) {
		prop_object_release(pkg_filesd);
		prop_object_release(pkgd);
		return EINVAL;
	}
	if (!prop_dictionary_set(pkgd, "files", files)) {
		prop_object_release(pkg_filesd);
		prop_object_release(pkgd);
		return EINVAL;
	}

	/* add conf_files in pkgd */
	array = prop_dictionary_get(pkg_filesd, "conf_files");
	if (array != NULL && prop_array_count(array)) {
		found = true;
		for (i = 0; i < prop_array_count(array); i++) {
			obj2 = prop_array_get(array, i);
			fileobj = prop_dictionary_get(obj2, "file");
			if (!prop_array_add(files, fileobj)) {
				prop_object_release(pkgd);
				prop_object_release(pkg_filesd);
				return EINVAL;
			}
		}
	}
	/* add files array in pkgd */
	array = prop_dictionary_get(pkg_filesd, "files");
	if (array != NULL && prop_array_count(array)) {
		found = true;
		for (i = 0; i < prop_array_count(array); i++) {
			obj2 = prop_array_get(array, i);
			fileobj = prop_dictionary_get(obj2, "file");
			if (!prop_array_add(files, fileobj)) {
				prop_object_release(pkgd);
				prop_object_release(pkg_filesd);
				return EINVAL;
			}
		}
	}
	/* add links array in pkgd */
	array = prop_dictionary_get(pkg_filesd, "links");
	if (array != NULL && prop_array_count(array)) {
		found = true;
		for (i = 0; i < prop_array_count(array); i++) {
			obj2 = prop_array_get(array, i);
			fileobj = prop_dictionary_get(obj2, "file");
			if (!prop_array_add(files, fileobj)) {
				prop_object_release(pkgd);
				prop_object_release(pkg_filesd);
				return EINVAL;
			}
		}
	}
	prop_object_release(pkg_filesd);
	if (!found) {
		prop_object_release(pkgd);
		return 0;
	}
	/* add pkgd into provided array */
	if (!prop_array_add(ifd->idxfiles, pkgd)) {
		prop_object_release(pkgd);
		return EINVAL;
	}
	prop_object_release(pkgd);
	ifd->flush = true;
	printf("Registered `%s' in repository files index.\n", pkgver);

	return 0;
}

/*
 * Create the index files cache for all packages in repository.
 */
int
repo_genindex_files(const char *pkgdir)
{
	prop_array_t idx;
	struct index_files_data *ifd = NULL;
	size_t i;
	char *plist, *tmppkgver, *pkgver, *arch, *saveptr;
	int rv;

	plist = xbps_pkg_index_plist(pkgdir);
	if (plist == NULL)
		return ENOMEM;

	/* internalize repository index plist */
	idx = prop_array_internalize_from_zfile(plist);
	if (idx == NULL) {
		free(plist);
		return errno;
	}
	free(plist);

	/* internalize repository index-files plist (if exists) */
	plist = xbps_pkg_index_files_plist(pkgdir);
	if (plist == NULL) {
		rv = ENOMEM;
		goto out;
	}
	ifd = calloc(1, sizeof(*ifd));
	if (ifd == NULL) {
		rv = ENOMEM;
		goto out;
	}
	ifd->pkgdir = pkgdir;
	ifd->idxfiles = prop_array_internalize_from_zfile(plist);
	ifd->idx = prop_array_copy(idx);
	ifd->obsoletes = prop_array_create();
	if (ifd->idxfiles == NULL) {
		/* missing file, create new one */
		ifd->idxfiles = prop_array_create();
		ifd->new = true;
	}

	/* iterate over index.plist array */
	rv = xbps_callback_array_iter(idx, genindex_files_cb, ifd);
	if (rv != 0)
		goto out;

	/* remove obsolete pkg entries */
	if (!ifd->new) {
		rv = xbps_callback_array_iter(ifd->idxfiles,
		    rmobsoletes_files_cb, ifd);
		if (rv != 0)
			goto out;
		for (i = 0; i < prop_array_count(ifd->obsoletes); i++) {
			prop_array_get_cstring(ifd->obsoletes, i, &tmppkgver);
			pkgver = strtok_r(tmppkgver, ",", &saveptr);
			arch = strtok_r(NULL, ",", &saveptr);
			free(tmppkgver);
			if (!xbps_remove_pkg_from_array_by_pkgver(
			    ifd->idxfiles, pkgver, arch)) {
				rv = EINVAL;
				goto out;
			}
			printf("Removed obsolete entry for `%s'.\n", pkgver);
		}
	}
	if (!ifd->flush)
		goto out;

	/* externalize index-files array */
	if (!prop_array_externalize_to_zfile(ifd->idxfiles, plist)) {
		rv = errno;
		goto out;
	}
out:
	if (rv == 0)
		printf("%u packages registered in repository files index.\n",
		    prop_array_count(ifd->idxfiles));
	if (ifd->obsoletes != NULL)
		prop_object_release(ifd->obsoletes);
	if (ifd->idxfiles != NULL)
		prop_object_release(ifd->idxfiles);
	if (ifd->idx != NULL)
		prop_object_release(ifd->idx);
	if (plist != NULL)
		free(plist);
	if (ifd != NULL)
		free(ifd);
	if (idx != NULL)
		prop_object_release(idx);

	return rv;
}
