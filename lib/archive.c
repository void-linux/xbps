/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

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
