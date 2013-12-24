/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/plist_fetch.c
 * @brief Package URL metadata files handling
 * @defgroup plist_fetch Package URL metadata files handling
 */

struct fetch_archive {
	struct url *url;
	struct fetchIO *fetch;
	char buffer[32768];
};

static int
fetch_archive_open(struct archive *a _unused, void *client_data)
{
	struct fetch_archive *f = client_data;

	f->fetch = fetchGet(f->url, NULL);
	if (f->fetch == NULL)
		return ENOENT;

	return 0;
}

static ssize_t
fetch_archive_read(struct archive *a _unused, void *client_data, const void **buf)
{
	struct fetch_archive *f = client_data;

	*buf = f->buffer;

	return fetchIO_read(f->fetch, f->buffer, sizeof(f->buffer));
}

static int
fetch_archive_close(struct archive *a _unused, void *client_data)
{
	struct fetch_archive *f = client_data;

	if (f->fetch != NULL)
		fetchIO_close(f->fetch);
	free(f);

	return 0;
}

static struct archive *
open_archive_by_url(struct url *url)
{
	struct fetch_archive *f;
	struct archive *a;

	f = malloc(sizeof(struct fetch_archive));
	if (f == NULL)
		return NULL;

	f->url = url;
	if ((a = archive_read_new()) == NULL) {
		free(f);
		return NULL;
	}
	archive_read_support_compression_gzip(a);
	archive_read_support_compression_bzip2(a);
	archive_read_support_compression_xz(a);
	archive_read_support_format_tar(a);

	if (archive_read_open(a, f, fetch_archive_open, fetch_archive_read,
	    fetch_archive_close)) {
		free(f);
		archive_read_finish(a);
		return NULL;
	}

	return a;
}

static struct archive *
open_archive(const char *url)
{
	struct url *u;
	struct archive *a;

	if (!xbps_repository_is_remote(url)) {
		if ((a = archive_read_new()) == NULL)
			return NULL;

		archive_read_support_compression_gzip(a);
		archive_read_support_compression_bzip2(a);
		archive_read_support_compression_xz(a);
		archive_read_support_format_tar(a);

		if (archive_read_open_filename(a, url, 32768)) {
			archive_read_finish(a);
			return NULL;
		}
		return a;
	}
	
	if ((u = fetchParseURL(url)) == NULL)
		return NULL;

	a = open_archive_by_url(u);
	fetchFreeURL(u);

	return a;
}

xbps_dictionary_t
xbps_get_pkg_plist_from_binpkg(const char *fname, const char *plistf)
{
	xbps_dictionary_t plistd = NULL;
	struct archive *a;
	struct archive_entry *entry;
	const char *comptype;
	int i = 0;

	assert(fname != NULL);
	assert(plistf != NULL);

	if ((a = open_archive(fname)) == NULL)
		return NULL;

	/*
	 * Save compression type string for future use.
	 */
	comptype = archive_compression_name(a);

	while ((archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		if (strcmp(archive_entry_pathname(entry), plistf)) {
			archive_read_data_skip(a);
			if (i >= 3) {
				/*
				 * Archive does not contain required
				 * plist file, discard it completely.
				 */
				errno = ENOENT;
				break;
			}
			i++;
			continue;
		}
		plistd = xbps_archive_get_dictionary(a, entry);
		if (plistd == NULL) {
			errno = EINVAL;
			break;
		}
		xbps_dictionary_set_cstring_nocopy(plistd,
		    "archive-compression-type", comptype);

		break;
	}
	archive_read_finish(a);

	return plistd;
}
