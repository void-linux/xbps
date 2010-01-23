/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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
#include <fcntl.h>

#include <xbps_api.h>

/**
 * @file lib/unpack.c
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
 *  - If it's an <b>essential</b> package, files from installed package are
 *    compared with new package and obsolete files are removed.
 *  - Finally its state is set to XBPS_PKG_STATE_UNPACKED.
 *
 * The following image shown below represents a transaction dictionary
 * returned by xbps_repository_get_transaction_dict():
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
set_extract_flags(int *flags)
{
	*flags = 0;
	if (getuid() == 0)
		*flags = FEXTRACT_FLAGS;
	else
		*flags = EXTRACT_FLAGS;
}

/*
 * TODO: remove printfs and return appropiate errors to be interpreted by
 * the consumer.
 */
static int
unpack_archive_fini(struct archive *ar, prop_dictionary_t pkg)
{
	prop_dictionary_t filesd = NULL, old_filesd = NULL;
	struct archive_entry *entry;
	size_t entry_idx = 0;
	const char *pkgname, *version, *rootdir, *entry_str, *transact;
	char *buf;
	int rv = 0, flags, lflags;
	bool essential, preserve, actgt, skip_entry, update;
	bool props_plist_found, files_plist_found;

	assert(ar != NULL);
	assert(pkg != NULL);

	essential = preserve = actgt = skip_entry = update = false;
	props_plist_found = files_plist_found = false;
	rootdir = xbps_get_rootdir();
	flags = xbps_get_flags();

	if (strcmp(rootdir, "") == 0)
		rootdir = "/";

	if (chdir(rootdir) == -1)
		return errno;

	if (!prop_dictionary_get_cstring_nocopy(pkg, "pkgname", &pkgname))
		return errno;
	if (!prop_dictionary_get_cstring_nocopy(pkg, "version", &version))
		return errno;
	/*
	 * The following two objects are OPTIONAL.
	 */
	prop_dictionary_get_bool(pkg, "essential", &essential);
	prop_dictionary_get_bool(pkg, "preserve", &preserve);
	
	if (!prop_dictionary_get_cstring_nocopy(pkg, "trans-action",
	    &transact))
		return errno;
	if (strcmp(transact, "update") == 0)
		update = true;

	/*
	 * While updating, always remove current INSTALL/REMOVE
	 * scripts, because a package upgrade might not have those
	 * anymore.
	 */
	if (update) {
		buf = xbps_xasprintf(".%s/metadata/%s/INSTALL",
		    XBPS_META_PATH, pkgname);
		if (buf == NULL)
			return errno;
		if (access(buf, R_OK|X_OK) == 0) {
			if (unlink(buf) == -1) {
				free(buf);
				return errno;
			}
		}
		free(buf);
		buf = xbps_xasprintf(".%s/metadata/%s/REMOVE",
		    XBPS_META_PATH, pkgname);
		if (buf == NULL)
			return errno;
		if (access(buf, R_OK|X_OK) == 0) {
			if (unlink(buf) == -1) {
				free(buf);
				return errno;
			}
		}
		free(buf);
	}

	/*
	 * Process the archive files.
	 */
	while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		entry_str = archive_entry_pathname(entry);
		set_extract_flags(&lflags);
		/*
		 * Now check what currenty entry in the archive contains.
		 */
		if (((strcmp("./files.plist", entry_str)) == 0) ||
		    ((strcmp("./props.plist", entry_str)) == 0) || essential) {
			/*
			 * Always overwrite files in essential packages,
			 * and plist metadata files.
			 */
			lflags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE;
			lflags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
		}

		/*
		 * Run the pre INSTALL action if the file is there.
		 */
		if (strcmp("./INSTALL", entry_str) == 0) {
			buf = xbps_xasprintf(".%s/metadata/%s/INSTALL",
			    XBPS_META_PATH, pkgname);
			if (buf == NULL)
				return errno;

			actgt = true;
			archive_entry_set_pathname(entry, buf);
			archive_entry_set_mode(entry, 0750);

			if (archive_read_extract(ar, entry, lflags) != 0) {
				if ((rv = archive_errno(ar)) != EEXIST) {
					free(buf);
					return rv;
				}
			}
			
			if ((rv = xbps_file_chdir_exec(rootdir, buf, "pre",
			     pkgname, version, update ? "yes" : "no",
			     NULL)) != 0) {
				free(buf);
				fprintf(stderr,
				    "%s: preinst action target error %s\n",
				    pkgname, strerror(errno));
				return rv;
			}
			/* Pass to the next entry if successful */
			free(buf);
			entry_idx++;
			continue;

		/*
		 * Unpack metadata files in final directory.
		 */
		} else if (strcmp("./REMOVE", entry_str) == 0) {
			buf = xbps_xasprintf(".%s/metadata/%s/REMOVE",
			    XBPS_META_PATH, pkgname);
			if (buf == NULL)
				return errno;
			archive_entry_set_pathname(entry, buf);
			free(buf);
			archive_entry_set_mode(entry, 0750);
			if (archive_read_extract(ar, entry, lflags) != 0)
				return archive_errno(ar);

			/* Pass to next entry if successful */
			entry_idx++;
			continue;

		} else if (strcmp("./files.plist", entry_str) == 0) {
			/*
			 * Now we have a dictionary from the entry
			 * in memory. Will be written to disk later, when
			 * all files are extracted.
			 */
			filesd = xbps_read_dict_from_archive_entry(ar, entry);
			if (filesd == NULL)
				return errno;

			/* Pass to next entry */
			files_plist_found = true;
			entry_idx++;
			continue;

		} else if (strcmp("./props.plist", entry_str) == 0) {
			buf = xbps_xasprintf(".%s/metadata/%s/props.plist",
			    XBPS_META_PATH, pkgname);
			if (buf == NULL)
				return errno;
			archive_entry_set_pathname(entry, buf);
			free(buf);

			if (archive_read_extract(ar, entry, lflags) != 0)
				return archive_errno(ar);

			/* Pass to next entry if successful */
			props_plist_found = true;
			entry_idx++;
			continue;

		} else {
			/*
			 * Handle configuration files.
			 */
			if ((rv = xbps_config_file_from_archive_entry(filesd,
			    entry, pkgname, &lflags, &skip_entry)) != 0) {
				prop_object_release(filesd);
				return rv;
			}
			if (skip_entry) {
				archive_read_data_skip(ar);
				skip_entry = false;
				entry_idx++;
				continue;
			}
		}

		/*
		 * If XBPS_PKGFILES or XBPS_PKGPROPS weren't found
		 * in the archive at this phase, skip all data.
		 */
		if (!files_plist_found || !props_plist_found) {
			archive_read_data_skip(ar);
			/*
			 * If we have processed 4 entries and the two
			 * required metadata files weren't found, bail out.
			 * This is not an XBPS binary package.
			 */
			if (entry_idx >= 3)
				return ENOPKG;

			entry_idx++;
			continue;
		}

		/*
		 * Extract entry from archive.
		 */
		if (archive_read_extract(ar, entry, lflags) != 0) {
			rv = archive_errno(ar);
			if (rv != EEXIST) {
				fprintf(stderr, "ERROR: %s...exiting!\n",
				    archive_error_string(ar));
				return rv;;
			} else if (rv == EEXIST) {
				if (flags & XBPS_FLAG_VERBOSE) {
					fprintf(stderr,
					    "WARNING: ignoring existent "
					    "path: %s\n",
					    archive_entry_pathname(entry));
				}
				rv = 0;
				continue;
			}
		}
		if (flags & XBPS_FLAG_VERBOSE)
			printf(" %s\n", archive_entry_pathname(entry));
	}

	if ((rv = archive_errno(ar)) == 0) {
		buf = xbps_xasprintf(".%s/metadata/%s/files.plist",
		    XBPS_META_PATH, pkgname);
		if (buf == NULL) {
			prop_object_release(filesd);
			return errno;
		}
		/*
		 * Check if files.plist exists and pkg is marked as
		 * essential and NOT preserve, in that case we need to check
		 * for obsolete files and remove them if necessary.
		 */
		if (!preserve && essential && (access(buf, R_OK) == 0)) {
			old_filesd =
			    prop_dictionary_internalize_from_file(buf);
			if (old_filesd == NULL) {
				prop_object_release(filesd);
				free(buf);
				return errno;
			}
			rv = xbps_remove_obsoletes(old_filesd, filesd);
			if (rv != 0) {
				prop_object_release(old_filesd);
				prop_object_release(filesd);
				free(buf);
				return rv;
			}
			prop_object_release(old_filesd);
		}
		/*
		 * Now that all files were successfully unpacked, we
		 * can safely externalize files.plist because the path
		 * is reachable.
		 */
		if (!prop_dictionary_externalize_to_file(filesd, buf)) {
			prop_object_release(filesd);
			free(buf);
			return errno;
		}
		free(buf);
	}
	if (filesd)
		prop_object_release(filesd);

	return rv;
}

int
xbps_unpack_binary_pkg(prop_dictionary_t pkg)
{
	const char *pkgname, *repoloc;
	struct archive *ar = NULL;
	char *binfile = NULL;
	int pkg_fd, rv = 0;

	assert(pkg != NULL);

	if (!prop_dictionary_get_cstring_nocopy(pkg, "pkgname", &pkgname))
		return errno;
	if (!prop_dictionary_get_cstring_nocopy(pkg, "repository", &repoloc))
		return errno;
	binfile = xbps_get_binpkg_local_path(pkg, repoloc);
	if (binfile == NULL)
		return EINVAL;

	if ((pkg_fd = open(binfile, O_RDONLY)) == -1) {
		rv = errno;
		goto out;
	}

	ar = archive_read_new();
	if (ar == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Enable support for tar format and all compression methods.
	 */
	archive_read_support_compression_all(ar);
	archive_read_support_format_tar(ar);

	if ((rv = archive_read_open_fd(ar, pkg_fd,
	     ARCHIVE_READ_BLOCKSIZE)) != 0)
		goto out;

	if ((rv = unpack_archive_fini(ar, pkg)) != 0)
		goto out;

	/*
	 * Set package state to unpacked.
	 */
	rv = xbps_set_pkg_state_installed(pkgname, XBPS_PKG_STATE_UNPACKED);

out:
	if (ar)
		archive_read_finish(ar);
	if (pkg_fd != -1)
		(void)close(pkg_fd);
	if (binfile)
		free(binfile);

	return rv;
}
