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
#include <stdlib.h>
#include <string.h>

#include "xbps_api_impl.h"

enum type {
	TYPE_LINK = 1,
	TYPE_DIR,
	TYPE_FILE,
	TYPE_CONFFILE,
};

struct item {
	struct item *hnext;
	const char *file;
	size_t len;
	struct {
		const char *pkgname;
		const char *pkgver;
		const char *sha256;
		uint64_t size;
		enum type type;
		unsigned int index;
	} old, new;
	bool deleted;
};

#define ITHSIZE	1024
#define ITHMASK	(ITHSIZE - 1)

static struct item *ItemHash[ITHSIZE];

static struct item **items;
static size_t itemsidx = 0;
static size_t itemssz = 0;

static int
itemhash(const char *file)
{
	int hv = 0xA1B5F342;
	int i;

	assert(file);

	for (i = 0; file[i]; ++i)
		hv = (hv << 5) ^ (hv >> 23) ^ file[i];

	return hv & ITHMASK;
}

static struct item *
lookupItem(const char *file)
{
	struct item *item;

	assert(file);

	for (item = ItemHash[itemhash(file+1)]; item; item = item->hnext) {
		if (strcmp(file, item->file+1) == 0)
			return item;
	}
	return NULL;
}

static struct item *
addItem(const char *file)
{
	struct item **itemp;
	struct item *item = calloc(sizeof(*item), 1);
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

	itemp = &ItemHash[itemhash(file+1)];
	item->hnext = *itemp;
	if ((item->file = xbps_xasprintf(".%s", file)) == NULL) {
		free(item);
		return NULL;
	}
	item->len = strlen(file);
	assert(item->file);
	*itemp = item;

	return item;
}

static bool
can_delete_directory(struct xbps_handle *xhp, const char *file, size_t len, size_t max)
{
	struct item *item;
	size_t rmcount = 0, fcount = 0;
	DIR *dp;

	dp = opendir(file);
	if (dp == NULL) {
		xbps_dbg_printf(xhp, "[files] "
			"%s wtf: %s\n", file, strerror(errno));
		if (errno == ENOENT) {
			return true;
		} else {
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

	xbps_dbg_printf(xhp, "[files] "
		"transaction deletes %lu out of %lu files in: %s\n",
	   rmcount, fcount, file);

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

		if (item->new.type == 0 && item->old.type != TYPE_DIR) {
			/*
			 * File was removed and is not provided by any
			 * new package.
			 * Probably obsolete.
			 */
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
					rv, item->new.pkgver,
					"%s: [files] file `%s': %s",
					item->new.pkgver, item->file, strerror(EEXIST));
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
			xbps_dbg_printf(xhp, "[files] "
				"%s changed from directory to file\n", item->file);
			if (!can_delete_directory(xhp, item->file, item->len, i)) {
				xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
					rv, item->old.pkgver,
					"%s: [files] Directory `%s' can not be deleted",
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
				xbps_dbg_printf(xhp, "[obsoletes] ignoring "
					"%s removal\n", item->file);
				break;
			}
		}
		if (found)
			continue;

		/*
		 * Skip unexisting files and keep files with hash mismatch.
		 */
		if (item->old.sha256) {
			rv = xbps_file_hash_check(item->file, item->old.sha256);
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
				continue;
			default:
				break;
			}
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

		xbps_dbg_printf(xhp, "[obsoletes] "
			"%s\n", item->file);

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
		const char *sha256, enum type type, bool remove)
{
	struct item *item;
	int rv = 0;

	assert(file);

	if ((item = lookupItem(file)) == NULL) {
		item = addItem(file);
		if (item == NULL)
			return ENOMEM;
		item->deleted = false;
		goto add;
	}

	if (remove) {
		if (item->old.type == 0) {
			/*
			 * File wasn't removed before.
			 */
		} else if (type == TYPE_DIR && item->old.type == TYPE_DIR) {
			/*
			 * Multiple packages removing the same directory.
			 */
			return 0;
		} else {
			/*
			 * Multiple packages removing the same file.
			 * Shouldn't happen, but its not fatal.
			 */
			xbps_dbg_printf(xhp, "%s: [files] file `%s' already removed"
				"by `%s'\n", pkgname, file, item->old.pkgname);
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
				rv, pkgver,
				"%s: [files] file already installed by package `%s': %s",
				pkgver, item->new.pkgname, pkgname, file);
			return EEXIST;
		}
		goto add;
	}

	return 0;
add:
	if (remove) {
		item->old.pkgname = strdup(pkgname);
		item->old.pkgver = strdup(pkgver);
		item->old.type = type;
		item->old.size = size;
		item->old.index = idx;
		if (sha256)
			item->old.sha256 = strdup(sha256);
	} else {
		item->new.pkgname = strdup(pkgname);
		item->new.pkgver = strdup(pkgver);
		item->new.type = type;
		item->new.size = size;
		item->new.index = idx;
	}
	if (item->old.type && item->new.type) {
		/*
		 * The file was removed by one package
		 * and installed by another package.
		 */
		if (strcmp(item->new.pkgname, item->old.pkgname) != 0) {
			if (remove) {
				xbps_dbg_printf(xhp, "%s: [files] file `%s' moved to"
				    " package `%s'\n", pkgname, file, item->new.pkgname);
			} else {
				xbps_dbg_printf(xhp, "%s: [files] file `%s' moved from"
				    " package `%s'\n", pkgname, file, item->new.pkgname);
			}
		}
	}

	return 0;
}

static int
collect_files(struct xbps_handle *xhp, xbps_dictionary_t d,
			const char *pkgname, const char *pkgver, unsigned int idx,
			bool remove)
{
	xbps_array_t a;
	xbps_dictionary_t filed;
	uint64_t size;
	unsigned int i;
	int rv = 0;
	const char *file, *sha256 = NULL;

	if ((a = xbps_dictionary_get(d, "files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			if (remove)
				xbps_dictionary_get_cstring_nocopy(filed, "sha256", &sha256);
			size = 0;
			xbps_dictionary_get_uint64(filed, "size", &size);
			rv = collect_file(xhp, file, size, pkgname, pkgver, idx, sha256,
			    TYPE_FILE, remove);
			if (rv != 0)
				goto out;
		}
	}
	if ((a = xbps_dictionary_get(d, "conf_files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			size = 0;
			xbps_dictionary_get_uint64(filed, "size", &size);
			if (remove)
				xbps_dictionary_get_cstring_nocopy(filed, "sha256", &sha256);
#if 0
			/* XXX: how to handle conf_file size */
			if (remove && stat(file, &st) != -1 && size != (uint64_t)st.st_size)
				size = 0;
#endif
			rv = collect_file(xhp, file, size, pkgname, pkgver, idx, sha256,
			    TYPE_FILE, remove);
			if (rv != 0)
				goto out;
		}
	}
	if ((a = xbps_dictionary_get(d, "links"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			rv = collect_file(xhp, file, 0, pkgname, pkgver, idx, NULL,
			    TYPE_LINK, remove);
			if (rv != 0)
				goto out;
		}
	}
	if ((a = xbps_dictionary_get(d, "dirs"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			rv = collect_file(xhp, file, 0, pkgname, pkgver, idx, NULL,
			    TYPE_DIR, remove);
			if (rv != 0)
				goto out;
		}
	}

out:
	return rv;
}

static int
add_from_archive(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod,
		unsigned int idx)
{
	xbps_dictionary_t filesd;
	struct archive *ar = NULL;
	struct archive_entry *entry;
	struct stat st;
	const char *pkgver;
	char *bpkg, *pkgname;
	/* size_t entry_size; */
	int rv = 0, pkg_fd = -1;

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	assert(pkgver);

	pkgname = xbps_pkg_name(pkgver);
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
		    "%s: [files] failed to open binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (fstat(pkg_fd, &st) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: [files] failed to fstat binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (archive_read_open_fd(ar, pkg_fd, st.st_blksize) == ARCHIVE_FATAL) {
		rv = archive_errno(ar);
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: [files] failed to read binary package `%s': %s",
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
			rv = collect_files(xhp, filesd, pkgname, pkgver, idx, false);
			break;
		}
		archive_read_data_skip(ar);
	}

out:
	if (pkg_fd != -1)
		close(pkg_fd);
	if (ar)
		archive_read_finish(ar);
	free(bpkg);
	free(pkgname);
	return rv;
}

#if 0
bool HIDDEN
xbps_transaction_is_file_obsolete(struct xbps_handle *xhp, const char *file)
{
	struct item *item;
	/*
	 * If there is no transaction then consider the files obsolete.
	 * This only happens if `xbps_find_pkg_obsoletes` or this function
	 * is called without a transaction, e.g. in tests.
	 */
	if (!xhp->transd)
		return true;

	item = lookupItem(file);
	assert(item);

	/*
	 * The `file` is obsolete, if a package removed the `file`
	 * and no other package created `file`.
	 */
	return item->new.type == 0 && item->old.type != 0;
}
#endif

static int
pathcmp(const void *l1, const void *l2)
{
	const struct item *a = *(const struct item * const*)l1;
	const struct item *b = *(const struct item * const*)l2;
	return (a->len < b->len) - (b->len < a->len);
}

int HIDDEN
xbps_transaction_files(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_dictionary_t pkgd, filesd;
	xbps_object_t obj;
	const char *trans, *pkgver;
	int rv = 0;
	unsigned int idx = 0;

	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		char *pkgname;

		/*
		 * `idx` is used as package install index, to chose which
		 * choose the first package which owns or used to own the
		 * file deletes it.
		 */
		idx++;

		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		assert(trans);

		if ((strcmp(trans, "hold") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		assert(pkgver);
		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);

		xbps_set_cb_state(xhp, XBPS_STATE_FILES, 0, pkgver,
			"%s: collecting files...", pkgname);

		if ((strcmp(trans, "install") == 0) ||
		    (strcmp(trans, "update") == 0)) {
			rv = add_from_archive(xhp, obj, idx);
			if (rv != 0) {
				free(pkgname);
				break;
			}
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
			xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &oldpkgver);
			xbps_dictionary_get_bool(obj, "preserve", &preserve);
			/*
			 * Skip files from packages with `preserve`.
			 */
			if (preserve) {
				free(pkgname);
				continue;
			}
			filesd = xbps_pkgdb_get_pkg_files(xhp, pkgname);
			if (filesd == NULL) {
				free(pkgname);
				continue;
			}
			assert(oldpkgver);
			rv = collect_files(xhp, filesd, pkgname, pkgver, idx, true);
			if (rv != 0) {
				free(pkgname);
				break;
			}
		}
		free(pkgname);
	}
	xbps_object_iterator_reset(iter);

	qsort(items, itemsidx, sizeof (struct item *), pathcmp);

	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL, rv, xhp->rootdir,
		    "[files] failed to chdir to rootdir `%s': %s",
		    xhp->rootdir, strerror(errno));
	}

	if (rv != 0)
		return rv;
	return collect_obsoletes(xhp);
}
