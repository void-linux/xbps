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
fetch_archive_open(struct archive *a UNUSED, void *client_data)
{
	struct fetch_archive *f = client_data;

	f->fetch = fetchGet(f->url, NULL);

	if (f->fetch == NULL)
		return ENOENT;

	return 0;
}

static ssize_t
fetch_archive_read(struct archive *a UNUSED, void *client_data, const void **buf)
{
	struct fetch_archive *f = client_data;

	*buf = f->buffer;
	return fetchIO_read(f->fetch, f->buffer, sizeof(f->buffer));
}

static int
fetch_archive_close(struct archive *a UNUSED, void *client_data)
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
	archive_read_finish(a);

	return buf;
}

bool
xbps_repo_fetch_remote(struct xbps_repo *repo, const char *url)
{
	struct archive *a;
	struct archive_entry *entry;
	uint8_t i = 0;

	assert(url);
	assert(repo);

	if ((a = open_archive(url)) == NULL)
		return false;

	while ((archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		const char *bfile;
		char *buf;

		bfile = archive_entry_pathname(entry);
		if (bfile[0] == '.')
			bfile++; /* skip first dot */

		if (strcmp(bfile, "index-meta.plist") == 0) {
			buf = xbps_archive_get_file(a, entry);
			repo->idxmeta = xbps_dictionary_internalize(buf);
			free(buf);
			i++;
		} else if (strcmp(bfile, "index.plist") == 0) {
			buf = xbps_archive_get_file(a, entry);
			repo->idx = xbps_dictionary_internalize(buf);
			free(buf);
			i++;
		} else {
			archive_read_data_skip(a);
		}
		if (i == 2)
			break;
	}
	archive_read_finish(a);

	if (xbps_object_type(repo->idxmeta) == XBPS_TYPE_DICTIONARY)
		repo->is_signed = true;

	if (xbps_object_type(repo->idx) == XBPS_TYPE_DICTIONARY)
		return true;

	return false;
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

	while ((archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		const char *bfile;

		bfile = archive_entry_pathname(entry);
		if (bfile[0] == '.')
			bfile++; /* skip first dot */

		if (strcmp(bfile, fname) == 0) {
			rv = archive_read_data_into_fd(a, fd);
			if (rv != 0)
				rv = archive_errno(a);

			break;
		}
		archive_read_data_skip(a);
	}
	archive_read_finish(a);

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
