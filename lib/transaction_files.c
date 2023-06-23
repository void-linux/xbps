/*-
 * Copyright (c) 2019 Juan Romero Pardines.
 * Copyright (c) 2019 Duncan Overbruck <mail@duncano.de>.
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

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "xbps_api_impl.h"
#include "uthash.h"

enum type {
	TYPE_LINK = 1,
	TYPE_DIR,
	TYPE_FILE,
	TYPE_CONFFILE,
};

struct item {
	char *file;
	size_t len;
	struct {
		const char *pkgname;
		const char *pkgver;
		char *sha256;
		const char *target;
		uint64_t size;
		enum type type;
		/* index is the index of the package update/install/removal in the transaction
		 * and is used to decide which package should remove the given file or dir */
		unsigned int index;
		bool preserve;
		bool update;
		bool removepkg;
	} old, new;
	bool deleted;
	UT_hash_handle hh;
};

/* hash table to look up files by path */
static struct item *hashtab = NULL;

/* list of files to be sorted using qsort */
static struct item **items = NULL;
static size_t itemsidx = 0;
static size_t itemssz = 0;

static struct item *
lookupItem(const char *file)
{
	struct item *item = NULL;

	assert(file);

	HASH_FIND_STR(hashtab, file, item);
	return item;
}

static struct item *
addItem(const char *file)
{
	struct item *item = calloc(1, sizeof (struct item));
	if (item == NULL)
		return NULL;

	assert(file);
	assert(item);

	if (itemsidx+1 >= itemssz) {
		itemssz = itemssz ? itemssz*2 : 64;
		items = realloc(items, itemssz*sizeof (struct item *));
		if (items == NULL) {
			free(item);
			return NULL;
		}
	}
	items[itemsidx++] = item;

	if ((item->file = xbps_xasprintf(".%s", file)) == NULL) {
		free(item);
		return NULL;
	}
	item->len = strlen(item->file);

	/*
	 * File paths are stored relative, but looked up absolute.
	 * Skip the leading . (dot) and substract it from the length.
	 */
	HASH_ADD_KEYPTR(hh, hashtab, item->file+1, item->len-1, item);

	return item;
}

static const char *
typestr(enum type typ)
{
	switch (typ) {
	case TYPE_LINK:     return "symlink";
	case TYPE_DIR:      return "directory";
	case TYPE_FILE:     return "file";
	case TYPE_CONFFILE: return "configuration file";
	default:            return NULL;
	}
}

static bool
match_preserved_file(struct xbps_handle *xhp, const char *file)
{
	if (xhp->preserved_files == NULL)
		return false;

	assert(file && *file == '.');
	return xbps_match_string_in_array(xhp->preserved_files, file+1);
}

static bool
can_delete_directory(const char *file, size_t len, size_t max)
{
	struct item *item;
	size_t rmcount = 0, fcount = 0;
	DIR *dp;

	dp = opendir(file);
	if (dp == NULL) {
		if (errno == ENOENT) {
			return true;
		} else {
			xbps_dbg_printf("[files] %s: %s: %s\n",
			    __func__, file, strerror(errno));
			return false;
		}
	}

	/*
	 * 1. Check if there is tracked directory content,
	 *    which can't be deleted.
	 * 2. Count deletable directory content.
	 */
	for (size_t i = 0; i < max; i++) {
		item = items[i];
		if (strncmp(item->file, file, len) == 0) {
			if (!item->deleted) {
				closedir(dp);
				return false;
			}
			rmcount++;
		}
	}

	/*
	 * Check if directory contains more files than we can
	 * delete.
	 */
	while (readdir(dp) != 0)
		fcount++;

	/* ignore '.' and '..' */
	fcount -= 2;

	if (fcount <= rmcount) {
		xbps_dbg_printf("[files] only removed %zu out of %zu files: %s\n",
		    rmcount, fcount, file);
	}
	closedir(dp);

	return fcount <= rmcount;
}

static int
collect_obsoletes(struct xbps_handle *xhp)
{
	/* These are symlinks in Void and must not be removed */
	const char *basesymlinks[] = {
		"/bin",
		"/sbin",
		"/usr/sbin",
		"/lib",
		"/lib32",
		"/lib64",
		"/usr/lib32",
		"/usr/lib64",
		"/var/run",
	};
	xbps_dictionary_t obsd;
	struct item *item;
	int rv = 0;

	if (xhp->transd == NULL)
		return ENOTSUP;

	if (!xbps_dictionary_get_dict(xhp->transd, "obsolete_files", &obsd))
		return ENOENT;

	/*
	 * Iterate over all files, longest paths first,
	 * to check if directory contents of removed
	 * directories can be deleted.
	 *
	 * - Check if a file is obsolete
	 * - Check if obsolete file can be deleted.
	 * - Check if directory needs and can be deleted.
	 */
	for (size_t i = 0; i < itemsidx; i++) {
		xbps_array_t a;
		const char *pkgname;
		bool alloc = false, found = false;

		item = items[i];

		if (match_preserved_file(xhp, item->file)) {
			xbps_dbg_printf("[obsoletes] %s: file exists on disk"
			    " and must be preserved: %s\n", item->old.pkgver, item->file);
			continue;
		}

		if (item->new.type == 0) {
			/*
			 * File was removed and is not provided by any
			 * new package.
			 * Probably obsolete.
			 */
			if (item->old.preserve && item->old.update) {
				xbps_dbg_printf("[files] %s: skipping `preserve` %s: %s\n",
				    item->old.pkgver, typestr(item->old.type), item->file);
				continue;
			}
		} else if (item->new.type == TYPE_CONFFILE) {
			/*
			 * Ignore conf files.
			 */
			continue;
		} else if (item->old.type == 0) {
			/* XXX: add this new behaviour? */
#if 0
			/*
			 * Check if new file (untracked until now) exists.
			 */
			if (access(item->file, F_OK) == 0) {
				xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
				    EEXIST, item->new.pkgver,
				    "%s: file `%s' already exists.",
				    item->new.pkgver, item->file);
				rv = EEXIST;
				break;
			}
#endif
			continue;
		} else if (item->old.type == TYPE_DIR &&
		    item->new.type != TYPE_DIR && item->new.type != 0) {
			/*
			 * Directory replaced by a file or symlink.
			 * We MUST be able to delete the directory.
			 */
			xbps_dbg_printf("[files] %s: directory changed to %s: %s\n",
			    item->new.pkgver, typestr(item->new.type), item->file);
			if (!can_delete_directory(item->file, item->len, i)) {
				xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
				    ENOTEMPTY, item->old.pkgver,
				    "%s: directory `%s' can not be deleted.",
				    item->old.pkgver, item->file);
				return ENOTEMPTY;
			}
		} else if (item->new.type != item->old.type) {
			/*
			 * File type changed, we have to delete it.
			 */
		} else {
			continue;
		}

		/*
		 * Make sure to not remove any symlink of root directory.
		 */
		for (uint8_t x = 0; x < __arraycount(basesymlinks); x++) {
			if (strcmp(item->file+1, basesymlinks[x]) == 0) {
				found = true;
				break;
			}
		}
		if (found)
			continue;

		/*
		 * Skip unexisting files and keep files with hash mismatch.
		 */
		if (item->old.sha256 != NULL) {
			rv = xbps_file_sha256_check(item->file, item->old.sha256);
			switch (rv) {
			case 0:
				/* hash matches, we can safely delete and/or overwrite it */
				break;
			case ENOENT:
				/* mark unexisting files as deleted and ignore ENOENT */
				rv = 0;
				item->deleted = true;
				continue;
			case ERANGE:
				/* hash mismatch don't delete it */
				rv = 0;
				/*
				 * If the file is removed by uninstalling the package,
				 * no new package provides it and its not force removed,
				 * keep the file.
				 */
				if (item->old.removepkg && !item->new.pkgname &&
				    (xhp->flags & XBPS_FLAG_FORCE_REMOVE_FILES) != 0) {
					xbps_dbg_printf("[obsoletes] %s: SHA256 mismatch,"
					    " force remove %s: %s\n",
						item->old.pkgname, typestr(item->old.type),
					    item->file+1);
					break;
				}
				xbps_dbg_printf("[obsoletes] %s: SHA256 mismatch,"
				    " skipping remove %s: %s\n",
				    item->old.pkgname, typestr(item->old.type),
				    item->file+1);
				continue;
			default:
				break;
			}
		}

		/*
		 * On package removal without force, keep symlinks if target changed.
		 */
		if (item->old.pkgname && item->old.removepkg &&
		    item->old.type == TYPE_LINK && !item->new.pkgname &&
		    (xhp->flags & XBPS_FLAG_FORCE_REMOVE_FILES) == 0) {
			char path[PATH_MAX], *lnk;
			const char *file = item->file+1;
			if (strcmp(xhp->rootdir, "/") != 0) {
				snprintf(path, sizeof(path), "%s%s",
				    xhp->rootdir, item->file+1);
				file = path;
			}
			lnk = xbps_symlink_target(xhp, file, item->old.target);
			if (lnk == NULL) {
				xbps_dbg_printf("[obsoletes] %s "
				    "symlink_target: %s\n", item->file+1, strerror(errno));
				continue;
			}
			if (strcmp(lnk, item->old.target) != 0) {
				xbps_dbg_printf("[obsoletes] %s: skipping modified"
				    " symlink (stored `%s' current `%s'): %s\n",
				    item->old.pkgname, item->old.target, lnk, item->file+1);
				free(lnk);
				continue;
			}
			free(lnk);
		}

		/*
		 * Choose which package removes the obsolete files,
		 * based which packages is installed/unpacked first.
		 * This is necessary to not delete files
		 * after it was installed by another package.
		 */
		if (item->old.pkgname && item->new.pkgname) {
			pkgname = item->old.index > item->new.index ?
				item->new.pkgname : item->old.pkgname;
		} else if (item->old.pkgname) {
			pkgname = item->old.pkgname;
		} else {
			pkgname = item->new.pkgname;
		}
		assert(pkgname);

		xbps_dbg_printf("[obsoletes] %s: removes %s: %s\n",
		    pkgname, typestr(item->old.type), item->file+1);

		/*
		 * Mark file as being deleted, this is used when
		 * checking if a directory can be deleted.
		 */
		item->deleted = true;

		/*
		 * Add file to the packages `obsolete_files` dict
		 */
		if ((a = xbps_dictionary_get(obsd, pkgname)) == NULL) {
			if (!(a = xbps_array_create()) ||
				!(xbps_dictionary_set(obsd, pkgname, a)))
				return ENOMEM;
			alloc = true;
		}
		if (!xbps_array_add_cstring(a, item->file)) {
			if (alloc)
				xbps_object_release(a);
			return ENOMEM;
		}
		if (alloc)
			xbps_object_release(a);
	}

	return rv;
}

static int
collect_file(struct xbps_handle *xhp, const char *file, size_t size,
		const char *pkgname, const char *pkgver, unsigned int idx,
		const char *sha256, enum type type, bool update, bool removepkg,
		bool preserve, bool removefile, const char *target)
{
	struct item *item;

	assert(file);

	if ((item = lookupItem(file)) == NULL) {
		item = addItem(file);
		if (item == NULL)
			return ENOMEM;
		item->deleted = false;
		goto add;
	}

	if (removefile) {
		if (item->old.type == 0) {
			/*
			 * File wasn't removed before.
			 */
		} else if (type == TYPE_DIR && item->old.type == TYPE_DIR) {
			/*
			 * Multiple packages removing the same directory.
			 * Record the last package to remove this directory.
			 */
			if (idx < item->old.index || item->old.preserve)
				return 0;
			item->old.pkgname = pkgname;
			item->old.pkgver = pkgver;
			item->old.index = idx;
			item->old.preserve = preserve;
			item->old.update = update;
			item->old.removepkg = removepkg;
			return 0;
		} else {
			/*
			 * Multiple packages removing the same file.
			 * Shouldn't happen, but its not fatal.
			 */
			xbps_dbg_printf("[files] %s: file already removed"
			    " by package `%s': %s\n", pkgver, item->old.pkgver, file);

			/*
			 * Check if `preserve` is violated.
			 */
			if (item->old.preserve && !preserve) {
				xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
				    EPERM, item->old.pkgver,
				    "%s: preserved file `%s' removed by %s.",
				    item->old.pkgver, file, pkgver);
				return EPERM;
			} else if (preserve && !item->old.preserve) {
				xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
				    EPERM, pkgver,
				    "%s: preserved file `%s' removed by %s.",
				    pkgver, file, item->old.pkgver);
				return EPERM;
			}
			return 0;
		}
		goto add;
	} else {
		/*
		 * Multiple packages creating the same directory.
		 */
		if (item->new.type == 0) {
			/*
			 * File wasn't created before.
			 */
		} else if (type == TYPE_DIR && item->new.type == TYPE_DIR) {
			/*
			 * Multiple packages creating the same directory.
			 */
			return 0;
		} else {
			/*
			 * Multiple packages creating the same file.
			 * This should never happen in a transaction.
			 */
			xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
			    EEXIST, pkgver,
			    "%s: file `%s' already installed by package %s.",
			    pkgver, file, item->new.pkgver);
			if (xhp->flags & XBPS_FLAG_IGNORE_FILE_CONFLICTS)
				return 0;

			return EEXIST;
		}
		goto add;
	}

	return 0;
add:
	if (removefile) {
		item->old.pkgname = pkgname;
		item->old.pkgver = pkgver;
		item->old.type = type;
		item->old.size = size;
		item->old.index = idx;
		item->old.preserve = preserve;
		item->old.update = update;
		item->old.removepkg = removepkg;
		item->old.target = target;
		if (sha256)
			item->old.sha256 = strdup(sha256);
	} else {
		item->new.pkgname = pkgname;
		item->new.pkgver = pkgver;
		item->new.type = type;
		item->new.size = size;
		item->new.index = idx;
		item->new.preserve = preserve;
		item->new.update = update;
		item->new.removepkg = removepkg;
		item->new.target = target;
	}
	if (item->old.type && item->new.type) {
		/*
		 * The file was removed by one package
		 * and installed by another package.
		 */
		if (strcmp(item->new.pkgname, item->old.pkgname) != 0) {
			if (removefile) {
				xbps_dbg_printf("[files] %s: %s moved to"
				    " package `%s': %s\n", pkgver, typestr(item->old.type),
				    item->new.pkgver, file);
			} else {
				xbps_dbg_printf("[files] %s: %s moved from"
				    " package `%s': %s\n", pkgver, typestr(item->new.type),
				    item->old.pkgver, file);
			}
		}
	}

	return 0;
}

static int
collect_files(struct xbps_handle *xhp, xbps_dictionary_t d,
			const char *pkgname, const char *pkgver, unsigned int idx,
			bool update, bool removepkg, bool preserve, bool removefile)
{
	xbps_array_t a;
	xbps_dictionary_t filed;
	uint64_t size;
	unsigned int i;
	int rv = 0;
	const char *file, *sha256 = NULL;
	bool error = false;

	if ((a = xbps_dictionary_get(d, "files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			if (removefile)
				xbps_dictionary_get_cstring_nocopy(filed, "sha256", &sha256);
			size = 0;
			xbps_dictionary_get_uint64(filed, "size", &size);
			rv = collect_file(xhp, file, size, pkgname, pkgver, idx, sha256,
			    TYPE_FILE, update, removepkg, preserve, removefile, NULL);
			if (rv == EEXIST) {
				error = true;
				continue;
			} else if (rv != 0) {
				goto out;
			}
		}
	}
	if ((a = xbps_dictionary_get(d, "conf_files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			size = 0;
			xbps_dictionary_get_uint64(filed, "size", &size);
			if (removefile)
				xbps_dictionary_get_cstring_nocopy(filed, "sha256", &sha256);
#if 0
			/* XXX: how to handle conf_file size */
			if (removefile && stat(file, &st) != -1 && size != (uint64_t)st.st_size)
				size = 0;
#endif
			rv = collect_file(xhp, file, size, pkgname, pkgver, idx, sha256,
			    TYPE_CONFFILE, update, removepkg, preserve, removefile, NULL);
			if (rv == EEXIST) {
				error = true;
				continue;
			} else if (rv != 0) {
				goto out;
			}
		}
	}
	if ((a = xbps_dictionary_get(d, "links"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			const char *target = NULL;
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			xbps_dictionary_get_cstring_nocopy(filed, "target", &target);
			assert(target);
			rv = collect_file(xhp, file, 0, pkgname, pkgver, idx, NULL,
			    TYPE_LINK, update, removepkg, preserve, removefile, target);
			if (rv == EEXIST) {
				error = true;
				continue;
			} else if (rv != 0) {
				goto out;
			}
		}
	}
	if ((a = xbps_dictionary_get(d, "dirs"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			rv = collect_file(xhp, file, 0, pkgname, pkgver, idx, NULL,
			    TYPE_DIR, update, removepkg, preserve, removefile, NULL);
			if (rv == EEXIST) {
				error = true;
				continue;
			} else if (rv != 0) {
				goto out;
			}
		}
	}

out:
	if (error)
		rv = EEXIST;

	return rv;
}

static int
collect_binpkg_files(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod,
		unsigned int idx, bool update)
{
	xbps_dictionary_t filesd;
	struct archive *ar = NULL;
	struct archive_entry *entry;
	struct stat st;
	const char *pkgver, *pkgname;
	char *bpkg;
	/* size_t entry_size; */
	int rv = 0, pkg_fd = -1;

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	assert(pkgver);
	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	assert(pkgname);

	bpkg = xbps_repository_pkg_path(xhp, pkg_repod);
	if (bpkg == NULL) {
		rv = errno;
		goto out;
	}

	if ((ar = archive_read_new()) == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Enable support for tar format and gzip/bzip2/lzma compression methods.
	 */
	archive_read_support_filter_gzip(ar);
	archive_read_support_filter_bzip2(ar);
	archive_read_support_filter_xz(ar);
	archive_read_support_filter_lz4(ar);
	archive_read_support_filter_zstd(ar);
	archive_read_support_format_tar(ar);

	pkg_fd = open(bpkg, O_RDONLY|O_CLOEXEC);
	if (pkg_fd == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: failed to open binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (fstat(pkg_fd, &st) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: failed to fstat binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (archive_read_open_fd(ar, pkg_fd, st.st_blksize) == ARCHIVE_FATAL) {
		rv = archive_errno(ar);
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: failed to read binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}

	for (uint8_t i = 0; i < 4; i++) {
		const char *entry_pname;
		int ar_rv = archive_read_next_header(ar, &entry);
		if (ar_rv == ARCHIVE_EOF || ar_rv == ARCHIVE_FATAL)
			break;
		else if (ar_rv == ARCHIVE_RETRY)
			continue;

		entry_pname = archive_entry_pathname(entry);
		if ((strcmp("./files.plist", entry_pname)) == 0) {
			filesd = xbps_archive_get_dictionary(ar, entry);
			if (filesd == NULL) {
				rv = EINVAL;
				goto out;
			}
			rv = collect_files(xhp, filesd, pkgname, pkgver, idx,
			    update, false, false, false);
			xbps_object_release(filesd);
			goto out;
		}
		archive_read_data_skip(ar);
	}

out:
	if (pkg_fd != -1)
		close(pkg_fd);
	if (ar != NULL)
		archive_read_free(ar);
	free(bpkg);
	return rv;
}

static int
pathcmp(const void *l1, const void *l2)
{
	const struct item *a = *(const struct item * const*)l1;
	const struct item *b = *(const struct item * const*)l2;
	return (a->len < b->len) - (b->len < a->len);
}

static void
cleanup(void)
{
	struct item *item, *itmp;

	HASH_ITER(hh, hashtab, item, itmp) {
		HASH_DEL(hashtab, item);
		free(item->file);
		free(item->old.sha256);
		free(item->new.sha256);
		free(item);
	}
	free(items);
}

/*
 * xbps_transaction_files:
 *
 * - read files from each installed package in the transaction
 * - read files from each binary package in the transaction
 *
 * - Find file conflicts between packages before starting the transaction
 *
 * - Schedule the removal of files
 *   - unlink files before extracting the package if the file type changed,
 *     a symlink becomes a directory or a directory becomes a regular file
 *     or symlink.
 *   - directories replaced with other file types are checked to be empty
 *     to avoid ENOTEMPTY while unpacking packages.
 *   - the last package removing a file out of a directory
 *     will try to remove that directory to avoid ENOTEMPTY
 *   - the removal of obsolete files and directory is sorted by
 *     path length so that directory content is removed before
 *     removing the directory.
 */
int HIDDEN
xbps_transaction_files(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_dictionary_t pkgd, filesd;
	xbps_object_t obj;
	xbps_trans_type_t ttype;
	const char *pkgver, *pkgname;
	int rv = 0;
	unsigned int idx = 0;

	assert(xhp);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		bool update = false;

		/* increment the index of the given package package in the transaction */
		idx++;

		/* ignore pkgs in hold mode or in unpacked state */
		ttype = xbps_transaction_pkg_type(obj);
		if (ttype == XBPS_TRANS_HOLD || ttype == XBPS_TRANS_CONFIGURE) {
			continue;
		}

		if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver)) {
			return EINVAL;
		}
		if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname)) {
			return EINVAL;
		}

		update = (ttype == XBPS_TRANS_UPDATE);

		if (ttype == XBPS_TRANS_INSTALL || ttype == XBPS_TRANS_REINSTALL || ttype == XBPS_TRANS_UPDATE) {
			xbps_set_cb_state(xhp, XBPS_STATE_FILES, 0, pkgver,
			    "%s: collecting files...", pkgver);
			rv = collect_binpkg_files(xhp, obj, idx, update);
			if (rv != 0)
				goto out;
		}

		/*
		 * Always just try to get the package from the pkgdb:
		 * update and remove always have a previous package,
		 * `hold` and `configure` are skipped.
		 * And finally the reason to do is, `install` could be
		 * a reinstallation, in which case the files list could
		 * different between old and new "install".
		 */
		pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
		if (pkgd) {
			const char *oldpkgver;
			bool preserve = false;
			bool removepkg = (ttype == XBPS_TRANS_REMOVE);

			xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &oldpkgver);
			if (!xbps_dictionary_get_bool(obj, "preserve", &preserve))
				preserve = false;

			filesd = xbps_pkgdb_get_pkg_files(xhp, pkgname);
			if (filesd == NULL) {
				continue;
			}

			assert(oldpkgver);
			xbps_set_cb_state(xhp, XBPS_STATE_FILES, 0, oldpkgver,
			    "%s: collecting files...", oldpkgver);
			rv = collect_files(xhp, filesd, pkgname, pkgver, idx,
			    update, removepkg, preserve, true);
			if (rv != 0)
				goto out;
		}
	}
	xbps_object_iterator_reset(iter);

	/*
	 * Sort items by path length, to make it easier to find files in
	 * directories.
	 */
	qsort(items, itemsidx, sizeof (struct item *), pathcmp);

	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL, rv, xhp->rootdir,
		    "failed to chdir to rootdir `%s': %s",
		    xhp->rootdir, strerror(errno));
	}

out:
	if (rv != 0)
		return rv;

	rv = collect_obsoletes(xhp);
	cleanup();
	return rv;
}
