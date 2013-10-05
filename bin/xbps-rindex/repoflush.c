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

struct repodata *
repodata_init(struct xbps_handle *xhp, const char *repodir)
{
	struct repodata *rd;

	rd = malloc(sizeof(struct repodata));
	assert(rd);

	/* Create a tempfile for our repository archive */
	rd->repofile = xbps_repo_path(xhp, repodir);
	rd->tname = xbps_xasprintf("%s.XXXXXXXXXX", rd->repofile);
	if ((rd->repofd = mkstemp(rd->tname)) == -1) {
		free(rd);
		return NULL;
	}

	/* Create and write our repository archive */
	rd->ar = archive_write_new();
	assert(rd->ar);
	archive_write_set_compression_gzip(rd->ar);
	archive_write_set_format_pax_restricted(rd->ar);
	archive_write_set_options(rd->ar, "compression-level=9");
	archive_write_open_fd(rd->ar, rd->repofd);

	return rd;
}

int
repodata_add_buf(struct repodata *rd, const char *buf, const char *filename)
{
	return xbps_archive_append_buf(rd->ar, buf, strlen(buf),
	    filename, 0644, "root", "root");
}

void
repodata_flush(struct repodata *rd)
{
	mode_t myumask;

	/* Write data to tempfile and rename */
	archive_write_finish(rd->ar);
	fdatasync(rd->repofd);
	myumask = umask(0);
	(void)umask(myumask);
	assert(fchmod(rd->repofd, 0666 & ~myumask) != -1);
	close(rd->repofd);
	rename(rd->tname, rd->repofile);
	free(rd->repofile);
	free(rd->tname);
	free(rd);
}
