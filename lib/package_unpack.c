/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

#include "xbps_api_impl.h"

#define EXTRACT_FLAGS	ARCHIVE_EXTRACT_SECURE_NODOTDOT | \
			ARCHIVE_EXTRACT_SECURE_SYMLINKS | \
			ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS | \
			ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | \
			ARCHIVE_EXTRACT_UNLINK
#define FEXTRACT_FLAGS	ARCHIVE_EXTRACT_OWNER | EXTRACT_FLAGS


static int
set_extract_flags(uid_t euid)
{
	int flags;

	if (euid == 0)
		flags = FEXTRACT_FLAGS;
	else
		flags = EXTRACT_FLAGS;

	return flags;
}

static bool
match_preserved_file(struct xbps_handle *xhp, const char *entry)
{
	const char *file;

	if (xhp->preserved_files == NULL)
		return false;

	if (entry[0] == '.' && entry[1] != '\0') {
		file = strchr(entry, '.') + 1;
		assert(file);
	} else {
		file = entry;
	}

	return xbps_match_string_in_array(xhp->preserved_files, file);
}

static int
unpack_archive(struct xbps_handle *xhp,
	       xbps_dictionary_t pkg_repod,
	       const char *pkgver,
	       const char *fname,
	       struct archive *ar)
{
	xbps_dictionary_t binpkg_filesd, pkg_filesd, obsd;
	xbps_array_t array, obsoletes;
	xbps_trans_type_t ttype;
	const struct stat *entry_statp;
	struct stat st;
	struct xbps_unpack_cb_data xucd;
	struct archive_entry *entry;
	ssize_t entry_size;
	const char *entry_pname, *pkgname;
	char *buf = NULL;
	int ar_rv, rv, error, entry_type, flags;
	bool preserve, update, file_exists, keep_conf_file;
	bool skip_extract, force, xucd_stats;
	uid_t euid;

	binpkg_filesd = pkg_filesd = NULL;
	force = preserve = update = file_exists = false;
	xucd_stats = false;
	ar_rv = rv = error = entry_type = flags = 0;

	xbps_dictionary_get_bool(pkg_repod, "preserve", &preserve);
	ttype = xbps_transaction_pkg_type(pkg_repod);

	memset(&xucd, 0, sizeof(xucd));

	euid = geteuid();

	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname)) {
		return EINVAL;
	}

	if (xhp->flags & XBPS_FLAG_FORCE_UNPACK) {
		force = true;
	}

	if (ttype == XBPS_TRANS_UPDATE) {
		update = true;
	}

	/*
	 * Remove obsolete files.
	 */
	if (!preserve &&
	    xbps_dictionary_get_dict(xhp->transd, "obsolete_files", &obsd) &&
	    (obsoletes = xbps_dictionary_get(obsd, pkgname))) {
		for (unsigned int i = 0; i < xbps_array_count(obsoletes); i++) {
			const char *file = NULL;
			xbps_array_get_cstring_nocopy(obsoletes, i, &file);
			if (remove(file) == -1) {
				xbps_set_cb_state(xhp,
					XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL,
					errno, pkgver,
					"%s: failed to remove obsolete entry `%s': %s",
					pkgver, file, strerror(errno));
				continue;
			}
			xbps_set_cb_state(xhp,
				XBPS_STATE_REMOVE_FILE_OBSOLETE,
				0, pkgver, "%s: removed obsolete entry: %s", pkgver, file);
		}
	}

	/*
	 * Process the archive files.
	 */
	flags = set_extract_flags(euid);

	/*
	 * First get all metadata files on archive in this order:
	 * 	- INSTALL	<optional>
	 * 	- REMOVE 	<optional>
	 * 	- props.plist	<required> but currently ignored
	 * 	- files.plist	<required>
	 *
	 * The XBPS package must contain props and files plists, otherwise
	 * it's not a valid package.
	 */
	for (uint8_t i = 0; i < 4; i++) {
		ar_rv = archive_read_next_header(ar, &entry);
		if (ar_rv == ARCHIVE_EOF || ar_rv == ARCHIVE_FATAL)
			break;

		entry_pname = archive_entry_pathname(entry);
		entry_size = archive_entry_size(entry);

		if (strcmp("./INSTALL", entry_pname) == 0 ||
		    strcmp("./REMOVE", entry_pname) == 0 ||
		    strcmp("./props.plist", entry_pname) == 0) {
			archive_read_data_skip(ar);
		} else if (strcmp("./files.plist", entry_pname) == 0) {
			binpkg_filesd = xbps_archive_get_dictionary(ar, entry);
			if (binpkg_filesd == NULL) {
				rv = EINVAL;
				goto out;
			}
			break;
		} else {
			break;
		}
	}
	/*
	 * If there was any error extracting files from archive, error out.
	 */
	if (ar_rv == ARCHIVE_FATAL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, pkgver,
		    "%s: [unpack] 1: failed to extract files: %s",
		    pkgver, archive_error_string(ar));
		rv = EINVAL;
		goto out;
	}
	/*
	 * Bail out if required metadata files are not in archive.
	 */
	if (binpkg_filesd == NULL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, ENODEV, pkgver,
		    "%s: [unpack] invalid binary package `%s'.", pkgver, fname);
		rv = ENODEV;
		goto out;
	}

	/*
	 * Internalize current pkg metadata files plist.
	 */
	pkg_filesd = xbps_pkgdb_get_pkg_files(xhp, pkgname);

	/*
	 * Unpack all files on archive now.
	 */
	for (;;) {
		ar_rv = archive_read_next_header(ar, &entry);
		if (ar_rv == ARCHIVE_EOF || ar_rv == ARCHIVE_FATAL)
			break;
		else if (ar_rv == ARCHIVE_RETRY)
			continue;

		entry_pname = archive_entry_pathname(entry);
		entry_size = archive_entry_size(entry);
		entry_type = archive_entry_filetype(entry);
		entry_statp = archive_entry_stat(entry);
		/*
		 * Ignore directories from archive.
		 */
		if (entry_type == AE_IFDIR) {
			archive_read_data_skip(ar);
			continue;
		}
		/*
		 * Prepare unpack callback ops.
		 */
		if (xhp->unpack_cb != NULL) {
			xucd.xhp = xhp;
			xucd.pkgver = pkgver;
			xucd.entry = entry_pname;
			xucd.entry_size = entry_size;
			xucd.entry_is_conf = false;
			/*
			 * Compute total entries in progress data, if set.
			 * total_entries = files + conf_files + links.
			 */
			if (binpkg_filesd && !xucd_stats) {
				array = xbps_dictionary_get(binpkg_filesd, "files");
				xucd.entry_total_count +=
				    (ssize_t)xbps_array_count(array);
				array = xbps_dictionary_get(binpkg_filesd, "conf_files");
				xucd.entry_total_count +=
				    (ssize_t)xbps_array_count(array);
				array = xbps_dictionary_get(binpkg_filesd, "links");
				xucd.entry_total_count +=
				    (ssize_t)xbps_array_count(array);
				xucd_stats = true;
			}
		}
		/*
		 * Skip files that match noextract patterns from configuration file.
		 */
		if (xhp->noextract && xbps_patterns_match(xhp->noextract, entry_pname+1)) {
			xbps_dbg_printf("[unpack] %s skipped (matched by a pattern)\n", entry_pname+1);
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FILE_PRESERVED, 0,
			    pkgver, "%s: file `%s' won't be extracted, "
			    "it matches a noextract pattern.", pkgver, entry_pname);
			archive_read_data_skip(ar);
			continue;
		}
		/*
		 * Always check that extracted file exists and hash
		 * doesn't match, in that case overwrite the file.
		 * Otherwise skip extracting it.
		 */
		skip_extract = file_exists = keep_conf_file = false;
		if (lstat(entry_pname, &st) == 0)
			file_exists = true;
		/*
		 * Check if the file to be extracted must be preserved, if true,
		 * pass to the next file.
		 */
		if (file_exists && match_preserved_file(xhp, entry_pname)) {
			archive_read_data_skip(ar);
			xbps_dbg_printf("[unpack] `%s' exists on disk "
			    "and must be preserved, skipping.\n", entry_pname);
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FILE_PRESERVED, 0,
			    pkgver, "%s: file `%s' won't be extracted, "
			    "it's preserved.", pkgver, entry_pname);
			continue;
		}

		/*
		 * Check if current entry is a configuration file,
		 * that should be kept.
		 */
		if (!force && (entry_type == AE_IFREG)) {
			buf = strchr(entry_pname, '.') + 1;
			assert(buf != NULL);
			keep_conf_file = xbps_entry_is_a_conf_file(binpkg_filesd, buf);
		}

		/*
		 * If file to be extracted does not match the file type of
		 * file currently stored on disk and is not a conf file
		 * that should be kept, remove file on disk.
		 */
		if (file_exists && !keep_conf_file &&
		    ((entry_statp->st_mode & S_IFMT) != (st.st_mode & S_IFMT)))
			(void)remove(entry_pname);

		if (!force && (entry_type == AE_IFREG)) {
			if (file_exists && (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode))) {
				/*
				 * Handle configuration files.
				 * Skip packages that don't have "conf_files"
				 * array on its XBPS_PKGPROPS
				 * dictionary.
				 */
				if (keep_conf_file) {
					if (xhp->unpack_cb != NULL)
						xucd.entry_is_conf = true;

					rv = xbps_entry_install_conf_file(xhp,
					    binpkg_filesd, pkg_filesd, entry,
					    entry_pname, pkgver, S_ISLNK(st.st_mode));
					if (rv == -1) {
						/* error */
						goto out;
					} else if (rv == 0) {
						/*
						 * Keep curfile as is.
						 */
						skip_extract = true;
					}
					rv = 0;
				} else {
					rv = xbps_file_hash_check_dictionary(
					    xhp, binpkg_filesd, "files", buf);
					if (rv == -1) {
						/* error */
						xbps_dbg_printf(
						    "%s: failed to check"
						    " hash for `%s': %s\n",
						    pkgver, entry_pname,
						    strerror(errno));
						goto out;
					} else if (rv == 0) {
						/*
						 * hash match, skip extraction.
						 */
						xbps_dbg_printf(
						    "%s: file %s "
						    "matches existing SHA256, "
						    "skipping...\n",
						    pkgver, entry_pname);
						skip_extract = true;
					}
					rv = 0;
				}
			}
		}
		/*
		 * Check if current uid/gid differs from file in binpkg,
		 * and change permissions if true.
		 */
		if ((!force && file_exists && skip_extract && (euid == 0)) &&
		    (((archive_entry_uid(entry) != st.st_uid)) ||
		    ((archive_entry_gid(entry) != st.st_gid)))) {
			if (lchown(entry_pname,
			    archive_entry_uid(entry),
			    archive_entry_gid(entry)) != 0) {
				xbps_dbg_printf(
				    "%s: failed "
				    "to set uid/gid to %"PRIu64":%"PRIu64" (%s)\n",
				    pkgver, archive_entry_uid(entry),
				    archive_entry_gid(entry),
				    strerror(errno));
			} else {
				xbps_dbg_printf("%s: entry %s changed "
				    "uid/gid to %"PRIu64":%"PRIu64".\n", pkgver, entry_pname,
				    archive_entry_uid(entry),
				    archive_entry_gid(entry));
			}
		}
		/*
		 * Check if current file mode differs from file mode
		 * in binpkg and apply perms if true.
		 */
		if (!force && file_exists && skip_extract &&
		    (archive_entry_mode(entry) != st.st_mode)) {
			if (chmod(entry_pname,
			    archive_entry_mode(entry)) != 0) {
				xbps_dbg_printf(
				    "%s: failed "
				    "to set perms %s to %s: %s\n",
				    pkgver, archive_entry_strmode(entry),
				    entry_pname,
				    strerror(errno));
				rv = EINVAL;
				goto out;
			}
			xbps_dbg_printf("%s: entry %s changed file "
			    "mode to %s.\n", pkgver, entry_pname,
			    archive_entry_strmode(entry));
		}
		if (!force && skip_extract) {
			archive_read_data_skip(ar);
			continue;
		}
		/*
		 * Reset entry_pname again because if entry's pathname
		 * has been changed it will become a dangling pointer.
		 */
		entry_pname = archive_entry_pathname(entry);
		/*
		 * Extract entry from archive.
		 */
		if (archive_read_extract(ar, entry, flags) != 0) {
			error = archive_errno(ar);
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
			    error, pkgver,
			    "%s: [unpack] failed to extract file `%s': %s",
			    pkgver, entry_pname, strerror(error));
			break;
		} else {
			if (xhp->unpack_cb != NULL) {
				xucd.entry = entry_pname;
				xucd.entry_extract_count++;
				(*xhp->unpack_cb)(&xucd, xhp->unpack_cb_data);
			}
		}
	}
	/*
	 * If there was any error extracting files from archive, error out.
	 */
	if (error || ar_rv == ARCHIVE_FATAL) {
		rv = error;
		if (!rv)
			rv = ar_rv;
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, pkgver,
		    "%s: [unpack] failed to extract files: %s",
		    pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Externalize binpkg files.plist to disk, if not empty.
	 */
	if (xbps_dictionary_count(binpkg_filesd)) {
		mode_t prev_umask;
		prev_umask = umask(022);
		buf = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
		if (!xbps_dictionary_externalize_to_file(binpkg_filesd, buf)) {
			rv = errno;
			umask(prev_umask);
			free(buf);
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
			    rv, pkgver, "%s: [unpack] failed to externalize pkg "
			    "pkg metadata files: %s", pkgver, strerror(rv));
			goto out;
		}
		umask(prev_umask);
		free(buf);
	}
out:
	/*
	 * If unpacked pkg has no files, remove its files metadata plist.
	 */
	if (!xbps_dictionary_count(binpkg_filesd)) {
		buf = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
		unlink(buf);
		free(buf);
	}
	xbps_object_release(binpkg_filesd);

	return rv;
}

int HIDDEN
xbps_unpack_binary_pkg(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	char bpkg[PATH_MAX];
	struct archive *ar = NULL;
	struct stat st;
	const char *pkgver;
	ssize_t l;
	int pkg_fd = -1, rv = 0;
	mode_t myumask;

	assert(xbps_object_type(pkg_repod) == XBPS_TYPE_DICTIONARY);

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	xbps_set_cb_state(xhp, XBPS_STATE_UNPACK, 0, pkgver, NULL);

	l = xbps_pkg_path(xhp, bpkg, sizeof(bpkg), pkg_repod);
	if (l < 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    errno, pkgver,
		    "%s: [unpack] cannot determine binary package "
		    "file: %s", pkgver, strerror(errno));
		return -l;
	}

	if ((ar = archive_read_new()) == NULL)
		return ENOMEM;
	/*
	 * Enable support for tar format and some compression methods.
	 */
	archive_read_support_filter_gzip(ar);
	archive_read_support_filter_bzip2(ar);
	archive_read_support_filter_xz(ar);
	archive_read_support_filter_lz4(ar);
	archive_read_support_filter_zstd(ar);
	archive_read_support_format_tar(ar);

	myumask = umask(022);

	pkg_fd = open(bpkg, O_RDONLY|O_CLOEXEC);
	if (pkg_fd == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgver,
		    "%s: [unpack] failed to open binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (fstat(pkg_fd, &st) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgver,
		    "%s: [unpack] failed to fstat binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (archive_read_open_fd(ar, pkg_fd, st.st_blksize) == ARCHIVE_FATAL) {
		rv = archive_errno(ar);
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgver,
		    "%s: [unpack] failed to read binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	/*
	 * Externalize pkg files dictionary to metadir.
	 */
	if (access(xhp->metadir, R_OK|X_OK) == -1) {
		rv = errno;
		if (rv != ENOENT)
			goto out;

		if (xbps_mkpath(xhp->metadir, 0755) == -1) {
			rv = errno;
			goto out;
		}
	}
	/*
	 * Extract archive files.
	 */
	if ((rv = unpack_archive(xhp, pkg_repod, pkgver, bpkg, ar)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, pkgver,
		    "%s: [unpack] failed to unpack files from archive: %s",
		    pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Set package state to unpacked.
	 */
	if ((rv = xbps_set_pkg_state_dictionary(pkg_repod,
	    XBPS_PKG_STATE_UNPACKED)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgver,
		    "%s: [unpack] failed to set state to unpacked: %s",
		    pkgver, strerror(rv));
	}
	/* register alternatives */
	if ((rv = xbps_alternatives_register(xhp, pkg_repod)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgver,
		    "%s: [unpack] failed to register alternatives: %s",
		    pkgver, strerror(rv));
	}

out:
	if (pkg_fd != -1)
		close(pkg_fd);
	if (ar != NULL)
		archive_read_free(ar);

	/* restore */
	umask(myumask);

	return rv;
}
