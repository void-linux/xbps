/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <zlib.h>

#include "xbps_api_impl.h"

/*
 * Takes a compressed data buffer, decompresses it and returns the
 * new buffer uncompressed if all was right.
 */
#define _READ_CHUNK	8192

static char *
uncompress_plist_data(char *xml, size_t len)
{
	z_stream strm;
	unsigned char *out;
	char *uncomp_xml = NULL;
	size_t have;
	ssize_t totalsize = 0;
	int rv = 0;

	assert(xml != NULL);

	/* Decompress the mmap'ed buffer with zlib */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	/* 15+16 to use gzip method */
	if (inflateInit2(&strm, 15+16) != Z_OK)
		return NULL;

	strm.avail_in = len;
	strm.next_in = (unsigned char *)xml;

	/* Output buffer (uncompressed) */
	if ((uncomp_xml = malloc(_READ_CHUNK)) == NULL) {
		(void)inflateEnd(&strm);
		return NULL;
	}

	/* temp output buffer for inflate */
	if ((out = malloc(_READ_CHUNK)) == NULL) {
		(void)inflateEnd(&strm);
		free(uncomp_xml);
		return NULL;
	}

	/* Inflate the input buffer and copy into 'uncomp_xml' */
	do {
		strm.avail_out = _READ_CHUNK;
		strm.next_out = out;
		rv = inflate(&strm, Z_NO_FLUSH);
		switch (rv) {
			case Z_DATA_ERROR:
			case Z_STREAM_ERROR:
			case Z_NEED_DICT:
			case Z_MEM_ERROR:
			case Z_BUF_ERROR:
			case Z_VERSION_ERROR:
				(void)inflateEnd(&strm);
				free(uncomp_xml);
				free(out);
				return NULL;
		}
		have = _READ_CHUNK - strm.avail_out;
		totalsize += have;
		uncomp_xml = realloc(uncomp_xml, totalsize);
		memcpy(uncomp_xml + totalsize - have, out, have);
	} while (strm.avail_out == 0);

	/* we are done */
	(void)inflateEnd(&strm);
	free(out);

	return uncomp_xml;
}
#undef _READ_CHUNK

xbps_dictionary_t HIDDEN
xbps_archive_get_dictionary(struct archive *ar, struct archive_entry *entry)
{
	xbps_dictionary_t d = NULL;
	size_t buflen;
	ssize_t nbytes = -1;
	char *buf, *uncomp_buf;

	assert(ar != NULL);
	assert(entry != NULL);

	buflen = (size_t)archive_entry_size(entry);
	buf = malloc(buflen);
	if (buf == NULL)
		return NULL;

	nbytes = archive_read_data(ar, buf, buflen);
	if ((size_t)nbytes != buflen) {
		free(buf);
		return NULL;
	}

	/* If blob is already a dictionary we are done */
	d = xbps_dictionary_internalize(buf);
	if (xbps_object_type(d) == XBPS_TYPE_DICTIONARY) {
		free(buf);
		return d;
	}

	/* Try to uncompress blob */
	uncomp_buf = uncompress_plist_data(buf, buflen);
	if (uncomp_buf == NULL) {
		/* Error while decompressing */
		free(uncomp_buf);
		xbps_object_release(d);
		d = NULL;
	} else {
		/* We have the uncompressed data */
		d = xbps_dictionary_internalize(uncomp_buf);
		free(uncomp_buf);
	}

	free(buf);
	return d;
}

int
xbps_archive_append_buf(struct archive *ar, const void *buf, const size_t buflen,
	const char *fname, const mode_t mode, const char *uname, const char *gname)
{
	struct archive_entry *entry;
	time_t tm;

	assert(ar);
	assert(buf);
	assert(fname);
	assert(uname);
	assert(gname);

	tm = time(NULL);
	entry = archive_entry_new();
	assert(entry);

	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, mode);
	archive_entry_set_uname(entry, uname);
	archive_entry_set_gname(entry, gname);
	archive_entry_set_pathname(entry, fname);
	archive_entry_set_size(entry, buflen);
	archive_entry_set_atime(entry, tm, 0);
	archive_entry_set_mtime(entry, tm, 0);
	archive_entry_set_ctime(entry, tm, 0);

	if (archive_write_header(ar, entry) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return archive_errno(ar);
	}
	if (archive_write_data(ar, buf, buflen) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return archive_errno(ar);
	}
	archive_write_finish_entry(ar);
	archive_entry_free(entry);

	return 0;
}
