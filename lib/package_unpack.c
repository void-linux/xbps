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
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "xbps_api_impl.h"

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
	char *file;

	if (xhp->preserved_files == NULL)
		return false;

	if (entry[0] == '.' && entry[1] != '\0') {
		file = strchr(entry, '.') + 1;
		assert(file);
	} else {
		file = __UNCONST(entry);
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
	xbps_dictionary_t binpkg_propsd, binpkg_filesd, pkg_filesd;
	xbps_array_t array, obsoletes;
	xbps_object_t obj;
	xbps_data_t data;
	const struct stat *entry_statp;
	void *instbuf = NULL, *rembuf = NULL;
	struct stat st;
	struct xbps_unpack_cb_data xucd;
	struct archive_entry *entry;
	size_t  instbufsiz = 0, rembufsiz = 0;
	ssize_t entry_size;
	const char *file, *entry_pname, *transact, *binpkg_pkgver;
	char *pkgname, *buf;
	int ar_rv, rv, error, entry_type, flags;
	bool preserve, update, file_exists;
	bool skip_extract, force, xucd_stats;
	uid_t euid;

	binpkg_propsd = binpkg_filesd = pkg_filesd = NULL;
	force = preserve = update = file_exists = false;
	xucd_stats = false;
	ar_rv = rv = error = entry_type = flags = 0;

	xbps_dictionary_get_bool(pkg_repod, "preserve", &preserve);
	xbps_dictionary_get_cstring_nocopy(pkg_repod, "transaction", &transact);

	memset(&xucd, 0, sizeof(xucd));

	euid = geteuid();

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);

	if (xhp->flags & XBPS_FLAG_FORCE_UNPACK)
		force = true;

	if (strcmp(transact, "update") == 0)
		update = true;
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

		if (strcmp("./INSTALL", entry_pname) == 0) {
			/*
			 * Store file in a buffer to execute it later.
			 */
			instbufsiz = entry_size;
			instbuf = malloc(entry_size);
			assert(instbuf);
			if (archive_read_data(ar, instbuf, entry_size) != entry_size) {
				rv = EINVAL;
				goto out;
			}
		} else if (strcmp("./REMOVE", entry_pname) == 0) {
			/*
			 * Store file in a buffer to execute it later.
			 */
			rembufsiz = entry_size;
			rembuf = malloc(entry_size);
			assert(rembuf);
			if (archive_read_data(ar, rembuf, entry_size) != entry_size) {
				rv = EINVAL;
				goto out;
			}
		} else if (strcmp("./props.plist", entry_pname) == 0) {
			binpkg_propsd = xbps_archive_get_dictionary(ar, entry);
			if (binpkg_propsd == NULL) {
				rv = EINVAL;
				goto out;
			}
		} else if (strcmp("./files.plist", entry_pname) == 0) {
			binpkg_filesd = xbps_archive_get_dictionary(ar, entry);
			if (binpkg_filesd == NULL) {
				rv = EINVAL;
				goto out;
			}
		} else {
			archive_read_data_skip(ar);
		}
		if (binpkg_filesd)
			break;
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
	if (binpkg_propsd == NULL || binpkg_filesd == NULL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, ENODEV, pkgver,
		    "%s: [unpack] invalid binary package `%s'.", pkgver, fname);
		rv = ENODEV;
		goto out;
	}

	/*
	 * Check that the pkgver in the binpkg matches the one we're looking for.
	 */
	xbps_dictionary_get_cstring_nocopy(binpkg_propsd, "pkgver", &binpkg_pkgver);
	if (strcmp(pkgver, binpkg_pkgver) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, EINVAL, pkgver,
		    "%s: [unpack] pkgver mismatch repodata: `%s' binpkg: `%s'.", fname, pkgver, binpkg_pkgver);
		rv = EINVAL;
		goto out;
	}

	/*
	 * Internalize current pkg metadata files plist.
	 */
	pkg_filesd = xbps_pkgdb_get_pkg_files(xhp, pkgname);

	/* Add pkg install/remove scripts data objects into our dictionary */
	if (instbuf != NULL) {
		data = xbps_data_create_data(instbuf, instbufsiz);
		assert(data);
		xbps_dictionary_set(pkg_repod, "install-script", data);
		xbps_object_release(data);
	}
	if (rembuf != NULL) {
		data = xbps_data_create_data(rembuf, rembufsiz);
		assert(data);
		xbps_dictionary_set(pkg_repod, "remove-script", data);
		xbps_object_release(data);
	}
	/*
	 * Execute INSTALL "pre" ACTION before unpacking files.
	 */
	if (instbuf != NULL) {
		rv = xbps_pkg_exec_buffer(xhp, instbuf, instbufsiz, pkgver, "pre", update);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, pkgver,
			    "%s: [unpack] INSTALL script failed to execute pre ACTION: %s",
			    pkgver, strerror(rv));
			goto out;
		}
	}
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
		 * Always check that extracted file exists and hash
		 * doesn't match, in that case overwrite the file.
		 * Otherwise skip extracting it.
		 */
		skip_extract = file_exists = false;
		if (lstat(entry_pname, &st) == 0)
			file_exists = true;
		/*
		 * Check if the file to be extracted must be preserved, if true,
		 * pass to the next file.
		 */
		if (file_exists && match_preserved_file(xhp, entry_pname)) {
			archive_read_data_skip(ar);
			xbps_dbg_printf(xhp, "[unpack] `%s' exists on disk "
			    "and must be preserved, skipping.\n", entry_pname);
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FILE_PRESERVED, 0,
			    pkgver, "%s: file `%s' won't be extracted, "
			    "it's preserved.\n", pkgver, entry_pname);
			continue;
		}
		/*
		 * If file to be extracted does not match the file type of
		 * file currently stored on disk, remove file on disk.
		 */
		if (file_exists &&
		    ((entry_statp->st_mode & S_IFMT) != (st.st_mode & S_IFMT)))
			(void)remove(entry_pname);

		if (!force && (entry_type == AE_IFREG)) {
			buf = strchr(entry_pname, '.') + 1;
			assert(buf != NULL);
			if (file_exists && S_ISREG(st.st_mode)) {
				/*
				 * Handle configuration files. Check if current
				 * entry is a configuration file and take action
				 * if required. Skip packages that don't have
				 * "conf_files" array on its XBPS_PKGPROPS
				 * dictionary.
				 */
				if (xbps_entry_is_a_conf_file(binpkg_filesd, buf)) {
					if (xhp->unpack_cb != NULL)
						xucd.entry_is_conf = true;

					rv = xbps_entry_install_conf_file(xhp,
					    binpkg_filesd, pkg_filesd, entry,
					    entry_pname, pkgver);
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
						xbps_dbg_printf(xhp,
						    "%s: failed to check"
						    " hash for `%s': %s\n",
						    pkgver, entry_pname,
						    strerror(errno));
						goto out;
					} else if (rv == 0) {
						/*
						 * hash match, skip extraction.
						 */
						xbps_dbg_printf(xhp,
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
				xbps_dbg_printf(xhp,
				    "%s: failed "
				    "to set uid/gid to %"PRIu64":%"PRIu64" (%s)\n",
				    pkgver, archive_entry_uid(entry),
				    archive_entry_gid(entry),
				    strerror(errno));
			} else {
				xbps_dbg_printf(xhp, "%s: entry %s changed "
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
				xbps_dbg_printf(xhp,
				    "%s: failed "
				    "to set perms %s to %s: %s\n",
				    pkgver, archive_entry_strmode(entry),
				    entry_pname,
				    strerror(errno));
				rv = EINVAL;
				goto out;
			}
			xbps_dbg_printf(xhp, "%s: entry %s changed file "
			    "mode to %s.\n", pkgver, entry_pname,
			    archive_entry_strmode(entry));
		}
		/*
		 * Check if current file mtime differs from archive entry
		 * in binpkg and apply mtime if true.
		 */
		if (!force && file_exists && skip_extract &&
		    (archive_entry_mtime_nsec(entry) != st.st_mtime)) {
			struct timespec ts[2];

			ts[0].tv_sec = archive_entry_atime(entry);
			ts[0].tv_nsec = archive_entry_atime_nsec(entry);
			ts[1].tv_sec = archive_entry_mtime(entry);
			ts[1].tv_nsec = archive_entry_mtime_nsec(entry);

			if (utimensat(AT_FDCWD, entry_pname, ts,
				      AT_SYMLINK_NOFOLLOW) == -1) {
				xbps_dbg_printf(xhp,
				    "%s: failed "
				    "to set mtime %lu to %s: %s\n",
				    pkgver, archive_entry_mtime_nsec(entry),
				    entry_pname,
				    strerror(errno));
				rv = EINVAL;
				goto out;
			}
			xbps_dbg_printf(xhp, "%s: updated file timestamps to %s\n",
			    pkgver, entry_pname);
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
	/*
	 * Skip checking for obsolete files on:
	 * 	- Package with "preserve" keyword.
	 */
	if (preserve) {
		xbps_dbg_printf(xhp, "%s: preserved package, skipping obsoletes\n", pkgver);
		goto out;
	}
	/*
	 * Check and remove obsolete files on:
	 * 	- Package reinstall.
	 * 	- Package upgrade.
	 */
	if (pkg_filesd == NULL || !xbps_dictionary_count(pkg_filesd))
		goto out;

	obsoletes = xbps_find_pkg_obsoletes(xhp, pkg_filesd, binpkg_filesd);
	for (unsigned int i = 0; i < xbps_array_count(obsoletes); i++) {
		obj = xbps_array_get(obsoletes, i);
		file = xbps_string_cstring_nocopy(obj);
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
		xbps_object_release(obj);
	}
	/* XXX: cant free obsoletes here, need to copy values before */
	xbps_object_release(pkg_filesd);
out:
	/*
	 * If unpacked pkg has no files, remove its files metadata plist.
	 */
	if (!xbps_dictionary_count(binpkg_filesd)) {
		buf = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
		unlink(buf);
		free(buf);
	}
	if (xbps_object_type(binpkg_propsd) == XBPS_TYPE_DICTIONARY)
		xbps_object_release(binpkg_propsd);
	if (xbps_object_type(binpkg_filesd) == XBPS_TYPE_DICTIONARY)
		xbps_object_release(binpkg_filesd);
	if (pkgname != NULL)
		free(pkgname);
	if (instbuf != NULL)
		free(instbuf);
	if (rembuf != NULL)
		free(rembuf);

	return rv;
}

int HIDDEN
xbps_unpack_binary_pkg(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	struct archive *ar = NULL;
	struct stat st;
	const char *pkgver;
	char *bpkg = NULL;
	int pkg_fd = -1, rv = 0;
	mode_t myumask;

	assert(xbps_object_type(pkg_repod) == XBPS_TYPE_DICTIONARY);

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	xbps_set_cb_state(xhp, XBPS_STATE_UNPACK, 0, pkgver, NULL);

	bpkg = xbps_repository_pkg_path(xhp, pkg_repod);
	if (bpkg == NULL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    errno, pkgver,
		    "%s: [unpack] cannot determine binary package "
		    "file for `%s': %s", pkgver, bpkg, strerror(errno));
		return errno;
	}

	if ((ar = archive_read_new()) == NULL) {
		free(bpkg);
		return ENOMEM;
	}
	/*
	 * Enable support for tar format and gzip/bzip2/lzma compression methods.
	 */
	archive_read_support_compression_gzip(ar);
	archive_read_support_compression_bzip2(ar);
	archive_read_support_compression_xz(ar);
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
	if (ar)
		archive_read_finish(ar);
	if (bpkg)
		free(bpkg);

	/* restore */
	umask(myumask);

	return rv;
}
