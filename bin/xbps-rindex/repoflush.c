/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <fcntl.h>

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
