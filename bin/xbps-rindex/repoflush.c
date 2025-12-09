/*-
 * Copyright (c) 2013-2019 Juan Romero Pardines.
 * Copyright (c) 2023-2025 Duncan Overbruck <mail@duncano.de>.
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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include <xbps.h>

#include "defs.h"

static struct archive *
open_archive(int fd, const char *compression)
{
	struct archive *ar;
	int r;

	ar = archive_write_new();
	if (!ar)
		return NULL;
	/*
	 * Set compression format, zstd by default.
	 */
	if (compression == NULL || strcmp(compression, "zstd") == 0) {
		archive_write_add_filter_zstd(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "gzip") == 0) {
		archive_write_add_filter_gzip(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "bzip2") == 0) {
		archive_write_add_filter_bzip2(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "lz4") == 0) {
		archive_write_add_filter_lz4(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "xz") == 0) {
		archive_write_add_filter_xz(ar);
		archive_write_set_options(ar, "compression-level=9");
	} else if (strcmp(compression, "none") == 0) {
		/* empty */
	} else {
		archive_write_free(ar);
		errno = EINVAL;
		return NULL;
	}

	archive_write_set_format_pax_restricted(ar);
	r = archive_write_open_fd(ar, fd);
	if (r != ARCHIVE_OK) {
		r = -archive_errno(ar);
		if (r == 1)
			r = -EINVAL;
		archive_write_free(ar);
		errno = -r;
		return NULL;
	}

	return ar;
}

static int
archive_dict(struct archive *ar, const char *filename, xbps_dictionary_t dict)
{
	char *buf;
	int r;

	if (xbps_dictionary_count(dict) == 0) {
		r = xbps_archive_append_buf(ar, "", 0, filename, 0644,
		    "root", "root");
		if (r < 0)
			return r;
		return 0;
	}

	errno = 0;
	buf = xbps_dictionary_externalize(dict);
	if (!buf) {
		r = -errno;
		xbps_error_printf("failed to externalize dictionary for: %s\n",
		    filename);
		if (r == 0)
			return -EINVAL;
		return 0;
	}

	r = xbps_archive_append_buf(ar, buf, strlen(buf), filename, 0644,
	    "root", "root");

	free(buf);

	if (r < 0) {
		xbps_error_printf("failed to write archive entry: %s: %s\n",
		    filename, strerror(-r));
	}
	return r;
}

int
repodata_write_fd(int fd, xbps_dictionary_t index, xbps_dictionary_t stage,
    xbps_dictionary_t meta, const char *compression)
{
	struct archive *ar;
	int r;

	ar = open_archive(fd, compression);
	if (!ar) {
		r = -errno;
		xbps_error_printf("failed to open archive: %s\n", strerror(-r));
		return r;
	}

	r = archive_dict(ar, XBPS_REPODATA_INDEX, index);
	if (r < 0)
		goto err;
	r = archive_dict(ar, XBPS_REPODATA_META, meta);
	if (r < 0)
		goto err;
	r = archive_dict(ar, XBPS_REPODATA_STAGE, stage);
	if (r < 0)
		goto err;

	if (archive_write_close(ar) == ARCHIVE_FATAL) {
		r = -archive_errno(ar);
		xbps_error_printf(
		    "failed to close archive: %s\n", archive_error_string(ar));
		archive_write_free(ar);
		return r;
	}
	if (archive_write_free(ar) == ARCHIVE_FATAL) {
		r = -errno;
		xbps_error_printf(
		    "failed to free archive: %s\n", strerror(-r));
		archive_write_free(ar);
		return r;
	}

#ifdef HAVE_FDATASYNC
	fdatasync(fd);
#else
	fsync(fd);
#endif
	return 0;

err:
	archive_write_free(ar);
	return r;
}

int
repodata_write_tmpfile(char *path, size_t pathsz, char *tmp, size_t tmpsz,
    const char *repodir, const char *arch, xbps_dictionary_t index,
    xbps_dictionary_t stage, xbps_dictionary_t meta, const char *compression)
{
	mode_t prevumask;
	int fd;
	int r;

	r = snprintf(path, pathsz, "%s/%s-repodata", repodir, arch);
	if (r < 0 || (size_t)r >= pathsz) {
		xbps_error_printf("repodata path too long: %s: %s\n", path,
		    strerror(ENAMETOOLONG));
		return -ENAMETOOLONG;
	}

	r = snprintf(tmp, tmpsz, "%s.XXXXXXX", path);
	if (r < 0 || (size_t)r >= tmpsz) {
		xbps_error_printf("repodata tmp path too long: %s: %s\n", path,
		    strerror(ENAMETOOLONG));
		return -ENAMETOOLONG;
	}

	prevumask = umask(S_IXUSR|S_IRWXG|S_IRWXO);

	fd = mkstemp(tmp);
	if (fd == -1) {
		r = -errno;
		xbps_error_printf("failed to open temp file: %s: %s", tmp, strerror(-r));
		umask(prevumask);
		goto err;
	}

	umask(prevumask);

	r = repodata_write_fd(fd, index, stage, meta, compression);
	if (r < 0)
		goto err;

	if (fchmod(fd, 0664) == -1) {
		errno = -r;
		xbps_error_printf("failed to set mode: %s: %s\n",
		   tmp, strerror(-r));
		close(fd);
		unlink(tmp);
		return r;
	}
	close(fd);

	return 0;
err:
	if (fd != -1)
		close(fd);
	unlink(tmp);
	return r;
}

int
repodata_write_file(const char *repodir, const char *arch,
    xbps_dictionary_t index, xbps_dictionary_t stage, xbps_dictionary_t meta,
    const char *compression)
{
	char path[PATH_MAX];
	char tmp[PATH_MAX];
	int r;

	r = repodata_write_tmpfile(path, sizeof(path), tmp, sizeof(tmp), repodir,
	    arch, index, stage, meta, compression);
	if (r < 0)
		return r;

	if (rename(tmp, path) == -1) {
		r = -errno;
		xbps_error_printf("failed to rename repodata: %s: %s: %s\n",
		   tmp, path, strerror(-r));
		unlink(tmp);
		return r;
	}

	return 0;
}
