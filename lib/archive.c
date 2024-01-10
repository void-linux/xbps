/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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

#include <archive.h>
#include <archive_entry.h>

#include "xbps_api_impl.h"

char HIDDEN *
xbps_archive_get_file(struct archive *ar, struct archive_entry *entry)
{
	size_t buflen;
	ssize_t nbytes = -1;
	char *buf;

	assert(ar != NULL);
	assert(entry != NULL);

	buflen = (size_t)archive_entry_size(entry);
	buf = malloc(buflen+1);
	if (buf == NULL)
		return NULL;

	nbytes = archive_read_data(ar, buf, buflen);
	if ((size_t)nbytes != buflen) {
		free(buf);
		return NULL;
	}
	buf[buflen] = '\0';
	return buf;
}

xbps_dictionary_t HIDDEN
xbps_archive_get_dictionary(struct archive *ar, struct archive_entry *entry)
{
	xbps_dictionary_t d = NULL;
	char *buf;

	if ((buf = xbps_archive_get_file(ar, entry)) == NULL)
		return NULL;

	/* If blob is already a dictionary we are done */
	d = xbps_dictionary_internalize(buf);
	free(buf);
	return d;
}

int
xbps_archive_append_buf(struct archive *ar, const void *buf, const size_t buflen,
	const char *fname, const mode_t mode, const char *uname, const char *gname)
{
	struct archive_entry *entry;

	assert(ar);
	assert(buf);
	assert(fname);
	assert(uname);
	assert(gname);

	entry = archive_entry_new();
	if (entry == NULL)
		return archive_errno(ar);

	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, mode);
	archive_entry_set_uname(entry, uname);
	archive_entry_set_gname(entry, gname);
	archive_entry_set_pathname(entry, fname);
	archive_entry_set_size(entry, buflen);

	if (archive_write_header(ar, entry) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return archive_errno(ar);
	}
	if (archive_write_data(ar, buf, buflen) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return archive_errno(ar);
	}
	if (archive_write_finish_entry(ar) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return archive_errno(ar);
	}
	archive_entry_free(entry);

	return 0;
}
