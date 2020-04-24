/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef _XBPS_API_IMPL_H_
#define _XBPS_API_IMPL_H_

#include <assert.h>
#include "xbps.h"

/*
 * By default all public functions have default visibility, unless
 * visibility has been detected by configure and the HIDDEN definition
 * is used.
 */
#if HAVE_VISIBILITY
#define HIDDEN __attribute__ ((visibility("hidden")))
#else
#define HIDDEN
#endif

#include "queue.h"
#include "fetch.h"
#include "compat.h"

#define EXTRACT_FLAGS	ARCHIVE_EXTRACT_SECURE_NODOTDOT | \
			ARCHIVE_EXTRACT_SECURE_SYMLINKS | \
			ARCHIVE_EXTRACT_SECURE_NOABSOLUTEPATHS | \
			ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | \
			ARCHIVE_EXTRACT_UNLINK
#define FEXTRACT_FLAGS	ARCHIVE_EXTRACT_OWNER | EXTRACT_FLAGS

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))
#endif

/* libarchive compat */
#if ARCHIVE_VERSION_NUMBER >= 3000000

#define archive_read_support_compression_all(x) \
	archive_read_support_filter_all(x)

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

#ifndef __arraycount
#define __arraycount(x) (sizeof(x) / sizeof(*x))
#endif

/**
 * @private
 */
int HIDDEN dewey_match(const char *, const char *);
int HIDDEN xbps_pkgdb_init(struct xbps_handle *);
void HIDDEN xbps_pkgdb_release(struct xbps_handle *);
int HIDDEN xbps_pkgdb_conversion(struct xbps_handle *);
int HIDDEN xbps_array_replace_dict_by_name(xbps_array_t, xbps_dictionary_t,
		const char *);
int HIDDEN xbps_array_replace_dict_by_pattern(xbps_array_t, xbps_dictionary_t,
		const char *);
bool HIDDEN xbps_remove_pkg_from_array_by_name(xbps_array_t, const char *);
bool HIDDEN xbps_remove_pkg_from_array_by_pattern(xbps_array_t, const char *);
bool HIDDEN xbps_remove_pkg_from_array_by_pkgver(xbps_array_t, const char *);
void HIDDEN xbps_fetch_set_cache_connection(int, int);
void HIDDEN xbps_fetch_unset_cache_connection(void);
int HIDDEN xbps_cb_message(struct xbps_handle *, xbps_dictionary_t, const char *);
int HIDDEN xbps_entry_is_a_conf_file(xbps_dictionary_t, const char *);
int HIDDEN xbps_entry_install_conf_file(struct xbps_handle *, xbps_dictionary_t,
		xbps_dictionary_t, struct archive_entry *, const char *,
		const char *, bool);
xbps_dictionary_t HIDDEN xbps_find_virtualpkg_in_conf(struct xbps_handle *,
		xbps_dictionary_t, const char *);
xbps_dictionary_t HIDDEN xbps_find_pkg_in_dict(xbps_dictionary_t, const char *);
xbps_dictionary_t HIDDEN xbps_find_virtualpkg_in_dict(struct xbps_handle *,
		xbps_dictionary_t, const char *);
xbps_dictionary_t HIDDEN xbps_find_pkg_in_array(xbps_array_t, const char *,
		xbps_trans_type_t);
xbps_dictionary_t HIDDEN xbps_find_virtualpkg_in_array(struct xbps_handle *,
		xbps_array_t, const char *, xbps_trans_type_t);

/* transaction */
bool HIDDEN xbps_transaction_check_revdeps(struct xbps_handle *, xbps_array_t);
bool HIDDEN xbps_transaction_check_shlibs(struct xbps_handle *, xbps_array_t);
bool HIDDEN xbps_transaction_check_replaces(struct xbps_handle *, xbps_array_t);
bool HIDDEN xbps_transaction_check_conflicts(struct xbps_handle *, xbps_array_t);
bool HIDDEN xbps_transaction_store(struct xbps_handle *, xbps_array_t, xbps_dictionary_t, bool);
int HIDDEN xbps_transaction_init(struct xbps_handle *);
int HIDDEN xbps_transaction_files(struct xbps_handle *,
		xbps_object_iterator_t);
int HIDDEN xbps_transaction_fetch(struct xbps_handle *,
		xbps_object_iterator_t);
int HIDDEN xbps_transaction_pkg_deps(struct xbps_handle *, xbps_array_t, xbps_dictionary_t);

char HIDDEN *xbps_get_remote_repo_string(const char *);
int HIDDEN xbps_repo_sync(struct xbps_handle *, const char *);
int HIDDEN xbps_file_hash_check_dictionary(struct xbps_handle *,
		xbps_dictionary_t, const char *, const char *);
int HIDDEN xbps_file_exec(struct xbps_handle *, const char *, ...);
void HIDDEN xbps_set_cb_fetch(struct xbps_handle *, off_t, off_t, off_t,
		const char *, bool, bool, bool);
int HIDDEN xbps_set_cb_state(struct xbps_handle *, xbps_state_t, int,
		const char *, const char *, ...);
int HIDDEN xbps_unpack_binary_pkg(struct xbps_handle *, xbps_dictionary_t);
int HIDDEN xbps_remove_pkg(struct xbps_handle *, const char *, bool);
int HIDDEN xbps_register_pkg(struct xbps_handle *, xbps_dictionary_t);
char HIDDEN *xbps_archive_get_file(struct archive *, struct archive_entry *);
xbps_dictionary_t HIDDEN xbps_archive_get_dictionary(struct archive *,
		struct archive_entry *);
const char HIDDEN *vpkg_user_conf(struct xbps_handle *, const char *, bool);
xbps_array_t HIDDEN xbps_get_pkg_fulldeptree(struct xbps_handle *,
		const char *, bool);
struct xbps_repo HIDDEN *xbps_regget_repo(struct xbps_handle *,
		const char *);
int HIDDEN xbps_conf_init(struct xbps_handle *);

#endif /* !_XBPS_API_IMPL_H_ */
