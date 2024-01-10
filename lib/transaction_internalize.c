/*-
 * Copyright (c) 2021 Duncan Overbruck <mail@duncano.de>
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
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include "xbps_api_impl.h"

static int
internalize_script(xbps_dictionary_t pkg_repod, const char *script,
		struct archive *ar, struct archive_entry *entry)
{
	char buffer[BUFSIZ];
	xbps_data_t data = NULL;
	char *buf = NULL;
	int64_t entry_size = archive_entry_size(entry);

	if (entry_size == 0)
		return 0;
	if (entry_size < 0)
		return -EINVAL;

	if ((size_t)entry_size > sizeof buffer) {
		buf = malloc(entry_size);
		if (buf == NULL)
			return -errno;
	}

	if (archive_read_data(ar, buf != NULL ? buf : buffer, entry_size) != entry_size) {
		free(buf);
		return -errno;
	}

	data = xbps_data_create_data(buf != NULL ? buf : buffer, entry_size);
	if (data == NULL) {
		free(buf);
		return -errno;
	}

	free(buf);
	xbps_dictionary_set(pkg_repod, script, data);
	xbps_object_release(data);
	return 0;
}

static int
internalize_binpkg(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	char pkgfile[PATH_MAX];
	xbps_dictionary_t filesd = NULL, propsd = NULL;
	struct stat st;
	struct archive *ar = NULL;
	struct archive_entry *entry;
	const char *pkgver, *pkgname, *binpkg_pkgver;
	ssize_t l;
	int pkg_fd = -1;
	int rv = 0;

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	assert(pkgver);
	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	assert(pkgname);

	l = xbps_pkg_path(xhp, pkgfile, sizeof(pkgfile), pkg_repod);
	if (l < 0)
		return l;

	if ((ar = archive_read_new()) == NULL)
		return -errno;

	/*
	 * Enable support for tar format and gzip/bzip2/lzma compression methods.
	 */
	archive_read_support_filter_gzip(ar);
	archive_read_support_filter_bzip2(ar);
	archive_read_support_filter_xz(ar);
	archive_read_support_filter_lz4(ar);
	archive_read_support_filter_zstd(ar);
	archive_read_support_format_tar(ar);

	pkg_fd = open(pkgfile, O_RDONLY|O_CLOEXEC);
	if (pkg_fd == -1) {
		rv = -errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    -rv, pkgver,
		    "%s: failed to open binary package `%s': %s",
		    pkgver, pkgfile, strerror(rv));
		goto out;
	}
	if (fstat(pkg_fd, &st) == -1) {
		rv = -errno;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    -rv, pkgver,
		    "%s: failed to fstat binary package `%s': %s",
		    pkgver, pkgfile, strerror(rv));
		goto out;
	}
	if (archive_read_open_fd(ar, pkg_fd, st.st_blksize) == ARCHIVE_FATAL) {
		rv = archive_errno(ar);
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL,
		    -rv, pkgver,
		    "%s: failed to read binary package `%s': %s",
		    pkgver, pkgfile, strerror(rv));
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

		if (strcmp("./INSTALL", entry_pname) == 0) {
			rv = internalize_script(pkg_repod, "install-script", ar, entry);
			if (rv < 0)
				goto out;
		} else if (strcmp("./REMOVE", entry_pname) == 0) {
			rv = internalize_script(pkg_repod, "remove-script", ar, entry);
			if (rv < 0)
				goto out;
		} else if ((strcmp("./files.plist", entry_pname)) == 0) {
			filesd = xbps_archive_get_dictionary(ar, entry);
			if (filesd == NULL) {
				rv = -EINVAL;
				goto out;
			}
		} else if (strcmp("./props.plist", entry_pname) == 0) {
			propsd = xbps_archive_get_dictionary(ar, entry);
			if (propsd == NULL) {
				rv = -EINVAL;
				goto out;
			}
		} else {
			break;
		}
	}

	/*
	 * Bail out if required metadata files are not in archive.
	 */
	if (propsd == NULL || filesd == NULL) {
		rv = -ENODEV;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL, -rv, pkgver,
		    "%s: [files] invalid binary package `%s'.", pkgver, pkgfile);
		goto out;
	}

	/*
	 * Bail out if repo pkgver does not match binpkg pkgver, i.e. downgrade attack
	 * by advertising a old signed package with a new version.
	 */
	xbps_dictionary_get_cstring_nocopy(propsd, "pkgver", &binpkg_pkgver);
	if (strcmp(pkgver, binpkg_pkgver) != 0) {
		rv = -EINVAL;
		xbps_set_cb_state(xhp, XBPS_STATE_FILES_FAIL, -rv, pkgver,
		    "%s: [files] pkgver mismatch repodata: `%s' binpkg: `%s'.",
		    pkgfile, pkgver, binpkg_pkgver);
		goto out;
	}

out:
	xbps_object_release(propsd);
	xbps_object_release(filesd);
	if (pkg_fd != -1)
		close(pkg_fd);
	if (ar != NULL)
		archive_read_free(ar);
	return rv;
}

int
xbps_transaction_internalize(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_object_t obj;

	assert(xhp);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_trans_type_t ttype;
		int rv;

		ttype = xbps_transaction_pkg_type(obj);
		if (ttype != XBPS_TRANS_INSTALL && ttype != XBPS_TRANS_UPDATE)
			continue;

		rv = internalize_binpkg(xhp, obj);
		if (rv < 0)
			return rv;
	}
	xbps_object_iterator_reset(iter);

	return 0;
}
