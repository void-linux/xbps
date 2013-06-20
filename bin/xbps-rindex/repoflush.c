/*-
 * Copyright (c) 2013 Juan Romero Pardines.
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

int
repodata_flush(struct xbps_handle *xhp, const char *repodir,
	xbps_dictionary_t idx, xbps_dictionary_t idxfiles)
{
	struct archive *ar;
	mode_t myumask;
	char *repofile, *tname, *xml;
	int repofd;

	/* Create a tempfile for our repository archive */
	repofile = xbps_repo_path(xhp, repodir);
	tname = xbps_xasprintf("%s.XXXXXXXXXX", repofile);
	if ((repofd = mkstemp(tname)) == -1)
		return errno;

	/* Create and write our repository archive */
	ar = archive_write_new();
	assert(ar);
	archive_write_add_filter_gzip(ar);
	archive_write_set_format_pax_restricted(ar);
	archive_write_set_options(ar, "compression-level=9");
	archive_write_open_fd(ar, repofd);

	xml = xbps_dictionary_externalize(idx);
	assert(xml);
	if (xbps_archive_append_buf(ar, xml, strlen(xml),
	    XBPS_PKGINDEX, 0644, "root", "root") != 0) {
		free(xml);
		return -1;
	}
	free(xml);

	xml = xbps_dictionary_externalize(idxfiles);
	assert(xml);
	if (xbps_archive_append_buf(ar, xml, strlen(xml),
	    XBPS_PKGINDEX_FILES, 0644, "root", "root") != 0) {
		free(xml);
		return -1;
	}
	free(xml);

	archive_write_free(ar);

	/* Write data to tempfile and rename */
	fdatasync(repofd);
	myumask = umask(0);
	(void)umask(myumask);
	assert(fchmod(repofd, 0666 & ~myumask) != -1);
	close(repofd);
	rename(tname, repofile);
	free(repofile);
	free(tname);

	return 0;
}
