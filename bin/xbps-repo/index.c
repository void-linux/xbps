/*-
 * Copyright (c) 2009 Juan Romero Pardines.
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
#include <sys/utsname.h>
#include <sys/stat.h>

#include <xbps_api.h>
#include "defs.h"

/* Array of valid architectures */
static const char *archdirs[] = { "i686", "x86_64", "noarch", NULL };

static prop_dictionary_t
repoidx_getdict(const char *pkgdir)
{
	prop_dictionary_t dict;
	prop_array_t array;
	char *plist;

	plist = xbps_get_pkg_index_plist(pkgdir);
	if (plist == NULL)
		return NULL;

	dict = prop_dictionary_internalize_from_file(plist);
	if (dict == NULL) {
		dict = prop_dictionary_create();
		if (dict == NULL)
			goto out;

		array = prop_array_create();
		if (array == NULL) {
			prop_object_release(dict);
			goto out;
		}

		if (!prop_dictionary_set(dict, "packages", array)) {
			prop_object_release(dict);
			prop_object_release(array);
			goto out;
		}
		prop_object_release(array);
		if (!prop_dictionary_set_cstring_nocopy(dict,
		    "pkgindex-version", XBPS_PKGINDEX_VERSION)) {
			prop_object_release(dict);
			goto out;
		}
	}
out:
	free(plist);

	return dict;
}

static int
xbps_repo_addpkg_index(prop_dictionary_t idxdict, const char *filedir,
		       const char *file)
{
	prop_dictionary_t newpkgd, curpkgd;
	prop_array_t pkgar;
	struct archive *ar = NULL;
	struct archive_entry *entry;
	struct stat st;
	const char *pkgname, *version, *regver, *oldfilen;
	char *sha256, *filen, *tmpfilen, *tmpstr, *oldfilepath;
	int i = 0, rv = 0;

	if (idxdict == NULL || file == NULL)
		return EINVAL;

	pkgname = version = regver = oldfilen = NULL;
	sha256 = filen = tmpfilen = tmpstr = oldfilepath = NULL;

	tmpfilen = strdup(file);
	if (tmpfilen == NULL)
		return errno;

	filen = basename(tmpfilen);
	if (strcmp(tmpfilen, filen) == 0) {
		rv = EINVAL;
		goto out;
	}

	ar = archive_read_new();
	if (ar == NULL) {
		rv = errno;
		goto out;
	}
	/* Enable support for tar format and all compression methods */
	archive_read_support_compression_all(ar);
	archive_read_support_format_tar(ar);

	if ((rv = archive_read_open_filename(ar, file,
	     ARCHIVE_READ_BLOCKSIZE)) == -1) {
		rv = errno;
		goto out;
	}

	/*
	 * Open the binary package and read the props.plist
	 * into a buffer.
	 */
	while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		if (i >= 5) {
			/* 
			 * Unlikely that archive contains XBPS_PKGPROPS,
			 * discard it completely.
			 */
			archive_read_data_skip(ar);
			printf("W: archive %s does not contain required "
			    "props.plist file!\n", file);
			break;
		}
		if (strstr(archive_entry_pathname(entry), XBPS_PKGPROPS) == 0) {
			archive_read_data_skip(ar);
			i++;
			continue;
		}
		newpkgd = xbps_read_dict_from_archive_entry(ar, entry);
		if (newpkgd == NULL) {
			printf("%s: can't read %s metadata file, skipping!\n",
			    file, XBPS_PKGPROPS);
			break;
		}
		if (!prop_dictionary_get_cstring_nocopy(newpkgd, "pkgname",
		    &pkgname)) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		if (!prop_dictionary_get_cstring_nocopy(newpkgd, "version",
		    &version)) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		/*
		 * Check if this package exists already in the index, but first
		 * checking the version. If current package version is greater
		 * than current registered package, update the index; otherwise
		 * pass to the next one.
		 */
		curpkgd = xbps_find_pkg_in_dict(idxdict, "packages", pkgname);
		if (curpkgd) {
			if (!prop_dictionary_get_cstring_nocopy(curpkgd,
			    "version", &regver)) {
				prop_object_release(newpkgd);
				rv = errno;
				break;
			}
			if (xbps_cmpver(version, regver) <= 0) {
				printf("W: skipping %s. %s-%s already "
				    "registered.\n", filen, pkgname, regver);
				prop_object_release(newpkgd);
				archive_read_data_skip(ar);
				rv = EEXIST;
				break;
			}
			/*
			 * Current binpkg is newer than the one registered
			 * in package index, remove outdated binpkg file
			 * and its dictionary from the pkg index.
			 */
			if (!prop_dictionary_get_cstring_nocopy(curpkgd,
			    "filename", &oldfilen)) {
				prop_object_release(newpkgd);
				rv = errno;
				break;
			}
			oldfilepath = xbps_xasprintf("%s/%s", filedir,
			    oldfilen);
			if (oldfilepath == NULL) {
				prop_object_release(newpkgd);
				rv = errno;
				break;
			}
			if (remove(oldfilepath) == -1) {
				printf("E: Couldn't remove old package file "
				    "'%s'!\n", oldfilen);
				free(oldfilepath);
				prop_object_release(newpkgd);
				rv = errno;
				break;
			}
			free(oldfilepath);
			tmpstr = strdup(oldfilen);
			if (tmpstr == NULL) {
				prop_object_release(newpkgd);
				rv = errno;
				break;
			}
			if ((rv = xbps_remove_pkg_from_dict(idxdict,
			    "packages", pkgname)) != 0) {
				prop_object_release(newpkgd);
				free(tmpstr);
				break;
			}
			printf("W: removed outdated binpkg file "
			    "for '%s'.\n", tmpstr);
			free(tmpstr);
		}

		/*
		 * We have the dictionary now, add the required
		 * objects for the index.
		 */
		if (!prop_dictionary_set_cstring(newpkgd, "filename", filen)) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		sha256 = xbps_get_file_hash(file);
		if (sha256 == NULL) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		if (!prop_dictionary_set_cstring(newpkgd,
		    "filename-sha256", sha256)) {
			prop_object_release(newpkgd);
			free(sha256);
			rv = errno;
			break;
		}
		free(sha256);
		if (stat(file, &st) == -1) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		if (!prop_dictionary_set_uint64(newpkgd, "filename-size",
		    (uint64_t)st.st_size)) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		/* Get package array in repo index file */
		pkgar = prop_dictionary_get(idxdict, "packages");
		if (pkgar == NULL) {
			prop_object_release(newpkgd);
			rv = errno;
			break;
		}
		/*
		 * Add dictionary into the index and update package count.
		 */
		if (!xbps_add_obj_to_array(pkgar, newpkgd)) {
			prop_object_release(newpkgd);
			rv = EINVAL;
			break;
		}
		printf("Registered %s-%s (%s) in package index.\n",
		    pkgname, version, filen);

		if (!prop_dictionary_set_uint64(idxdict, "total-pkgs",
		    prop_array_count(pkgar)))
			rv = errno;

		break;
	}

out:
	if (ar)
		archive_read_finish(ar);
	if (tmpfilen)
		free(tmpfilen);

	return rv;
}

int
xbps_repo_genindex(const char *pkgdir)
{
	prop_dictionary_t idxdict = NULL;
	struct dirent *dp;
	DIR *dirp;
	struct utsname un;
	uint64_t npkgcnt = 0;
	char *binfile, *path, *plist;
	size_t i;
	int rv = 0;
	bool registered_newpkgs = false, foundpkg = false;

	if (uname(&un) == -1)
		return errno;

	/*
	 * Create or read existing package index plist file.
	 */
	idxdict = repoidx_getdict(pkgdir);
	if (idxdict == NULL)
		return errno;

	plist = xbps_get_pkg_index_plist(pkgdir);
	if (plist == NULL) {
		prop_object_release(idxdict);
		return errno;
	}

	/*
	 * Iterate over the known architecture directories to find
	 * binary packages.
	 */
	for (i = 0; archdirs[i] != NULL; i++) {
		if ((strcmp(archdirs[i], un.machine)) &&
		    (strcmp(archdirs[i], "noarch")))
			continue;

		path = xbps_xasprintf("%s/%s", pkgdir, archdirs[i]);
		if (path == NULL) {
			rv = errno;
			goto out;
		}

		dirp = opendir(path);
		if (dirp == NULL) {
			printf("E: unexistent '%s' directory!\n", path);
			free(path);
			continue;
		}

		while ((dp = readdir(dirp)) != NULL) {
			if ((strcmp(dp->d_name, ".") == 0) ||
			    (strcmp(dp->d_name, "..") == 0))
				continue;

			/* Ignore unknown files */
			if (strstr(dp->d_name, ".xbps") == NULL)
				continue;

			foundpkg = true;
			binfile = xbps_xasprintf("%s/%s", path, dp->d_name);
			if (binfile == NULL) {
				(void)closedir(dirp);
				free(path);
				rv = errno;
				goto out;
			}
			rv = xbps_repo_addpkg_index(idxdict, path, binfile);
			free(binfile);
			if (rv == EEXIST)
				continue;
			else if (rv != 0) {
				(void)closedir(dirp);
				free(path);
				goto out;
			}
			registered_newpkgs = true;
		}
		(void)closedir(dirp);
		free(path);
	}

	if (foundpkg == false) {
		/* No packages were found in directory */
		rv = ENOENT;
	} else {
		/*
		 * Show total count registered packages.
		 */
		prop_dictionary_get_uint64(idxdict, "total-pkgs", &npkgcnt);
		printf("%ju packages registered in package index.\n", npkgcnt);
		/*
		 * Don't write plist file if no packages were registered.
		 */
		if (registered_newpkgs == false)
			goto out;
		/*
		 * If any package was registered in package index, write
		 * plist file to storage.
		 */
		if (!prop_dictionary_externalize_to_file(idxdict, plist))
			rv = errno;
	}
out:
	free(plist);
	prop_object_release(idxdict);

	return rv;
}
