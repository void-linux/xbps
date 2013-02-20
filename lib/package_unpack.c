/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

static const char *
find_pkg_symlink_target(prop_dictionary_t d, const char *file)
{
	prop_array_t links;
	prop_object_t obj;
	size_t i;
	const char *pkgfile, *tgt = NULL;
	char *rfile;

	assert(d);

	links = prop_dictionary_get(d, "links");
	assert(links);

	for (i = 0; i < prop_array_count(links); i++) {
		rfile = strchr(file, '.') + 1;
		obj = prop_array_get(links, i);
		prop_dictionary_get_cstring_nocopy(obj, "file", &pkgfile);
		if (strcmp(rfile, pkgfile) == 0) {
			prop_dictionary_get_cstring_nocopy(obj, "target", &tgt);
			break;
		}
	}

	return tgt;
}

static int
unpack_archive(struct xbps_handle *xhp,
	       prop_dictionary_t pkg_repod,
	       struct archive *ar)
{
	prop_dictionary_t pkg_metad = NULL, filesd = NULL, old_filesd = NULL;
	prop_array_t array, obsoletes;
	prop_object_t obj;
	prop_data_t data;
	void *instbuf = NULL, *rembuf = NULL;
	const struct stat *entry_statp;
	struct stat st;
	struct xbps_unpack_cb_data xucd;
	struct archive_entry *entry;
	size_t i, entry_idx = 0;
	size_t instbufsiz, rembufsiz;
	ssize_t entry_size;
	const char *file, *entry_pname, *transact, *pkgname;
	const char *version, *pkgver, *fname, *tgtlnk;
	char *dname, *buf, *buf2, *p, *p2;
	int ar_rv, rv, rv_stat, flags;
	bool preserve, update, conf_file, file_exists, skip_obsoletes;
	bool softreplace, skip_extract, force;
	uid_t euid;

	assert(prop_object_type(pkg_repod) == PROP_TYPE_DICTIONARY);
	assert(ar != NULL);

	force = preserve = update = conf_file = file_exists = false;
	skip_obsoletes = softreplace = false;

	prop_dictionary_get_bool(pkg_repod, "preserve", &preserve);
	prop_dictionary_get_bool(pkg_repod, "skip-obsoletes", &skip_obsoletes);
	prop_dictionary_get_bool(pkg_repod, "softreplace", &softreplace);
	prop_dictionary_get_cstring_nocopy(pkg_repod,
	    "transaction", &transact);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "version", &version);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "filename", &fname);

	euid = geteuid();

	if (xhp->flags & XBPS_FLAG_FORCE_UNPACK)
		force = true;

	if (xhp->unpack_cb != NULL) {
		/* initialize data for unpack cb */
		memset(&xucd, 0, sizeof(xucd));
	}
	if (access(xhp->rootdir, R_OK) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			goto out;
		}
		if (xbps_mkpath(xhp->rootdir, 0750) == -1) {
			rv = errno;
			goto out;
		}
	}
	if (chdir(xhp->rootdir) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    errno, pkgname, version,
		    "%s: [unpack] failed to chdir to rootdir `%s': %s",
		    pkgver, xhp->rootdir, strerror(errno));
		rv = errno;
		goto out;
	}
	if (strcmp(transact, "update") == 0)
		update = true;
	/*
	 * Process the archive files.
	 */
	for (;;) {
		ar_rv = archive_read_next_header(ar, &entry);
		if (ar_rv == ARCHIVE_EOF || ar_rv == ARCHIVE_FATAL)
			break;
		else if (ar_rv == ARCHIVE_RETRY)
			continue;

		entry_statp = archive_entry_stat(entry);
		entry_pname = archive_entry_pathname(entry);
		entry_size = archive_entry_size(entry);
		flags = set_extract_flags(euid);
		/*
		 * Ignore directories from archive.
		 */
		if (S_ISDIR(entry_statp->st_mode)) {
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
		}
		if (strcmp("./INSTALL", entry_pname) == 0) {
			/*
			 * Store file in a buffer and execute
			 * the "pre" action from it.
			 */
			instbufsiz = entry_size;
			instbuf = malloc(entry_size);
			assert(instbuf);

			if (archive_read_data(ar, instbuf, entry_size) !=
			    entry_size) {
				rv = EINVAL;
				free(instbuf);
				goto out;
			}

			rv = xbps_pkg_exec_buffer(xhp, instbuf, instbufsiz,
					pkgname, version, "pre", update);
			if (rv != 0) {
				xbps_set_cb_state(xhp,
				    XBPS_STATE_UNPACK_FAIL,
				    rv, pkgname, version,
				    "%s: [unpack] INSTALL script failed "
				    "to execute pre ACTION: %s",
				    pkgver, strerror(rv));
				free(instbuf);
				goto out;
			}
			continue;

		} else if (strcmp("./REMOVE", entry_pname) == 0) {
			/* store file in a buffer */
			rembufsiz = entry_size;
			rembuf = malloc(entry_size);
			assert(rembuf);
			if (archive_read_data(ar, rembuf, entry_size) !=
			    entry_size) {
				rv = EINVAL;
				free(rembuf);
				goto out;
			}
			continue;

		} else if (strcmp("./files.plist", entry_pname) == 0) {
			/*
			 * Internalize this entry into a prop_dictionary
			 * to check for obsolete files if updating a package.
			 * It will be extracted to disk at the end.
			 */
			filesd = xbps_dictionary_from_archive_entry(ar, entry);
			if (filesd == NULL) {
				rv = errno;
				goto out;
			}
			continue;
		} else if (strcmp("./props.plist", entry_pname) == 0) {
			/* ignore this one; we use pkg data from repo index */
			archive_read_data_skip(ar);
			continue;
		}
		/*
		 * If XBPS_PKGFILES or XBPS_PKGPROPS weren't found
		 * in the archive at this phase, skip all data.
		 */
		if (filesd == NULL) {
			archive_read_data_skip(ar);
			/*
			 * If we have processed 4 entries and the two
			 * required metadata files weren't found, bail out.
			 * This is not an XBPS binary package.
			 */
			if (entry_idx >= 3) {
				xbps_set_cb_state(xhp,
				    XBPS_STATE_UNPACK_FAIL,
				    ENODEV, pkgname, version,
				    "%s: [unpack] invalid binary package `%s'.",
				    pkgver, fname);
				rv = ENODEV;
				goto out;
			}

			entry_idx++;
			continue;
		}
		/*
		 * Compute total entries in progress data, if set.
		 * total_entries = files + conf_files + links.
		 */
		if (xhp->unpack_cb != NULL) {
			xucd.entry_total_count = 0;
			array = prop_dictionary_get(filesd, "files");
			xucd.entry_total_count +=
			    (ssize_t)prop_array_count(array);
			array = prop_dictionary_get(filesd, "conf_files");
			xucd.entry_total_count +=
			    (ssize_t)prop_array_count(array);
			array = prop_dictionary_get(filesd, "links");
			xucd.entry_total_count +=
			    (ssize_t)prop_array_count(array);
		}
		/*
		 * Always check that extracted file exists and hash
		 * doesn't match, in that case overwrite the file.
		 * Otherwise skip extracting it.
		 */
		conf_file = skip_extract = file_exists = false;
		rv_stat = lstat(entry_pname, &st);
		if (rv_stat == 0)
			file_exists = true;

		if (!force && S_ISREG(entry_statp->st_mode)) {
			buf = strchr(entry_pname, '.') + 1;
			assert(buf != NULL);
			if (file_exists) {
				/*
				 * Handle configuration files. Check if current
				 * entry is a configuration file and take action
				 * if required. Skip packages that don't have
				 * "conf_files" array on its XBPS_PKGPROPS
				 * dictionary.
				 */
				if (xbps_entry_is_a_conf_file(pkg_repod, buf)) {
					conf_file = true;
					if (xhp->unpack_cb != NULL)
						xucd.entry_is_conf = true;

					rv = xbps_entry_install_conf_file(xhp,
					    filesd, entry, entry_pname,
					    pkgname, version);
					if (rv == -1) {
						/* error */
						goto out;
					} else if (rv == 0) {
						/*
						 * Keep curfile as is.
						 */
						skip_extract = true;
					}
				} else {
					rv = xbps_file_hash_check_dictionary(
					    xhp, filesd, "files", buf);
					if (rv == -1) {
						/* error */
						xbps_dbg_printf(xhp,
						    "%s-%s: failed to check"
						    " hash for `%s': %s\n",
						    pkgname, version,
						    entry_pname,
						    strerror(errno));
						goto out;
					} else if (rv == 0) {
						/*
						 * hash match, skip extraction.
						 */
						xbps_dbg_printf(xhp,
						    "%s-%s: file %s "
						    "matches existing SHA256, "
						    "skipping...\n", pkgname,
						    version, entry_pname);
						skip_extract = true;
					}
				}
			}
		} else if (!force && S_ISLNK(entry_statp->st_mode)) {
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
					xbps_dbg_printf(xhp, "%s-%s: symlink "
					    "%s matched, skipping...\n",
					    pkgname, version, entry_pname);
					skip_extract = true;
				}
				free(buf);
			}
		}
		/*
		 * Check if current file mode differs from file mode
		 * in binpkg and apply perms if true.
		 */
		if (!force && file_exists && skip_extract &&
		    (entry_statp->st_mode != st.st_mode)) {
			if (chmod(entry_pname,
			    entry_statp->st_mode) != 0) {
				xbps_dbg_printf(xhp,
				    "%s-%s: failed "
				    "to set perms %s to %s: %s\n",
				    pkgname, version,
				    archive_entry_strmode(entry),
				    entry_pname,
				    strerror(errno));
				rv = EINVAL;
				goto out;
			}
			xbps_dbg_printf(xhp, "%s-%s: entry %s changed file "
			    "mode to %s.\n", pkgname, version, entry_pname,
			    archive_entry_strmode(entry));
		}
		/*
		 * Check if current uid/gid differs from file in binpkg,
		 * and change permissions if true.
		 */
		if ((!force && file_exists && skip_extract && (euid == 0)) &&
		    (((entry_statp->st_uid != st.st_uid)) ||
		    ((entry_statp->st_gid != st.st_gid)))) {
			if (chown(entry_pname,
			    entry_statp->st_uid, entry_statp->st_gid) != 0) {
				xbps_dbg_printf(xhp,
				    "%s-%s: failed "
				    "to set uid/gid to %u:%u (%s)\n",
				    pkgname, version,
				    entry_statp->st_uid, entry_statp->st_gid,
				    strerror(errno));
			} else {
				xbps_dbg_printf(xhp, "%s-%s: entry %s changed "
				    "uid/gid to %u:%u.\n", pkgname, version,
				    entry_pname,
				    entry_statp->st_uid, entry_statp->st_gid);
			}
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
			    XBPS_STATE_CONFIG_FILE, 0,
			    pkgname, version,
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
			rv = archive_errno(ar);
			xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
			    rv, pkgname, version,
			    "%s: [unpack] failed to extract file `%s': %s",
			    pkgver, entry_pname, strerror(rv));
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
	if ((rv = archive_errno(ar)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgname, version,
		    "%s: [unpack] failed to extract files: %s",
		    pkgver, fname, archive_error_string(ar));
		goto out;
	}
	/*
	 * Skip checking for obsolete files on:
	 * 	- New package installation without "softreplace" keyword.
	 * 	- Package with "preserve" keyword.
	 * 	- Package with "skip-obsoletes" keyword.
	 */

	if (skip_obsoletes || preserve || (!softreplace && !update))
		goto out1;
	/*
	 * Check and remove obsolete files on:
	 * 	- Package upgrade.
	 * 	- Package with "softreplace" keyword.
	 */
	old_filesd = xbps_pkgdb_get_pkg_metadata(xhp, pkgname);
	if (old_filesd == NULL)
		goto out1;

	obsoletes = xbps_find_pkg_obsoletes(xhp, old_filesd, filesd);
	for (i = 0; i < prop_array_count(obsoletes); i++) {
		obj = prop_array_get(obsoletes, i);
		file = prop_string_cstring_nocopy(obj);
		if (remove(file) == -1) {
			xbps_set_cb_state(xhp,
			    XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL,
			    errno, pkgname, version,
			    "%s: failed to remove obsolete entry `%s': %s",
			    pkgver, file, strerror(errno));
			continue;
		}
		xbps_set_cb_state(xhp,
		    XBPS_STATE_REMOVE_FILE_OBSOLETE,
		    0, pkgname, version,
		    "%s: removed obsolete entry: %s", pkgver, file);
		prop_object_release(obj);
	}

out1:
	prop_dictionary_make_immutable(pkg_repod);
	pkg_metad = prop_dictionary_copy_mutable(pkg_repod);

	/* Add objects from XBPS_PKGFILES */
	array = prop_dictionary_get(filesd, "files");
	if (array && prop_array_count(array))
		prop_dictionary_set(pkg_metad, "files", array);
	array = prop_dictionary_get(filesd, "conf_files");
	if (array && prop_array_count(array))
		prop_dictionary_set(pkg_metad, "conf_files", array);
	array = prop_dictionary_get(filesd, "links");
	if (array && prop_array_count(array))
		prop_dictionary_set(pkg_metad, "links", array);
	array = prop_dictionary_get(filesd, "dirs");
	if (array && prop_array_count(array))
		prop_dictionary_set(pkg_metad, "dirs", array);

	/* Add install/remove scripts data objects */
	if (instbuf != NULL) {
		data = prop_data_create_data(instbuf, instbufsiz);
		assert(data);
		prop_dictionary_set(pkg_metad, "install-script", data);
		prop_object_release(data);
		free(instbuf);
	}
	if (rembuf != NULL) {
		data = prop_data_create_data(rembuf, rembufsiz);
		assert(data);
		prop_dictionary_set(pkg_metad, "remove-script", data);
		prop_object_release(data);
		free(rembuf);
	}
	/* Remove unneeded objs from transaction */
	prop_dictionary_remove(pkg_metad, "remove-and-update");
	prop_dictionary_remove(pkg_metad, "transaction");
	prop_dictionary_remove(pkg_metad, "state");

	/*
	 * Externalize pkg dictionary to metadir.
	 */
	if (access(xhp->metadir, R_OK|X_OK) == -1) {
		if (errno == ENOENT) {
			xbps_mkpath(xhp->metadir, 0755);
		} else {
			rv = errno;
			goto out;
		}
	}
	buf = xbps_xasprintf("%s/.%s.plist", XBPS_META_PATH, pkgname);
	if (!prop_dictionary_externalize_to_file(pkg_metad, buf)) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    errno, pkgname, version,
		    "%s: [unpack] failed to extract metadata file `%s': %s",
		    pkgver, buf, strerror(errno));
		free(buf);
		goto out;
	}
	free(buf);
out:
	if (prop_object_type(pkg_metad) == PROP_TYPE_DICTIONARY)
		prop_object_release(pkg_metad);
	if (prop_object_type(filesd) == PROP_TYPE_DICTIONARY)
		prop_object_release(filesd);

	return rv;
}

int HIDDEN
xbps_unpack_binary_pkg(struct xbps_handle *xhp, prop_dictionary_t pkg_repod)
{
	struct archive *ar = NULL;
	const char *pkgname, *version, *pkgver;
	char *bpkg;
	int pkg_fd, rv = 0;

	assert(prop_object_type(pkg_repod) == PROP_TYPE_DICTIONARY);

	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "version", &version);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);

	xbps_set_cb_state(xhp, XBPS_STATE_UNPACK, 0, pkgname, version, NULL);

	bpkg = xbps_repository_pkg_path(xhp, pkg_repod);
	if (bpkg == NULL) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    errno, pkgname, version,
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
		rv = archive_errno(ar);
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgname, version,
		    "%s: [unpack] failed to open binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		free(bpkg);
		archive_read_free(ar);
		return rv;
	}
	archive_read_open_fd(ar, pkg_fd, ARCHIVE_READ_BLOCKSIZE);
	free(bpkg);

	/*
	 * Extract archive files.
	 */
	if ((rv = unpack_archive(xhp, pkg_repod, ar)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgname, version,
		    "%s: [unpack] failed to unpack files from archive: %s",
		    pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Set package state to unpacked.
	 */
	if ((rv = xbps_set_pkg_state_installed(xhp, pkgname, version,
	    XBPS_PKG_STATE_UNPACKED)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL,
		    rv, pkgname, version,
		    "%s: [unpack] failed to set state to unpacked: %s",
		    pkgver, strerror(rv));
	}
out:
	close(pkg_fd);
	if (ar)
		archive_read_free(ar);

	return rv;
}
