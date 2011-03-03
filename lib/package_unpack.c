/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "xbps_api_impl.h"

/**
 * @file lib/package_unpack.c
 * @brief Binary package file unpacking routines
 * @defgroup unpack Binary package file unpacking functions
 *
 * Unpacking a binary package involves the following steps:
 *  - Its <b>pre-install</b> target in the INSTALL script is executed
 *    (if available).
 *  - Metadata files are extracted.
 *  - All other kind of files on archive are extracted.
 *  - Handles configuration files by taking care of updating them with
 *    new versions if necessary and to not overwrite modified ones.
 *  - Files from installed package are compared with new package and
 *    obsolete files are removed.
 *  - Finally its state is set to XBPS_PKG_STATE_UNPACKED.
 *
 * The following image shown below represents a transaction dictionary
 * returned by xbps_transaction_prepare():
 *
 * @image html images/xbps_transaction_dictionary.png
 *
 * Legend:
 *   - <b>Salmon filled box</b>: The transaction dictionary.
 *   - <b>White filled box</b>: mandatory objects.
 *   - <b>Grey filled box</b>: optional objects.
 *   - <b>Green filled box</b>: possible value set in the object, only one of
 *     them is set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static void
set_extract_flags(int *flags, bool update)
{
	int lflags = 0;

	if (getuid() == 0)
		lflags = FEXTRACT_FLAGS;
	else
		lflags = EXTRACT_FLAGS;

	if (!update) {
		/*
		 * Only overwrite files while updating.
		 */
		lflags |= ARCHIVE_EXTRACT_NO_OVERWRITE;
		lflags |= ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
	}

	*flags = lflags;
}

static int
extract_metafile(struct archive *ar,
		 struct archive_entry *entry,
		 const char *file,
		 const char *pkgname,
		 const char *version,
		 bool exec,
		 int flags)
{
	char *buf;
	int rv;

	buf = xbps_xasprintf(".%s/metadata/%s/%s",
	    XBPS_META_PATH, pkgname, file);
	if (buf == NULL)
		return ENOMEM;

	archive_entry_set_pathname(entry, buf);
	free(buf);
	if (exec)
		archive_entry_set_perm(entry, 0750);

	if (archive_read_extract(ar, entry, flags) != 0) {
		if ((rv = archive_errno(ar)) != EEXIST) {
			xbps_error_printf("failed to extract metadata file `%s'"
			    "for `%s-%s': %s\n", file, pkgname, version,
			    strerror(rv));
		}
	}

	return 0;
}

static int
remove_metafile(const char *file, const char *pkgname, const char *version)
{
	char *buf;

	buf = xbps_xasprintf(".%s/metadata/%s/%s",
	    XBPS_META_PATH, file, pkgname);
	if (buf == NULL)
		return ENOMEM;

	if (unlink(buf) == -1) {
		if (errno && errno != ENOENT) {
			xbps_error_printf("failed to remove metadata file "
			    "`%s' while unpacking `%s-%s': %s\n", file,
			    pkgname, version, strerror(errno));
			free(buf);
			return errno;
		}
	}
	free(buf);

	return 0;
}

static int
remove_file_wrong_hash(prop_dictionary_t d, const char *file)
{
	struct stat st;
	const char *hash;
	int rv = 0;

	if (stat(file, &st) == -1)
		if (errno != ENOENT)
			return errno;

	if (!S_ISREG(st.st_mode))
		return 0;

	/* Only check for regular files, not symlinks, dirs or conffiles. */
	hash = xbps_get_file_hash_from_dict(d, "files", file);
	if (hash) {
		rv = xbps_check_file_hash(file, hash);
		if (rv == ERANGE) {
			(void)unlink(file);
			xbps_warn_printf("Removed `%s' entry with "
			   "unmatched hash.\n", file);
		}
	}
	return rv;
}

/*
 * Execute the unpack progress function callback if set and its
 * private data is also set. It's so sad that
 * archive_read_set_progress_callback() from libarchive(3) cannot be used
 * here because sometimes it misses some entries by unknown reasons.
 */
#define RUN_PROGRESS_CB()							\
do {										\
	if (xhp != NULL && xhp->xbps_unpack_cb != NULL && xhp->xupd != NULL)	\
		(*xhp->xbps_unpack_cb)(xhp->xupd);				\
} while (0)

static int
unpack_archive(prop_dictionary_t pkg_repod,
	       struct archive *ar,
	       const char *pkgname,
	       const char *version,
	       const struct xbps_handle *xhp)
{
	prop_dictionary_t propsd = NULL, filesd = NULL, old_filesd = NULL;
	prop_array_t array;
	struct archive_entry *entry;
	size_t nmetadata = 0, entry_idx = 0;
	const char *entry_pname, *transact;
	char *buf;
	int rv, flags;
	bool preserve, update;

	assert(ar != NULL);
	assert(pkg_repod != NULL);
	assert(pkgname != NULL);
	assert(version != NULL);

	preserve = update = false;

	if (chdir(xhp->rootdir) == -1) {
		xbps_error_printf("cannot chdir to rootdir for "
		    "`%s-%s': %s\n", pkgname, version, strerror(errno));
		return errno;
	}

	prop_dictionary_get_bool(pkg_repod, "preserve", &preserve);
	prop_dictionary_get_cstring_nocopy(pkg_repod,
	    "transaction", &transact);
	assert(transact != NULL);

	if (strcmp(transact, "update") == 0)
		update = true;

	/*
	 * While updating, always remove current INSTALL/REMOVE
	 * scripts, because a package upgrade might not have those
	 * anymore.
	 */
	if (update) {
		if ((rv = remove_metafile("INSTALL", pkgname, version)) != 0)
			return rv;
		if ((rv = remove_metafile("REMOVE", pkgname, version)) != 0)
			return rv;
	}
	/*
	 * Process the archive files.
	 */
	while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		entry_pname = archive_entry_pathname(entry);
		set_extract_flags(&flags, update);
		if (xhp != NULL && xhp->xbps_unpack_cb != NULL &&
		    xhp->xupd != NULL) {
			xhp->xupd->entry = entry_pname;
			xhp->xupd->entry_size = archive_entry_size(entry);
			xhp->xupd->entry_is_metadata = false;
			xhp->xupd->entry_is_conf = false;
		}

		if (strcmp("./INSTALL", entry_pname) == 0) {
			/*
			 * Extract the INSTALL script first to execute
			 * the pre install target.
			 */
			buf = xbps_xasprintf(".%s/metadata/%s/INSTALL",
			    XBPS_META_PATH, pkgname);
			if (buf == NULL) {
				rv = ENOMEM;
				goto out;
			}
			rv = extract_metafile(ar, entry, "INSTALL",
			    pkgname, version, true, flags);
			if (rv != 0) {
				free(buf);
				goto out;
			}
			rv = xbps_file_exec(buf, "pre",
			     pkgname, version, update ? "yes" : "no", NULL);
			free(buf);
			if (rv != 0) {
				xbps_error_printf("%s-%s: pre-install script "
				    "error: %s\n", pkgname, version,
				    strerror(rv));
				goto out;
			}
			nmetadata++;
			if (xhp->xupd != NULL) {
				xhp->xupd->entry_is_metadata = true;
				xhp->xupd->entry_extract_count++;
			}
			RUN_PROGRESS_CB();
			continue;

		} else if (strcmp("./REMOVE", entry_pname) == 0) {
			rv = extract_metafile(ar, entry, "REMOVE",
			    pkgname, version, true, flags);
			if (rv != 0)
				goto out;

			nmetadata++;
			if (xhp->xupd != NULL) {
				xhp->xupd->entry_is_metadata = true;
				xhp->xupd->entry_extract_count++;
			}
			RUN_PROGRESS_CB();
			continue;

		} else if (strcmp("./files.plist", entry_pname) == 0) {
			/*
			 * Internalize this entry into a prop_dictionary
			 * to check for obsolete files if updating a package.
			 * It will be extracted to disk at the end.
			 */
			filesd = xbps_read_dict_from_archive_entry(ar, entry);
			if (filesd == NULL) {
				rv = errno;
				goto out;
			}
			nmetadata++;
			if (xhp->xupd != NULL) {
				xhp->xupd->entry_is_metadata = true;
				xhp->xupd->entry_extract_count++;
			}
			RUN_PROGRESS_CB();
			continue;

		} else if (strcmp("./props.plist", entry_pname) == 0) {
			rv = extract_metafile(ar, entry, XBPS_PKGPROPS,
			    pkgname, version, false, flags);
			if (rv != 0)
				goto out;

			propsd = xbps_get_pkg_dict_from_metadata_plist(
			    pkgname, XBPS_PKGPROPS);
			if (propsd == NULL) {
				rv = errno;
				goto out;
			}
			nmetadata++;
			if (xhp->xupd != NULL) {
				xhp->xupd->entry_is_metadata = true;
				xhp->xupd->entry_extract_count++;
			}
			RUN_PROGRESS_CB();
			continue;
		}
		/*
		 * If XBPS_PKGFILES or XBPS_PKGPROPS weren't found
		 * in the archive at this phase, skip all data.
		 */
		if (propsd == NULL || filesd == NULL) {
			archive_read_data_skip(ar);
			/*
			 * If we have processed 4 entries and the two
			 * required metadata files weren't found, bail out.
			 * This is not an XBPS binary package.
			 */
			if (entry_idx >= 3) {
				xbps_error_printf("invalid binary pkg archive"
				    "for `%s-%s'\n", pkgname, version);
				return ENODEV;
			}

			entry_idx++;
			continue;
		}
		/*
		 * Compute total entries in progress data, if set.
		 * total_entries = metadata + files + conf_files + links.
		 */
		if (xhp->xupd != NULL) {
			xhp->xupd->entry_total_count = nmetadata;
			array = prop_dictionary_get(filesd, "files");
			xhp->xupd->entry_total_count +=
			    (ssize_t)prop_array_count(array);
			array = prop_dictionary_get(filesd, "conf_files");
			xhp->xupd->entry_total_count +=
			    (ssize_t)prop_array_count(array);
			array = prop_dictionary_get(filesd, "links");
			xhp->xupd->entry_total_count +=
			    (ssize_t)prop_array_count(array);
		}

		/*
		 * Handle configuration files. Check if current entry is
		 * a configuration file and take action if required. Skip
		 * packages that don't have the "conf_files" array in
		 * the XBPS_PKGPROPS dictionary.
		 */
		rv = xbps_entry_is_a_conf_file(propsd, entry_pname);
		if (rv == -1) {
			/* error */
			goto out;
		} else if (rv == 1) {
			if (xhp->xupd != NULL)
				xhp->xupd->entry_is_conf = true;

			rv = xbps_entry_install_conf_file(filesd,
			    entry, entry_pname, pkgname, version);
			if (rv == -1) {
				/* error */
				goto out;
			} else if (rv == 1) {
				/*
				 * Configuration file should be installed.
				 */
				flags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE;
				flags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
			} else {
				/*
				 * Keep current configuration file
				 * as is now and pass to next entry.
				 */
				archive_read_data_skip(ar);
				RUN_PROGRESS_CB();
				continue;
			}
		}
		/*
		 * Check if current entry already exists on disk and
		 * if the sha256 hash doesn't match, remove the file.
		 * Only do this if we are _installing_ a package.
		 */
		if (!update) {
			rv = remove_file_wrong_hash(filesd, entry_pname);
			if (rv != 0) {
				xbps_dbg_printf("remove_file_wrong_hash "
				    "failed for `%s': %s\n", entry_pname,
				    strerror(rv));
				goto out;
			}
		}

		/*
		 * Extract entry from archive.
		 */
		if (archive_read_extract(ar, entry, flags) != 0) {
			rv = archive_errno(ar);
			if (rv != EEXIST) {
				xbps_error_printf("failed to extract `%s' "
				    "from `%s-%s': %s\n", entry_pname,
				    pkgname, version, strerror(rv));
				goto out;
			} else {
				if (xhp->flags & XBPS_FLAG_VERBOSE)
					xbps_warn_printf("ignoring existing "
					    "entry: %s\n", entry_pname);

				RUN_PROGRESS_CB();
				continue;
			}
		}
		if (xhp->xupd != NULL)
			xhp->xupd->entry_extract_count++;

		RUN_PROGRESS_CB();
	}

	if ((rv = archive_errno(ar)) == 0) {
		buf = xbps_xasprintf(".%s/metadata/%s/%s",
		    XBPS_META_PATH, pkgname, XBPS_PKGFILES);
		if (buf == NULL) {
			rv = ENOMEM;
			goto out;
		}
		/*
		 * Check if files.plist exists and pkg is NOT marked as
		 * preserve, in that case we need to check for obsolete files
		 * and remove them if necessary.
		 */
		if (!preserve) {
			old_filesd =
			    prop_dictionary_internalize_from_zfile(buf);
			if (old_filesd) {
				rv = xbps_remove_obsoletes(old_filesd, filesd);
				if (rv != 0) {
					prop_object_release(old_filesd);
					free(buf);
					rv = errno;
					goto out;
				}
				prop_object_release(old_filesd);

			} else if (errno && errno != ENOENT) {
				free(buf);
				rv = errno;
				goto out;
			}
		}
		/*
		 * Now that all files were successfully unpacked, we
		 * can safely externalize files.plist because the path
		 * is reachable.
		 */
		if (!prop_dictionary_externalize_to_zfile(filesd, buf)) {
			rv = errno;
			xbps_error_printf("failed to extract metadata %s file"
			    "for `%s-%s': %s\n", XBPS_PKGFILES, pkgname,
			    version, strerror(rv));
			free(buf);
			goto out;
		}
		free(buf);
	}

out:
	if (filesd)
		prop_object_release(filesd);
	if (propsd)
		prop_object_release(propsd);

	return rv;
}
#undef RUN_PROGRESS_CB

int
xbps_unpack_binary_pkg(prop_dictionary_t pkg_repod)
{
	const struct xbps_handle *xhp;
	struct archive *ar;
	const char *pkgname, *version, *repoloc, *pkgver;
	char *bpkg;
	int rv = 0;

	assert(pkg_repod != NULL);

	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "version", &version);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "repository", &repoloc);

	bpkg = xbps_get_binpkg_repo_uri(pkg_repod, repoloc);
	if (bpkg == NULL) {
		xbps_error_printf("cannot determine binary pkg file "
		    "for `%s-%s': %s\n", pkgname, version, strerror(errno));
		return errno;
	}

	ar = archive_read_new();
	if (ar == NULL) {
		rv = ENOMEM;
		goto out;
	}
	/*
	 * Enable support for tar format and all compression methods.
	 */
	archive_read_support_compression_all(ar);
	archive_read_support_format_tar(ar);

	if (archive_read_open_filename(ar, bpkg, ARCHIVE_READ_BLOCKSIZE) != 0) {
		rv = archive_errno(ar);
		xbps_error_printf("failed to open `%s' binpkg: %s\n",
		    bpkg, strerror(rv));
		goto out;
	}
	/*
	 * Set extract progress callback if specified.
	 */
	xhp = xbps_handle_get();
	if (xhp != NULL && xhp->xbps_unpack_cb != NULL && xhp->xupd != NULL) {
		xhp->xupd->entry_extract_count = 0;
		xhp->xupd->entry_total_count = 0;
	}
	/*
	 * Extract archive files.
	 */
	rv = unpack_archive(pkg_repod, ar, pkgname, version, xhp);
	if (rv != 0) {
		xbps_error_printf("failed to unpack `%s' binpkg: %s\n",
		    bpkg, strerror(rv));
		goto out;
	}
	/*
	 * Set package state to unpacked.
	 */
	rv = xbps_set_pkg_state_installed(pkgname, version, pkgver,
	    XBPS_PKG_STATE_UNPACKED);
	if (rv != 0) {
		xbps_error_printf("failed to set `%s-%s' to unpacked "
		    "state: %s\n", pkgname, version, strerror(rv));
	}
out:
	if (bpkg)
		free(bpkg);
	if (ar)
		archive_read_finish(ar);

	return rv;
}
