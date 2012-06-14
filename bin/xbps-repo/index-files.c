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
	bool flush;
	bool new;
};

static int
rmobsoletes_files_cb(struct xbps_handle *xhp,
		     prop_object_t obj,
		     void *arg,
		     bool *done)
{
	struct index_files_data *ifd = arg;
	const char *pkgver, *arch;
	char *str;

	(void)xhp;
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
genindex_files_cb(struct xbps_handle *xhp,
		  prop_object_t obj,
		  void *arg,
		  bool *done)
{
	prop_object_t obj2, fileobj;
	prop_dictionary_t pkg_filesd, pkgd;
	prop_array_t files, pkg_cffiles, pkg_files, pkg_links;
	struct index_files_data *ifd = arg;
	const char *binpkg, *pkgver, *arch;
	char *file;
	bool found = false;
	size_t i;

	(void)xhp;
	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "filename", &binpkg);
	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);

	if (xbps_find_pkg_in_array_by_pkgver(ifd->idxfiles, pkgver, arch))  {
		fprintf(stderr, "index-files: skipping `%s' (%s), "
		    "already registered.\n", pkgver, arch);
		return 0;
	}

	file = xbps_xasprintf("%s/%s/%s", ifd->pkgdir, arch, binpkg);
	if (file == NULL)
		return ENOMEM;

	/* internalize files.plist from binary package archive */
	pkg_filesd = xbps_dictionary_metadata_plist_by_url(file, "./files.plist");
	if (pkg_filesd == NULL) {
		free(file);
		return EINVAL;
	}
	free(file);

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
		prop_object_release(pkg_filesd);
		return 0;
	}

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
	if (pkg_cffiles != NULL) {
		for (i = 0; i < prop_array_count(pkg_cffiles); i++) {
			obj2 = prop_array_get(pkg_cffiles, i);
			fileobj = prop_dictionary_get(obj2, "file");
			if (!prop_array_add(files, fileobj)) {
				prop_object_release(pkgd);
				prop_object_release(pkg_filesd);
				return EINVAL;
			}
		}
	}
	/* add files array in pkgd */
	if (pkg_files != NULL) {
		for (i = 0; i < prop_array_count(pkg_files); i++) {
			obj2 = prop_array_get(pkg_files, i);
			fileobj = prop_dictionary_get(obj2, "file");
			if (!prop_array_add(files, fileobj)) {
				prop_object_release(pkgd);
				prop_object_release(pkg_filesd);
				return EINVAL;
			}
		}
	}
	/* add links array in pkgd */
	if (pkg_links != NULL) {
		for (i = 0; i < prop_array_count(pkg_links); i++) {
			obj2 = prop_array_get(pkg_links, i);
			fileobj = prop_dictionary_get(obj2, "file");
			if (!prop_array_add(files, fileobj)) {
				prop_object_release(pkgd);
				prop_object_release(pkg_filesd);
				return EINVAL;
			}
		}
	}
	prop_object_release(pkg_filesd);
	/* add pkgd into provided array */
	if (!prop_array_add(ifd->idxfiles, pkgd)) {
		prop_object_release(pkgd);
		return EINVAL;
	}
	printf("index-files: added `%s' (%s)\n", pkgver, arch);
	prop_object_release(pkgd);
	ifd->flush = true;

	return 0;
}

/*
 * Create the index files cache for all packages in repository.
 */
int
repo_genindex_files(struct xbps_handle *xhp, const char *pkgdir)
{
	prop_array_t idx;
	struct index_files_data *ifd = NULL;
	size_t i, x;
	const char *p, *arch;
	char *plist, *pkgver;
	int rv;

	plist = xbps_pkg_index_plist(xhp, pkgdir);
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
	plist = xbps_pkg_index_files_plist(xhp, pkgdir);
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
	ifd->idx = idx;
	ifd->obsoletes = prop_array_create();
	if (ifd->idxfiles == NULL) {
		/* missing file, create new one */
		ifd->idxfiles = prop_array_create();
		ifd->new = true;
	}

	/* remove obsolete pkg entries */
	if (!ifd->new) {
		rv = xbps_callback_array_iter(xhp, ifd->idxfiles,
		    rmobsoletes_files_cb, ifd);
		if (rv != 0)
			goto out;
		for (i = 0; i < prop_array_count(ifd->obsoletes); i++) {
			prop_array_get_cstring_nocopy(ifd->obsoletes, i, &p);
			pkgver = strdup(p);
			for (x = 0; x < strlen(p); x++) {
				if ((pkgver[x] = p[x]) == ',') {
					pkgver[x] = '\0';
					break;
				}
			}
			arch = strchr(p, ',') + 1;
			if (!xbps_remove_pkg_from_array_by_pkgver(
			    ifd->idxfiles, pkgver, arch)) {
				free(pkgver);
				rv = EINVAL;
				goto out;
			}
			printf("index-files: removed obsolete entry `%s' "
			    "(%s)\n", pkgver, arch);
			free(pkgver);
		}
	}
	/* iterate over index.plist array */
	if ((rv = xbps_callback_array_iter(xhp, idx, genindex_files_cb, ifd)) != 0)
		goto out;

	if (!ifd->flush)
		goto out;

	/* externalize index-files array */
	if (!prop_array_externalize_to_zfile(ifd->idxfiles, plist)) {
		rv = errno;
		goto out;
	}
out:
	if (rv == 0)
		printf("index-files: %u packages registered.\n",
		    prop_array_count(ifd->idxfiles));
	if (ifd->idxfiles != NULL)
		prop_object_release(ifd->idxfiles);
	if (plist != NULL)
		free(plist);
	if (ifd != NULL)
		free(ifd);
	if (idx != NULL)
		prop_object_release(idx);

	return rv;
}
