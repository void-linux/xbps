/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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

static int unpack_archive_fini(struct archive *, prop_dictionary_t, bool);
static void set_extract_flags(int *);

int SYMEXPORT
xbps_unpack_binary_pkg(prop_dictionary_t pkg, bool essential)
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

	if ((rv = unpack_archive_fini(ar, pkg, essential)) == 0) {
		/*
		 * If installation of package was successful, make sure
		 * its files are written in storage (if possible).
		 */
		if (fsync(pkg_fd) == -1) {
			rv = errno;
			goto out;
		}
		/*
		 * Set package state to unpacked.
		 */
		rv = xbps_set_pkg_state_installed(pkgname,
		    XBPS_PKG_STATE_UNPACKED);
	}

out:
	if (ar)
		archive_read_finish(ar);
	if (pkg_fd != -1)
		(void)close(pkg_fd);
	if (binfile)
		free(binfile);

	return rv;
}

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
unpack_archive_fini(struct archive *ar, prop_dictionary_t pkg,
		    bool essential)
{
	prop_dictionary_t filesd = NULL, old_filesd = NULL;
	struct archive_entry *entry;
	const char *pkgname, *version, *rootdir, *entry_str;
	char *buf, *buf2;
	int rv = 0, flags, lflags;
	bool actgt = false, skip_entry = false;

	assert(ar != NULL);
	assert(pkg != NULL);
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

	while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		entry_str = archive_entry_pathname(entry);
		set_extract_flags(&lflags);
		if (((strcmp("./INSTALL", entry_str)) == 0) ||
		    ((strcmp("./REMOVE", entry_str)) == 0) ||
		    ((strcmp("./files.plist", entry_str)) == 0) ||
		    ((strcmp("./props.plist", entry_str)) == 0) || essential) {
			/*
			 * Always overwrite files in essential packages,
			 * and metadata files.
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

			if (archive_read_extract(ar, entry, lflags) != 0) {
				if ((rv = archive_errno(ar)) != EEXIST) {
					free(buf);
					return rv;
				}
			}

			if ((rv = xbps_file_chdir_exec(rootdir, buf, "pre",
			     pkgname, version, NULL)) != 0) {
				free(buf);
				printf("%s: preinst action target error %s\n",
				    pkgname, strerror(errno));
				return rv;
			}
			/* pass to the next entry if successful */
			free(buf);
			continue;

		/*
		 * Unpack metadata files in final directory.
		 */
		} else if (strcmp("./REMOVE", entry_str) == 0) {
			buf2 = xbps_xasprintf(".%s/metadata/%s/REMOVE",
			    XBPS_META_PATH, pkgname);
			if (buf2 == NULL)
				return errno;
			archive_entry_set_pathname(entry, buf2);
			free(buf2);
			buf2 = NULL;
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
			continue;

		} else if (strcmp("./props.plist", entry_str) == 0) {
			buf2 = xbps_xasprintf(".%s/metadata/%s/props.plist",
			    XBPS_META_PATH, pkgname);
			if (buf2 == NULL)
				return errno;
			archive_entry_set_pathname(entry, buf2);
			free(buf2);
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
				continue;
			}
		}

		/*
		 * Extract entry from archive.
		 */
		if (archive_read_extract(ar, entry, lflags) != 0) {
			rv = archive_errno(ar);
			if (rv != EEXIST) {
				printf("ERROR: %s...exiting!\n",
				    archive_error_string(ar));
				return rv;;
			} else if (rv == EEXIST) {
				if (flags & XBPS_FLAG_VERBOSE) {
					printf("WARNING: ignoring existent "
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
		buf2 = xbps_xasprintf(".%s/metadata/%s/files.plist",
		    XBPS_META_PATH, pkgname);
		if (buf2 == NULL) {
			prop_object_release(filesd);
			return errno;
		}
		/*
		 * Check if files.plist exists and pkg is marked as
		 * essential, in that case we need to check for obsolete
		 * files and remove them if necessary.
		 */
		if (essential && (access(buf2, R_OK) == 0)) {
			old_filesd =
			    prop_dictionary_internalize_from_file(buf2);
			if (old_filesd == NULL) {
				prop_object_release(filesd);
				free(buf2);
				return errno;
			}
			rv = xbps_remove_obsoletes(old_filesd, filesd);
			if (rv != 0) {
				prop_object_release(old_filesd);
				prop_object_release(filesd);
				free(buf2);
				return rv;
			}
			prop_object_release(old_filesd);
		}
		/*
		 * Now that all files were successfully unpacked, we
		 * can safely externalize files.plist because the path
		 * is reachable.
		 */
		if (!prop_dictionary_externalize_to_file(filesd, buf2)) {
			prop_object_release(filesd);
			free(buf2);
			return errno;
		}
		free(buf2);
	}
	prop_object_release(filesd);

	return rv;
}
