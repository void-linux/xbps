/*-
 * Copyright (c) 2013-2019 Juan Romero Pardines.
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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include <xbps.h>

#include "defs.h"

bool
repodata_flush(struct xbps_handle *xhp, const char *repodir,
	const char *reponame, xbps_dictionary_t idx, xbps_dictionary_t meta,
	const char *compression)
{
	struct archive *ar;
	char *repofile, *tname, *buf;
	int rv, repofd = -1;
	mode_t mask;
	bool result;

	/* Create a tempfile for our repository archive */
	repofile = xbps_repo_path_with_name(xhp, repodir, reponame);
	assert(repofile);
	tname = xbps_xasprintf("%s.XXXXXXXXXX", repofile);
	assert(tname);
	mask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if ((repofd = mkstemp(tname)) == -1)
		return false;

	umask(mask);
	/* Create and write our repository archive */
	ar = archive_write_new();
	if (ar == NULL)
		return false;

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
		return false;
	}

	archive_write_set_format_pax_restricted(ar);
	if (archive_write_open_fd(ar, repofd) != ARCHIVE_OK)
		return false;

	/* XBPS_REPOIDX */
	buf = xbps_dictionary_externalize(idx);
	if (buf == NULL)
		return false;
	rv = xbps_archive_append_buf(ar, buf, strlen(buf),
	    XBPS_REPOIDX, 0644, "root", "root");
	free(buf);
	if (rv != 0)
		return false;

	/* XBPS_REPOIDX_META */
	if (meta == NULL) {
		/* fake entry */
		buf = strdup("DEADBEEF");
		if (buf == NULL)
			return false;
	} else {
		buf = xbps_dictionary_externalize(meta);
	}
	rv = xbps_archive_append_buf(ar, buf, strlen(buf),
	    XBPS_REPOIDX_META, 0644, "root", "root");
	free(buf);
	if (rv != 0)
		return false;

	/* Write data to tempfile and rename */
	if (archive_write_close(ar) != ARCHIVE_OK)
		return false;
	if (archive_write_free(ar) != ARCHIVE_OK)
		return false;
#ifdef HAVE_FDATASYNC
	fdatasync(repofd);
#else
	fsync(repofd);
#endif
	if (fchmod(repofd, 0664) == -1) {
		close(repofd);
		unlink(tname);
		result = false;
		goto out;
	}
	close(repofd);
	if (rename(tname, repofile) == -1) {
		unlink(tname);
		result = false;
		goto out;
	}
	result = true;
out:
	free(repofile);
	free(tname);

	return result;
}
