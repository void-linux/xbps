/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef _XBPS_RINDEX_DEFS_H_
#define _XBPS_RINDEX_DEFS_H_

#include <xbps.h>

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

#ifndef __UNCONST
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))
#endif

#define _XBPS_RINDEX		"xbps-rindex"

/* From index-add.c */
int	index_add(struct xbps_handle *, int, int, char **, bool, const char *);

/* From index-clean.c */
int	index_clean(struct xbps_handle *, const char *, bool, const char *);

/* From remove-obsoletes.c */
int	remove_obsoletes(struct xbps_handle *, const char *);

/* From sign.c */
int	sign_repo(struct xbps_handle *, const char *, const char *,
		const char *, const char *);
int	sign_pkgs(struct xbps_handle *, int, int, char **, const char *, bool);

/* From repoflush.c */
bool	repodata_flush(struct xbps_handle *, const char *, const char *,
		xbps_dictionary_t, xbps_dictionary_t, const char *);

#endif /* !_XBPS_RINDEX_DEFS_H_ */
