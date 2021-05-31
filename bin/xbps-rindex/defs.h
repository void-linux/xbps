/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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

#ifndef _XBPS_RINDEX_DEFS_H_
#define _XBPS_RINDEX_DEFS_H_

#include <xbps.h>
#include <assert.h>
#include <ctype.h>

/* libarchive compat */
#if ARCHIVE_VERSION_NUMBER >= 3000000

#define archive_read_support_compression_gzip(x) \
	archive_read_support_filter_gzip(x)

#define archive_read_support_compression_bzip2(x) \
	archive_read_support_filter_bzip2(x)

#define archive_read_support_compression_xz(x) \
	archive_read_support_filter_xz(x)

#define archive_write_set_compression_gzip(x) \
	archive_write_add_filter_gzip(x)

#define archive_write_set_compression_bzip2(x) \
	archive_write_add_filter_bzip2(x)

#define archive_write_set_compression_xz(x) \
	archive_write_add_filter_xz(x)

#define archive_read_finish(x) \
	archive_read_free(x)

#define archive_write_finish(x) \
	archive_write_free(x)

#define archive_compression_name(x) \
	archive_filter_name(x, 0)

#endif

#define _XBPS_RINDEX		"xbps-rindex"

/* From index-add.c */
int	index_add(struct xbps_handle *, int, int, char **, bool, const char *);

/* From index-clean.c */
int	index_clean(struct xbps_handle *, const char *, bool, const char *);

/* From remove-obsoletes.c */
int	remove_obsoletes(struct xbps_handle *, const char *, int);

/* From sign.c */
int	sign_repo(struct xbps_handle *, const char *, const char *,
		const char *, const char *);
int	sign_pkgs(struct xbps_handle *, int, int, char **, const char *, bool);

/* From repoflush.c */
bool	repodata_flush(struct xbps_handle *, const char *, const char *,
		xbps_dictionary_t, xbps_dictionary_t, const char *);

#endif /* !_XBPS_RINDEX_DEFS_H_ */
