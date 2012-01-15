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
	prop_array_t idxfiles;
	const char *pkgdir;
};

static int
genindex_files_cb(prop_object_t obj, void *arg, bool *done)
{
	prop_dictionary_t pkg_filesd, pkgd;
	prop_array_t array;
	struct index_files_data *ifd = arg;
	const char *binpkg, *pkgver;
	char *file;
	bool found = false;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "filename", &binpkg);
	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

	file = xbps_xasprintf("%s/%s", ifd->pkgdir, binpkg);
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
	pkgd = prop_dictionary_create();
	if (pkgd == NULL) {
		prop_object_release(pkg_filesd);
		return ENOMEM;
	}
	/* add conf_files array in pkgd */
	array = prop_dictionary_get(pkg_filesd, "conf_files");
	if (array != NULL && prop_array_count(array)) {
		found = true;
		if (!prop_dictionary_set(pkgd, "conf_files", array)) {
			prop_object_release(pkgd);
			prop_object_release(pkg_filesd);
			return EINVAL;
		}
	}
	/* add files array in pkgd */
	array = prop_dictionary_get(pkg_filesd, "files");
	if (array != NULL && prop_array_count(array)) {
		found = true;
		if (!prop_dictionary_set(pkgd, "files", array)) {
			prop_object_release(pkgd);
			prop_object_release(pkg_filesd);
			return EINVAL;
		}
	}
	/* add links array in pkgd */
	array = prop_dictionary_get(pkg_filesd, "links");
	if (array != NULL && prop_array_count(array)) {
		found = true;
		if (!prop_dictionary_set(pkgd, "links", array)) {
			prop_object_release(pkgd);
			prop_object_release(pkg_filesd);
			return EINVAL;
		}
	}
	prop_object_release(pkg_filesd);
	if (!found) {
		prop_object_release(pkgd);
		return 0;
	}
	/* pkgver obj in pkgd */
	if (!prop_dictionary_set_cstring(pkgd, "pkgver", pkgver)) {
		prop_object_release(pkgd);
		return EINVAL;
	}

	/* add pkgd into provided array */
	if (!prop_array_add(ifd->idxfiles, pkgd)) {
		prop_object_release(pkgd);
		return EINVAL;
	}
	prop_object_release(pkgd);

	return 0;
}

/*
 * Create the index files cache for all packages in repository.
 */
int
repo_genindex_files(const char *pkgdir)
{
	prop_dictionary_t idxdict, idxfilesd;
	struct index_files_data *ifd;
	char *plist, *files_plist;
	int rv;

	plist = xbps_pkg_index_plist(pkgdir);
	if (plist == NULL)
		return ENOMEM;

	/* internalize repository index plist */
	idxdict = prop_dictionary_internalize_from_zfile(plist);
	if (idxdict == NULL) {
		free(plist);
		return errno;
	}

	ifd = malloc(sizeof(*ifd));
	if (ifd == NULL) {
		prop_object_release(idxdict);
		free(plist);
		return ENOMEM;
	}
	ifd->pkgdir = pkgdir;
	ifd->idxfiles = prop_array_create();

	printf("Creating repository's index files cache...\n");

	/* iterate over index.plist packages array */
	rv = xbps_callback_array_iter_in_dict(idxdict,
	    "packages", genindex_files_cb, ifd);
	prop_object_release(idxdict);
	free(plist);
	if (rv != 0) {
		prop_object_release(ifd->idxfiles);
		free(ifd);
		return rv;
	}
	idxfilesd = prop_dictionary_create();
	/* add array into the index-files dictionary */
	if (!prop_dictionary_set(idxfilesd, "packages", ifd->idxfiles)) {
		prop_object_release(ifd->idxfiles);
		prop_object_release(idxfilesd);
		free(ifd);
		return EINVAL;
	}
	files_plist = xbps_pkg_index_files_plist(pkgdir);
	if (files_plist == NULL) {
		prop_object_release(ifd->idxfiles);
		prop_object_release(idxfilesd);
		free(ifd);
		return ENOMEM;
	}
	/* externalize index-files dictionary to the plist file */
	if (!prop_dictionary_externalize_to_zfile(idxfilesd, files_plist)) {
		free(files_plist);
		prop_object_release(ifd->idxfiles);
		prop_object_release(idxfilesd);
		free(ifd);
		return errno;
	}
	free(files_plist);
	prop_object_release(idxfilesd);
	prop_object_release(ifd->idxfiles);
	free(ifd);

	return 0;
}
