/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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

#include "defs.h"
#include "xbps/xbps_array.h"

#include <archive.h>
#include <archive_entry.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <memory.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xbps.h>

#define OPEN_AR_READ(ar)                       \
	{                                          \
		(ar) = archive_read_new();             \
		assert(ar);                            \
		archive_read_support_filter_gzip(ar);  \
		archive_read_support_filter_bzip2(ar); \
		archive_read_support_filter_xz(ar);    \
		archive_read_support_filter_lz4(ar);   \
		archive_read_support_filter_zstd(ar);  \
		archive_read_support_format_tar(ar);   \
	}


static void
list_packages(xbps_array_t dest, struct archive* ar) {
	struct archive_entry* entry;
	const char*           path;

	while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		path = archive_entry_pathname(entry);
		if (strcmp(path, "HASHES") == 0)
			continue;
		xbps_array_add_cstring(dest, path);
		archive_read_data_skip(ar);
	}
}

static const char* match_pkgname_in_array(xbps_array_t array, const char* str) {
	xbps_object_iterator_t iter;
	xbps_object_t          obj;
	const char*            pkgdep = NULL;
	char                   pkgname[XBPS_NAME_SIZE];

	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);
	assert(str != NULL);

	iter = xbps_array_iterator(array);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		/* match by pkgname against pkgver */
		pkgdep = xbps_string_cstring_nocopy(obj);
		if (!xbps_pkg_name(pkgname, XBPS_NAME_SIZE, pkgdep))
			break;
		if (strcmp(pkgname, str) == 0) {
			xbps_object_iterator_release(iter);
			return pkgdep;
		}
	}

	xbps_object_iterator_release(iter);
	return NULL;
}

static inline struct archive_entry*
make_entry(const char* pkgname, size_t size) {
	struct archive_entry* entry;

	entry = archive_entry_new();
	if (entry == NULL)
		return NULL;

	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0644);
	archive_entry_set_uname(entry, "root");
	archive_entry_set_gname(entry, "root");
	archive_entry_set_pathname(entry, pkgname);
	archive_entry_set_size(entry, size);

	return entry;
}

static inline int
make_file_string(xbps_dictionary_t pkg, char* buffer, int buffer_size) {
	const char *pkg_file, *pkg_target, *pkg_sha256;
	int         result;

	if (!xbps_dictionary_get_cstring_nocopy(pkg, "file", &pkg_file))
		return errno = EINVAL, 0;

	if (!xbps_dictionary_get_cstring_nocopy(pkg, "target", &pkg_target))
		pkg_target = "";

	if (!xbps_dictionary_get_cstring_nocopy(pkg, "sha256", &pkg_sha256))
		pkg_sha256 = "";

	if ((result = snprintf(buffer, buffer_size, "%s%%%s%%%s\n", pkg_sha256, pkg_file, pkg_target)) >= buffer_size)
		return errno = ENOBUFS, 0;

	if (result == -1)
		return 0;

	return result;
}

static inline int
length_file_string(xbps_dictionary_t pkg) {
	const char* field;
	size_t      size = 3;    // 2x ':' + '\n'

	size += xbps_dictionary_get_cstring_nocopy(pkg, "file", &field)
	          ? strlen(field)
	          : 0;

	size += xbps_dictionary_get_cstring_nocopy(pkg, "target", &field)
	          ? strlen(field)
	          : 0;

	size += xbps_dictionary_get_cstring_nocopy(pkg, "sha256", &field)
	          ? strlen(field)
	          : 0;

	return size;
}

int files_add(struct xbps_handle* xhp, int args, int argmax, char** argv, bool force, const char* compression, int* count_added, int* count_total) {
	static char           pkg_line[4096];
	xbps_dictionary_t     props_plist, files_plist;
	xbps_array_t          existing_files, ignore_packages;
	char *                tmprepodir = NULL, *repodir = NULL, *new_ar_path, *rlockfname = NULL, *files_uri = NULL;
	int                   rv = 0, ret = 0, rlockfd = -1;
	FILE*                 old_ar_file;
	int                   new_ar_file;
	struct archive *      old_ar = NULL, *new_ar;
	struct archive_entry* entry;
	mode_t                mask;

	*count_added = 0, *count_total = 0;

	existing_files = xbps_array_create();
	ignore_packages = xbps_array_create();

	assert(argv);
	/*
	 * Read the repository data or create index dictionaries otherwise.
	 */
	if ((tmprepodir = strdup(argv[args])) == NULL)
		return ENOMEM;

	repodir = dirname(tmprepodir);
	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		xbps_error_printf("xbps-rindex: cannot lock repository "
		                  "%s: %s\n",
		                  repodir, strerror(errno));
		rv = -1;
		goto out;
	}

	files_uri = xbps_repo_path_with_name(xhp, repodir, "files");
	if ((old_ar_file = fopen(files_uri, "r")) == NULL) {
		if (errno != ENOENT) {
			xbps_error_printf("[repo] `%s' failed to open archive %s\n", files_uri, strerror(errno));
			goto out;
		}
	} else {
		OPEN_AR_READ(old_ar);

		if (archive_read_open_FILE(old_ar, old_ar_file) == ARCHIVE_FATAL) {
			xbps_dbg_printf("[repo] `%s' failed to open repodata archive %s\n", files_uri, archive_error_string(old_ar));
			return false;
		}

		list_packages(existing_files, old_ar);
		*count_total = xbps_array_count(existing_files);
		archive_read_close(old_ar);
		fseek(old_ar_file, 0, SEEK_SET);
		OPEN_AR_READ(old_ar);
		if (archive_read_open_FILE(old_ar, old_ar_file) == ARCHIVE_FATAL) {
			xbps_dbg_printf("[repo] `%s' failed to open repodata archive %s\n", files_uri, archive_error_string(old_ar));
			return false;
		}
	}

	new_ar_path = xbps_xasprintf("%s.XXXXXXXXXX", files_uri);
	assert(new_ar_path);
	mask = umask(S_IXUSR | S_IRWXG | S_IRWXO);
	if ((new_ar_file = mkstemp(new_ar_path)) == -1)
		return false;

	umask(mask);

	new_ar = archive_write_new();
	assert(new_ar);

	if (compression == NULL || strcmp(compression, "zstd") == 0) {
		archive_write_add_filter_zstd(new_ar);
		archive_write_set_options(new_ar, "compression-level=9");
	} else if (strcmp(compression, "gzip") == 0) {
		archive_write_add_filter_gzip(new_ar);
		archive_write_set_options(new_ar, "compression-level=9");
	} else if (strcmp(compression, "bzip2") == 0) {
		archive_write_add_filter_bzip2(new_ar);
		archive_write_set_options(new_ar, "compression-level=9");
	} else if (strcmp(compression, "lz4") == 0) {
		archive_write_add_filter_lz4(new_ar);
		archive_write_set_options(new_ar, "compression-level=9");
	} else if (strcmp(compression, "xz") == 0) {
		archive_write_add_filter_xz(new_ar);
		archive_write_set_options(new_ar, "compression-level=9");
	} else if (strcmp(compression, "none") == 0) {
		/* empty */
	} else {
		return false;
	}

	archive_write_set_format_pax_restricted(new_ar);
	if (archive_write_open_fd(new_ar, new_ar_file) != ARCHIVE_OK)
		return false;

	/*
	 * Process all packages specified in argv.
	 */
	for (int i = args; i < argmax; i++) {
		const char *          arch = NULL, *pkg = argv[i], *dbpkgver = NULL, *pkgname = NULL, *pkgver = NULL;
		struct archive_entry* file_entry;
		xbps_array_t          keys;
		size_t                total_size = 0;

		assert(pkg);
		/*
		 * Read metadata props plist dictionary from binary package.
		 */
		if ((props_plist = xbps_archive_fetch_plist(pkg, "/props.plist")) == NULL) {
			xbps_error_printf("index: failed to read %s metadata for `%s', skipping!\n", XBPS_PKGPROPS, pkg);
			continue;
		}

		xbps_dictionary_get_cstring_nocopy(props_plist, "architecture", &arch);
		xbps_dictionary_get_cstring_nocopy(props_plist, "pkgver", &pkgver);
		if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
			fprintf(stderr, "index: skipping %s, unmatched arch (%s)\n", pkgver, arch);
			xbps_object_release(props_plist);
		}

		xbps_dictionary_get_cstring_nocopy(props_plist, "pkgname", &pkgname);
		if (!force && (dbpkgver = match_pkgname_in_array(existing_files, pkgname))) {
			/* Only check version if !force */
			ret = xbps_cmpver(pkgver, dbpkgver);

			/*
			 * If the considered package reverts the package in the index,
			 * consider the current package as the newer one.
			 */
			if (ret < 0 && xbps_pkg_reverts(props_plist, dbpkgver)) {
				ret = 1;
				/*
				 * If package in the index reverts considered package, consider the
				 * package in the index as the newer one.
				 */
			} else if (ret > 0 && xbps_pkg_reverts(props_plist, pkgver)) {
				ret = -1;
			}

			/* Same version or index version greater */
			if (ret <= 0) {
				xbps_object_release(props_plist);
				continue;
			}

			xbps_array_add_cstring(ignore_packages, dbpkgver);

			(*count_total)--;
			printf("files: updating `%s' -> `%s' (%s)\n", dbpkgver, pkgver, arch);
		}

		if ((files_plist = xbps_archive_fetch_plist(pkg, "/files.plist")) == NULL) {
			xbps_error_printf("files: failed to read files.plist metadata for `%s', skipping!\n", pkg);
			xbps_object_release(props_plist);
			continue;
		}

		keys = xbps_dictionary_all_keys(files_plist);
		for (unsigned int j = 0; j < xbps_array_count(keys); j++) {
			xbps_array_t files = xbps_dictionary_get_keysym(files_plist, xbps_array_get(keys, j));

			for (unsigned int k = 0; k < xbps_array_count(files); k++)
				total_size += length_file_string(xbps_array_get(files, k));
		}

		file_entry = make_entry(pkgver, total_size);
		if (archive_write_header(new_ar, file_entry) != ARCHIVE_OK) {
			xbps_warn_printf("files: unable to write entry for %s: %s\n", pkgver, archive_error_string(new_ar));
			archive_entry_free(file_entry);
			xbps_object_release(props_plist);
			xbps_object_release(files_plist);
			continue;
		}

		for (unsigned int j = 0; j < xbps_array_count(keys); j++) {
			int          size;
			xbps_array_t files = xbps_dictionary_get_keysym(files_plist, xbps_array_get(keys, j));

			for (unsigned int k = 0; k < xbps_array_count(files); k++) {
				if (!(size = make_file_string(xbps_array_get(files, k), pkg_line, sizeof(pkg_line)))) {
					xbps_warn_printf("files: unable to create file-entry for %s: %s\n", pkgver, strerror(errno));
					continue;
				}

				if (archive_write_data(new_ar, pkg_line, size) == -1) {
					xbps_warn_printf("files: unable to file-write entry for %s: %s\n", pkgver, archive_error_string(new_ar));
					continue;
				}
			}
		}


		if (archive_write_finish_entry(new_ar) != ARCHIVE_OK) {
			archive_entry_free(file_entry);
			exit(archive_errno(new_ar));
		}
		archive_entry_free(file_entry);

		(*count_added)++;
		(*count_total)++;
		printf("files: added `%s' (%s)\n", pkgver, arch);

		xbps_object_release(props_plist);
		xbps_object_release(files_plist);
	}

	if (*count_added > 0) {
		if (old_ar != NULL) {
			char        buffer[1024];
			size_t      buffer_size;
			const char* pkgname;

			while (archive_read_next_header(old_ar, &entry) == ARCHIVE_OK) {
				pkgname = archive_entry_pathname(entry);
				if (xbps_match_string_in_array(ignore_packages, pkgname)) {
					archive_read_data_skip(old_ar);
					continue;
				}

				printf("files: copying `%s' from old archive\n", pkgname);

				if (archive_write_header(new_ar, entry) != ARCHIVE_OK) {
					archive_entry_free(entry);
					continue;
				}

				while ((buffer_size = archive_read_data(old_ar, buffer, sizeof(buffer))) > 0) {
					assert(archive_write_data(new_ar, buffer, buffer_size) > 0);
				}

				archive_write_finish_entry(new_ar);
			}

			archive_read_free(old_ar);
		}
		/* Write data to tempfile and rename */
		if (archive_write_close(new_ar) != ARCHIVE_OK)
			return false;
		if (archive_write_free(new_ar) != ARCHIVE_OK)
			return false;

		if (fchmod(new_ar_file, 0664) == -1) {
			close(new_ar_file);
			unlink(new_ar_path);
			goto out;
		}
		close(new_ar_file);
		if (rename(new_ar_path, files_uri) == -1) {
			unlink(new_ar_path);
			goto out;
		}
	} else {
		if (old_ar != NULL)
			archive_read_free(old_ar);

		/* Write data to tempfile and rename */
		if (archive_write_close(new_ar) != ARCHIVE_OK)
			return false;
		if (archive_write_free(new_ar) != ARCHIVE_OK)
			return false;

		close(new_ar_file);
		unlink(new_ar_path);
	}

out:
	xbps_repo_unlock(rlockfd, rlockfname);

	if (tmprepodir)
		free(tmprepodir);

	return rv;
}
