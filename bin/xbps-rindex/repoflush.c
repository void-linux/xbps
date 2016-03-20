/*-
 * Copyright (c) 2013-2015 Juan Romero Pardines.
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
	const char *reponame, xbps_dictionary_t idx, xbps_dictionary_t meta)
{
	struct archive *ar;
	char *repofile, *tname, *buf;
	int rv, repofd = -1;
	mode_t mask;

	/* Create a tempfile for our repository archive */
	repofile = xbps_repo_path_with_name(xhp, repodir, reponame);
	tname = xbps_xasprintf("%s.XXXXXXXXXX", repofile);
	mask = umask(S_IXUSR|S_IRWXG|S_IRWXO);
	if ((repofd = mkstemp(tname)) == -1)
		return false;

	umask(mask);
	/* Create and write our repository archive */
	ar = archive_write_new();
	assert(ar);
	archive_write_set_compression_gzip(ar);
	archive_write_set_format_pax_restricted(ar);
	archive_write_set_options(ar, "compression-level=9");
	archive_write_open_fd(ar, repofd);

	/* XBPS_REPOIDX */
	buf = xbps_dictionary_externalize(idx);
	assert(buf);
	rv = xbps_archive_append_buf(ar, buf, strlen(buf),
	    XBPS_REPOIDX, 0644, "root", "root");
	free(buf);
	if (rv != 0)
		return false;

	/* XBPS_REPOIDX_META */
	if (meta == NULL) {
		/* fake entry */
		buf = strdup("DEADBEEF");
	} else {
		buf = xbps_dictionary_externalize(meta);
	}
	rv = xbps_archive_append_buf(ar, buf, strlen(buf),
	    XBPS_REPOIDX_META, 0644, "root", "root");
	free(buf);
	if (rv != 0)
		return false;

	/* Write data to tempfile and rename */
	archive_write_finish(ar);
#ifdef HAVE_FDATASYNC
	fdatasync(repofd);
#else
	fsync(repofd);
#endif
	assert(fchmod(repofd, 0664) != -1);
	close(repofd);
	rename(tname, repofile);
	free(repofile);
	free(tname);

	return true;
}
