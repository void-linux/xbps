/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
 * Copyright (c) 2008, 2009 Joerg Sonnenberger <joerg (at) NetBSD.org>
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
 *-
 * From: $NetBSD: pkg_io.c,v 1.9 2009/08/16 21:10:15 joerg Exp $
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

#include "xbps_api_impl.h"
#include "fetch.h"

/**
 * @file lib/plist_fetch.c
 * @brief Package URL metadata files handling
 * @defgroup plist_fetch Package URL metadata files handling
 */

static struct archive *
open_archive(const char *url)
{
	struct archive *ar;
	int r;

	ar = xbps_archive_read_new();
	if (!ar) {
		r = -errno;
		xbps_error_printf("failed to open archive: %s: %s\n", url, strerror(-r));
		errno = -r;
		return NULL;
	}

	if (xbps_repository_is_remote(url)) {
		r = xbps_archive_read_open_remote(ar, url);
	} else {
		r = xbps_archive_read_open(ar, url);
	}
	if (r < 0) {
		xbps_error_printf("failed to open archive: %s: %s\n", url, strerror(-r));
		archive_read_free(ar);
		errno = -r;
		return NULL;
	}
	
	return ar;
}

char *
xbps_archive_fetch_file(const char *url, const char *fname)
{
	struct archive *a;
	struct archive_entry *entry;
	char *buf = NULL;

	assert(url);
	assert(fname);

	if ((a = open_archive(url)) == NULL)
		return NULL;

	while ((archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		const char *bfile;

		bfile = archive_entry_pathname(entry);
		if (bfile[0] == '.')
			bfile++; /* skip first dot */

		if (strcmp(bfile, fname) == 0) {
			buf = xbps_archive_get_file(a, entry);
			break;
		}
		archive_read_data_skip(a);
	}
	archive_read_free(a);

	return buf;
}

int
xbps_archive_fetch_file_into_fd(const char *url, const char *fname, int fd)
{
	struct archive *a;
	struct archive_entry *entry;
	int rv = 0;

	assert(url);
	assert(fname);
	assert(fd != -1);

	if ((a = open_archive(url)) == NULL)
		return EINVAL;

	for (;;) {
		const char *bfile;
		rv = archive_read_next_header(a, &entry);
		if (rv == ARCHIVE_EOF) {
			rv = 0;
			break;
		}
		if (rv == ARCHIVE_FATAL) {
			const char *error = archive_error_string(a);
			if (error != NULL) {
				xbps_error_printf(
				    "Reading archive entry from: %s: %s\n",
				    url, error);
			} else {
				xbps_error_printf(
				    "Reading archive entry from: %s: %s\n",
				    url, strerror(xbps_archive_errno(a)));
			}
			rv = xbps_archive_errno(a);
			break;
		}
		bfile = archive_entry_pathname(entry);
		if (bfile[0] == '.')
			bfile++; /* skip first dot */

		if (strcmp(bfile, fname) == 0) {
			rv = archive_read_data_into_fd(a, fd);
			if (rv != ARCHIVE_OK)
				rv = xbps_archive_errno(a);
			break;
		}
		archive_read_data_skip(a);
	}
	archive_read_free(a);

	return rv;
}

xbps_dictionary_t
xbps_archive_fetch_plist(const char *url, const char *plistf)
{
	xbps_dictionary_t d;
	char *buf;

	if ((buf = xbps_archive_fetch_file(url, plistf)) == NULL)
		return NULL;

	d = xbps_dictionary_internalize(buf);
	free(buf);
	return d;
}
