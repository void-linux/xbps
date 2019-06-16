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

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

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
	struct {
		const char *pkgver;
		uint64_t size;
		enum type type;
	} old, new;
};

#define ITHSIZE	1024
#define ITHMASK	(ITHSIZE - 1)

static struct item *ItemHash[ITHSIZE];

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

	for (item = ItemHash[itemhash(file)]; item; item = item->hnext) {
		if (strcmp(file, item->file) == 0)
			return item;
	}
	return NULL;
}

static struct item *
addItem(const char *file)
{
	struct item **itemp;
	struct item *item = calloc(sizeof(*item), 1);

	assert(file);
	assert(item);

	itemp = &ItemHash[itemhash(file)];
	item->hnext = *itemp;
	item->file = strdup(file);
	assert(item->file);
	*itemp = item;

	return item;
}

static int
collect_file(struct xbps_handle *xhp, const char *file, size_t size,
		const char *pkgver, enum type type, bool remove)
{
	struct item *item;
	int rv = 0;

	assert(file);

	if ((item = lookupItem(file)) == NULL) {
		item = addItem(file);
		if (remove) {
			item->old.pkgver = pkgver;
			item->old.type = type;
			item->old.size = size;
		} else {
			item->new.pkgver = pkgver;
			item->new.type = type;
			item->new.size = size;
		}
		return 0;
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
			xbps_dbg_printf(xhp, "%s: [trans] file `%s' already removed"
				"by `%s'\n", pkgver, file, item->old.pkgver);
			return 0;
		}
		item->old.pkgver = pkgver;
		item->old.type = type;
		item->old.size = size;
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
				"%s: [trans] file installed by package `%s' and `%s': %s",
				pkgver, item->new.pkgver, pkgver, file);
			return EEXIST;
		}
		item->new.pkgver = pkgver;
		item->new.type = type;
		item->new.size = size;
	}

	if (item->old.type && item->new.type) {
		/*
		 * The file was removed by one package
		 * and installed by another package.
		 */
		char *newpkgname, *oldpkgname;
		newpkgname = xbps_pkg_name(item->new.pkgver);
		oldpkgname = xbps_pkg_name(item->old.pkgver);
		if (strcmp(newpkgname, oldpkgname) != 0) {
			if (remove) {
				xbps_dbg_printf(xhp, "%s: [trans] file `%s' moved to"
				    " package `%s'\n", pkgver, file, item->new.pkgver);
			} else {
				xbps_dbg_printf(xhp, "%s: [trans] file `%s' moved from"
				    " package `%s'\n", pkgver, file, item->new.pkgver);
			}
		}
		free(newpkgname);
		free(oldpkgname);
	}

	return 0;
}

static int
collect_files(struct xbps_handle *xhp, xbps_dictionary_t d,
			const char *pkgver, bool remove)
{
	struct stat st;
	xbps_array_t a;
	xbps_dictionary_t filed;
	uint64_t size;
	unsigned int i;
	int rv = 0;
	const char *file;

	if ((a = xbps_dictionary_get(d, "files"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			size = 0;
			xbps_dictionary_get_uint64(filed, "size", &size);
			rv = collect_file(xhp, file, size, pkgver, TYPE_FILE, remove);
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
			/* XXX: how to handle conf_file size */
			if (remove && stat(file, &st) != -1 && size != (uint64_t)st.st_size)
				size = 0;
			rv = collect_file(xhp, file, size, pkgver, TYPE_FILE, remove);
			if (rv != 0)
				goto out;
		}
	}
	if ((a = xbps_dictionary_get(d, "links"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			rv = collect_file(xhp, file, 0, pkgver,  TYPE_LINK, remove);
			if (rv != 0)
				goto out;
		}
	}
	if ((a = xbps_dictionary_get(d, "dirs"))) {
		for (i = 0; i < xbps_array_count(a); i++) {
			filed = xbps_array_get(a, i);
			xbps_dictionary_get_cstring_nocopy(filed, "file", &file);
			rv = collect_file(xhp, file, 0, pkgver,  TYPE_DIR, remove);
			if (rv != 0)
				goto out;
		}
	}

out:
	return rv;
}

static int
add_from_archive(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	xbps_dictionary_t filesd;
	struct archive *ar = NULL;
	struct archive_entry *entry;
	struct stat st;
	const char *pkgver;
	char *bpkg;
	/* size_t entry_size; */
	int rv = 0, pkg_fd = -1;

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	assert(pkgver);

	bpkg = xbps_repository_pkg_path(xhp, pkg_repod);
	if (bpkg == NULL)
		return errno;

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
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: [trans] failed to open binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (fstat(pkg_fd, &st) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: [trans] failed to fstat binary package `%s': %s",
		    pkgver, bpkg, strerror(rv));
		goto out;
	}
	if (archive_read_open_fd(ar, pkg_fd, st.st_blksize) == ARCHIVE_FATAL) {
		rv = archive_errno(ar);
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    rv, pkgver,
		    "%s: [trans] failed to read binary package `%s': %s",
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
			rv = collect_files(xhp, filesd, pkgver, false);
			break;
		}
		archive_read_data_skip(ar);
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

int HIDDEN
xbps_transaction_files(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_dictionary_t pkgd, filesd;
	xbps_object_t obj;
	const char *trans, *pkgver;
	bool preserve;
	int rv = 0;

	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		char *pkgname;

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
			rv = add_from_archive(xhp, obj);
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
			xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &oldpkgver);
			xbps_dictionary_get_bool(obj, "preserve", &preserve);
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
			rv = collect_files(xhp, filesd, oldpkgver, true);
			if (rv != 0) {
				free(pkgname);
				break;
			}
		}
		free(pkgname);
	}
	xbps_object_iterator_reset(iter);

	return rv;
}
