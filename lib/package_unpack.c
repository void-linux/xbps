/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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

/*
 * XXX WTF clearly clang should stfu and accept struct initializers
 * 		struct foo foo = {0};
 */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#endif

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

static const char *
find_pkg_symlink_target(xbps_dictionary_t d, const char *file)
{
	xbps_array_t links;
	xbps_object_t obj;
	const char *pkgfile, *tgt = NULL;
	char *rfile;

	links = xbps_dictionary_get(d, "links");
	for (unsigned int i = 0; i < xbps_array_count(links); i++) {
		rfile = strchr(file, '.') + 1;
		obj = xbps_array_get(links, i);
		xbps_dictionary_get_cstring_nocopy(obj, "file", &pkgfile);
		if (strcmp(rfile, pkgfile) == 0) {
			xbps_dictionary_get_cstring_nocopy(obj, "target", &tgt);
			break;
		}
	}

	return tgt;
}

static int
create_pkg_metaplist(struct xbps_handle *xhp, const char *pkgname, const char *pkgver,
		     xbps_dictionary_t propsd, xbps_dictionary_t filesd,
		     const void *instbuf, const size_t instbufsiz,
		     const void *rembuf, const size_t rembufsiz)
{
	xbps_array_t array;
	xbps_dictionary_t pkg_metad;
	xbps_data_t data;
	char *buf;
	int rv = 0;

	xbps_dictionary_make_immutable(propsd);
	pkg_metad = xbps_dictionary_copy_mutable(propsd);

	/* Add objects from XBPS_PKGFILES */
	array = xbps_dictionary_get(filesd, "files");
	if (xbps_array_count(array))
		xbps_dictionary_set(pkg_metad, "files", array);
	array = xbps_dictionary_get(filesd, "conf_files");
	if (xbps_array_count(array))
		xbps_dictionary_set(pkg_metad, "conf_files", array);
	array = xbps_dictionary_get(filesd, "links");
	if (xbps_array_count(array))
		xbps_dictionary_set(pkg_metad, "links", array);
	array = xbps_dictionary_get(filesd, "dirs");
	if (xbps_array_count(array))
		xbps_dictionary_set(pkg_metad, "dirs", array);

	/* Add install/remove scripts data objects */
	if (instbuf != NULL) {
		data = xbps_data_create_data(instbuf, instbufsiz);
		assert(data);
		xbps_dictionary_set(pkg_metad, "install-script", data);
		xbps_object_release(data);
	}
	if (rembuf != NULL) {
		data = xbps_data_create_data(rembuf, rembufsiz);
		assert(data);
		xbps_dictionary_set(pkg_metad, "remove-script", data);
		xbps_object_release(data);
	}
	/* Remove unneeded objs from transaction */
	xbps_dictionary_remove(pkg_metad, "remove-and-update");
	xbps_dictionary_remove(pkg_metad, "transaction");
	xbps_dictionary_remove(pkg_metad, "state");
	xbps_dictionary_remove(pkg_metad, "pkgname");
	xbps_dictionary_remove(pkg_metad, "version");

	/*
	 * Externalize pkg dictionary to metadir.
	 */
	if (access(xhp->metadir, R_OK|X_OK) == -1) {
		if (errno == ENOENT) {
			xbps_mkpath(xhp->metadir, 0755);
		} else {
			return errno;
		}
	}
	buf = xbps_xasprintf("%s/.%s.plist", XBPS_META_PATH, pkgname);
	if (!xbps_dictionary_externalize_to_file(pkg_metad, buf)) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    errno, pkgver,
		    "%s: [unpack] failed to write metadata file `%s': %s",
		    pkgver, buf, strerror(errno));
	}
	free(buf);
	xbps_object_release(pkg_metad);

	return rv;
}

static int
unpack_archive(struct xbps_handle *xhp,
	       xbps_dictionary_t pkg_repod,
	       const char *pkgver,
	       const char *fname,
	       struct archive *ar)
{
	xbps_dictionary_t propsd, filesd, metapropsd;
	xbps_array_t array, obsoletes;
	xbps_object_t obj;
	const struct stat *entry_statp;
	void *instbuf = NULL, *rembuf = NULL;
	struct stat st;
	struct xbps_unpack_cb_data xucd = {0};
	struct archive_entry *entry;
	size_t  instbufsiz = 0, rembufsiz = 0;
	ssize_t entry_size;
	const char *file, *entry_pname, *transact,  *tgtlnk;
	char *pkgname, *dname, *buf, *buf2, *p, *p2;
	int ar_rv, rv, entry_type, flags;
	bool preserve, update, conf_file, file_exists, skip_obsoletes;
	bool skip_extract, force, xucd_stats;
	uid_t euid;

	propsd = filesd = metapropsd = NULL;
	force = preserve = update = conf_file = file_exists = false;
	skip_obsoletes = xucd_stats = false;
	ar_rv = rv = entry_type = flags = 0;

	xbps_dictionary_get_bool(pkg_repod, "preserve", &preserve);
	xbps_dictionary_get_bool(pkg_repod, "skip-obsoletes", &skip_obsoletes);
	xbps_dictionary_get_cstring_nocopy(pkg_repod, "transaction", &transact);

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
	 * 	- props.plist	<required>
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
			propsd = xbps_archive_get_dictionary(ar, entry);
			if (propsd == NULL) {
				rv = EINVAL;
				goto out;
			}
		} else if (strcmp("./files.plist", entry_pname) == 0) {
			filesd = xbps_archive_get_dictionary(ar, entry);
			if (filesd == NULL) {
				rv = EINVAL;
				goto out;
			}
		}
		if (propsd && filesd)
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
	if (propsd == NULL || filesd == NULL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, ENODEV, pkgver,
		    "%s: [unpack] invalid binary package `%s'.", pkgver, fname);
		rv = ENODEV;
		goto out;
	}
	/*
	 * Create new metaplist file before unpacking any real file.
	 */
	metapropsd = xbps_pkgdb_get_pkg_metadata(xhp, pkgname);
	rv = create_pkg_metaplist(xhp, pkgname, pkgver,
	    propsd, filesd, instbuf, instbufsiz, rembuf, rembufsiz);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, pkgver,
		    "%s: [unpack] failed to create metaplist file: %s",
		    pkgver, strerror(rv));
		goto out;
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
			if (filesd && !xucd_stats) {
				array = xbps_dictionary_get(filesd, "files");
				xucd.entry_total_count +=
				    (ssize_t)xbps_array_count(array);
				array = xbps_dictionary_get(filesd, "conf_files");
				xucd.entry_total_count +=
				    (ssize_t)xbps_array_count(array);
				array = xbps_dictionary_get(filesd, "links");
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
		conf_file = skip_extract = file_exists = false;
		if (lstat(entry_pname, &st) == 0)
			file_exists = true;
		/*
		 * If file to be extracted does not match the file type of
		 * file currently stored on disk, remove file on disk.
		 */
		if (file_exists &&
		    ((entry_statp->st_mode & S_IFMT) != (st.st_mode & S_IFMT)))
			remove(entry_pname);

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
				if (xbps_entry_is_a_conf_file(filesd, buf)) {
					conf_file = true;
					if (xhp->unpack_cb != NULL)
						xucd.entry_is_conf = true;

					rv = xbps_entry_install_conf_file(xhp,
					    filesd, entry, entry_pname, pkgver,
					    pkgname);
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
					    xhp, filesd, "files", buf);
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
		} else if (!force && (entry_type == AE_IFLNK)) {
			/*
			 * Check if current link from binpkg hasn't been
			 * modified, otherwise extract new link.
			 */
			buf = realpath(entry_pname, NULL);
			if (buf) {
				if (strcmp(xhp->rootdir, "/")) {
					p = buf;
					p += strlen(xhp->rootdir);
				} else
					p = buf;
				tgtlnk = find_pkg_symlink_target(filesd,
				    entry_pname);
				assert(tgtlnk);
				if (strncmp(tgtlnk, "./", 2) == 0) {
					buf2 = strdup(entry_pname);
					assert(buf2);
					dname = dirname(buf2);
					p2 = xbps_xasprintf("%s/%s", dname, tgtlnk);
					free(buf2);
				} else {
					p2 = strdup(tgtlnk);
					assert(p2);
				}
				xbps_dbg_printf(xhp, "%s: symlink %s cur: %s "
				    "new: %s\n", pkgver, entry_pname, p, p2);

				if (strcmp(p, p2) == 0) {
					xbps_dbg_printf(xhp, "%s: symlink "
					    "%s matched, skipping...\n",
					    pkgver, entry_pname);
					skip_extract = true;
				}
				free(buf);
				free(p2);
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
				    "to set uid/gid to %u:%u (%s)\n",
				    pkgver, archive_entry_uid(entry),
				    archive_entry_gid(entry),
				    strerror(errno));
			} else {
				xbps_dbg_printf(xhp, "%s: entry %s changed "
				    "uid/gid to %u:%u.\n", pkgver, entry_pname,
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
		if (!update && conf_file && file_exists && !skip_extract) {
			/*
			 * If installing new package preserve old configuration
			 * file but renaming it to <file>.old.
			 */
			buf = xbps_xasprintf("%s.old", entry_pname);
			(void)rename(entry_pname, buf);
			free(buf);
			buf = NULL;
			xbps_set_cb_state(xhp,
			    XBPS_STATE_CONFIG_FILE, 0, pkgver,
			    "Renamed old configuration file "
			    "`%s' to `%s.old'.", entry_pname, entry_pname);
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
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
			    archive_errno(ar), pkgver,
			    "%s: [unpack] failed to extract file `%s': %s",
			    pkgver, entry_pname, archive_error_string(ar));
		} else {
			if (xhp->unpack_cb != NULL) {
				xucd.entry_extract_count++;
				(*xhp->unpack_cb)(&xucd, xhp->unpack_cb_data);
			}
		}
	}
	/*
	 * If there was any error extracting files from archive, error out.
	 */
	if (ar_rv == ARCHIVE_FATAL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, pkgver,
		    "%s: [unpack] failed to extract files: %s",
		    pkgver, archive_error_string(ar));
		goto out;
	}
	/*
	 * Skip checking for obsolete files on:
	 * 	- Package with "preserve" keyword.
	 * 	- Package with "skip-obsoletes" keyword.
	 */
	if (skip_obsoletes || preserve) {
		xbps_dbg_printf(xhp, "%s: skipping obsoletes\n", pkgver);
		goto out;
	}
	/*
	 * Check and remove obsolete files on:
	 * 	- Package reinstall.
	 * 	- Package upgrade.
	 */
	if (metapropsd == NULL || !xbps_dictionary_count(metapropsd))
		goto out;

	obsoletes = xbps_find_pkg_obsoletes(xhp, metapropsd, filesd);
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
	xbps_object_release(metapropsd);

out:
	if (xbps_object_type(filesd) == XBPS_TYPE_DICTIONARY)
		xbps_object_release(filesd);
	if (xbps_object_type(propsd) == XBPS_TYPE_DICTIONARY)
		xbps_object_release(propsd);
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
	if ((rv = xbps_set_pkg_state_installed(xhp, pkgver,
	    XBPS_PKG_STATE_UNPACKED)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgver,
		    "%s: [unpack] failed to set state to unpacked: %s",
		    pkgver, strerror(rv));
	}
out:
	if (pkg_fd != -1)
		close(pkg_fd);
	if (ar)
		archive_read_finish(ar);
	if (bpkg)
		free(bpkg);

	return rv;
}
