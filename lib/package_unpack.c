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
#include "xbps_api_impl.h"

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
 *  - Files from installed package are compared with new package and
 *    obsolete files are removed.
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

/*
 * TODO: remove printfs and return appropiate errors to be interpreted by
 * the consumer.
 */
static int
unpack_archive_fini(struct archive *ar,
		    prop_dictionary_t pkg,
		    const char *pkgname,
		    const char *version)
{
	prop_dictionary_t propsd, filesd, old_filesd;
	struct archive_entry *entry;
	size_t entry_idx = 0;
	const char *rootdir, *entry_str, *transact;
	char *buf;
	int rv, flags, lflags;
	bool preserve, skip_entry, update, replace_files_in_pkg_update;
	bool props_plist_found, files_plist_found;

	assert(ar != NULL);
	assert(pkg != NULL);

	preserve = skip_entry = update = replace_files_in_pkg_update = false;
	props_plist_found = files_plist_found = false;
	rootdir = xbps_get_rootdir();
	flags = xbps_get_flags();

	if (chdir(rootdir) == -1)
		return errno;

	prop_dictionary_get_bool(pkg, "preserve", &preserve);
	prop_dictionary_get_cstring_nocopy(pkg, "trans-action", &transact);
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
			return ENOMEM;

		if (unlink(buf) == -1) {
			if (errno && errno != ENOENT) {
				free(buf);
				return errno;
			}
		}
		free(buf);
		buf = xbps_xasprintf(".%s/metadata/%s/REMOVE",
		    XBPS_META_PATH, pkgname);
		if (buf == NULL)
			return ENOMEM;

		if (unlink(buf) == -1) {
			if (errno && errno != ENOENT) {
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
		set_extract_flags(&lflags, update);
		/*
		 * Run the pre INSTALL action if the file is there.
		 */
		if (strcmp("./INSTALL", entry_str) == 0) {
			buf = xbps_xasprintf(".%s/metadata/%s/INSTALL",
			    XBPS_META_PATH, pkgname);
			if (buf == NULL) {
				rv = ENOMEM;
				goto out;
			}

			archive_entry_set_pathname(entry, buf);
			archive_entry_set_perm(entry, 0750);

			if (archive_read_extract(ar, entry, lflags) != 0) {
				if ((rv = archive_errno(ar)) != EEXIST) {
					free(buf);
					goto out;
				}
			}
			
			rv = xbps_file_exec(buf, "pre",
			     pkgname, version, update ? "yes" : "no", NULL);
			if (rv != 0) {
				free(buf);
				fprintf(stderr,
				    "%s: preinst action target error %s\n",
				    pkgname, strerror(errno));
				goto out;
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
			if (buf == NULL) {
				rv = ENOMEM;
				goto out;
			}

			archive_entry_set_pathname(entry, buf);
			free(buf);
			archive_entry_set_perm(entry, 0750);
			if (archive_read_extract(ar, entry, lflags) != 0) {
				if ((rv = archive_errno(ar)) != EEXIST)
					goto out;
			}

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
			if (filesd == NULL) {
				rv = errno;
				goto out;
			}

			/* Pass to next entry */
			files_plist_found = true;
			entry_idx++;
			continue;

		} else if (strcmp("./props.plist", entry_str) == 0) {
			buf = xbps_xasprintf(".%s/metadata/%s/%s",
			    XBPS_META_PATH, pkgname, XBPS_PKGPROPS);
			if (buf == NULL) {
				rv = ENOMEM;
				goto out;
			}

			archive_entry_set_pathname(entry, buf);
			free(buf);

			if (archive_read_extract(ar, entry, lflags) != 0) {
				rv = archive_errno(ar);
				goto out;
			}

			propsd =
			    xbps_get_pkg_dict_from_metadata_plist(pkgname,
			    XBPS_PKGPROPS);
			if (propsd == NULL) {
				rv = errno;
				goto out;
			}
			/* Pass to next entry if successful */
			props_plist_found = true;
			entry_idx++;
			continue;
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
				return ENODEV;

			entry_idx++;
			continue;
		}

		/*
		 * Handle configuration files. Check if current entry is
		 * a configuration file and take action if required. Skip
		 * packages that don't have the "conf_files" array in
		 * the XBPS_PKGPROPS dictionary.
		 */
		if (prop_dictionary_get(propsd, "conf_files")) {
			if ((rv = xbps_config_file_from_archive_entry(filesd,
			    propsd, entry, &lflags, &skip_entry)) != 0)
				goto out;

			if (skip_entry) {
				archive_read_data_skip(ar);
				skip_entry = false;
				continue;
			}
		}

		/*
		 * Account for the following scenario (real example):
		 *
		 * 	- gtk+-2.20 is currently installed.
		 * 	- gtk+-2.20 contains libgdk_pixbuf.so.
		 * 	- gtk+-2.20 will be updated to 2.22 in the transaction.
		 * 	- gtk+-2.22 depends on gdk-pixbuf>=2.22.
		 * 	- gdk-pixbuf-2.22 contains libgdk_pixbuf.so.
		 * 	- gdk-pixbuf-2.22 will be installed in the transaction.
		 *
		 * We do the following to fix this:
		 *
		 * 	- gdk-pixbuf-2.22 installs its files overwritting
		 * 	  current ones if they exist.
		 * 	- gtk+ is updated to 2.22, it checks for obsolete files
		 * 	  and detects that the files that were owned in 2.20
		 * 	  don't match the SHA256 hash and skips them.
		 */
		replace_files_in_pkg_update = false;
		prop_dictionary_get_bool(pkg, "replace-files-in-pkg-update",
		    &replace_files_in_pkg_update);
		if (replace_files_in_pkg_update) {
			lflags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE;
			lflags &= ~ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER;
		}

		/*
		 * Extract entry from archive.
		 */
		if (archive_read_extract(ar, entry, lflags) != 0) {
			rv = archive_errno(ar);
			if (rv && rv != EEXIST) {
				fprintf(stderr, "ERROR: %s...exiting!\n",
				    archive_error_string(ar));
				goto out;
			} else if (rv == EEXIST) {
				if (flags & XBPS_FLAG_VERBOSE) {
					fprintf(stderr,
					    "WARNING: ignoring existent "
					    "path: %s\n",
					    archive_entry_pathname(entry));
				}
				continue;
			}
		}
		if (flags & XBPS_FLAG_VERBOSE)
			printf(" %s\n", archive_entry_pathname(entry));
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
			free(buf);
			rv = errno;
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

int
xbps_unpack_binary_pkg(prop_dictionary_t pkg)
{
	const char *pkgname, *version;
	struct archive *ar = NULL;
	char *binfile = NULL;
	int pkg_fd, rv = 0;

	assert(pkg != NULL);

	prop_dictionary_get_cstring_nocopy(pkg, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkg, "version", &version);

	binfile = xbps_get_binpkg_repo_uri(pkg);
	if (binfile == NULL)
		return EINVAL;

	if ((pkg_fd = open(binfile, O_RDONLY)) == -1) {
		rv = errno;
		xbps_dbg_printf("cannot open '%s' for unpacking %s\n",
		    binfile, strerror(errno));
		free(binfile);
		goto out;
	}
	free(binfile);

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

	if (archive_read_open_fd(ar, pkg_fd,
	     ARCHIVE_READ_BLOCKSIZE) != 0) {
		rv = errno;
		goto out;
	}

	if ((rv = unpack_archive_fini(ar, pkg, pkgname, version)) != 0)
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

	return rv;
}
