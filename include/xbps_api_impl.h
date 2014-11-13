/*-
 * Copyright (c) 2010-2014 Juan Romero Pardines.
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
			ARCHIVE_EXTRACT_SECURE_SYMLINKS
#define FEXTRACT_FLAGS	ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM | \
			ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_UNLINK | \
			EXTRACT_FLAGS

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
 * From lib/external/dewey.c
 */
int HIDDEN dewey_match(const char *, const char *);

/**
 * @private
 * From lib/pkgdb.c
 */
int HIDDEN xbps_pkgdb_init(struct xbps_handle *);
void HIDDEN xbps_pkgdb_release(struct xbps_handle *);
int HIDDEN xbps_pkgdb_conversion(struct xbps_handle *);

/**
 * @private
 * From lib/plist.c
 */
int HIDDEN xbps_array_replace_dict_by_name(xbps_array_t,
					   xbps_dictionary_t,
					   const char *);
int HIDDEN xbps_array_replace_dict_by_pattern(xbps_array_t,
					      xbps_dictionary_t,
					       const char *);

/**
 * @private
 * From lib/plist_remove.c
 */
bool HIDDEN xbps_remove_pkg_from_array_by_name(xbps_array_t, const char *);
bool HIDDEN xbps_remove_pkg_from_array_by_pattern(xbps_array_t, const char *);
bool HIDDEN xbps_remove_pkg_from_array_by_pkgver(xbps_array_t, const char *);

/**
 * @private
 * From lib/download.c
 */
void HIDDEN xbps_fetch_set_cache_connection(int, int);
void HIDDEN xbps_fetch_unset_cache_connection(void);

/**
 * @private
 * From lib/package_msg.c
 */
int HIDDEN xbps_cb_message(struct xbps_handle *, xbps_dictionary_t, const char *);

/**
 * @private
 * From lib/package_config_files.c
 */
int HIDDEN xbps_entry_is_a_conf_file(xbps_dictionary_t, const char *);
int HIDDEN xbps_entry_install_conf_file(struct xbps_handle *,
					xbps_dictionary_t,
					xbps_dictionary_t,
					struct archive_entry *,
					const char *,
					const char *);

/**
 * @private
 * From lib/repo.c
 */
void HIDDEN xbps_repo_invalidate(struct xbps_repo *);

/**
 * @private
 * From lib/repo_pkgdeps.c
 */
int HIDDEN xbps_repository_find_deps(struct xbps_handle *,
				     xbps_array_t,
				     xbps_dictionary_t);

/**
 * @private
 * From lib/plist_find.c
 */
xbps_dictionary_t HIDDEN xbps_find_pkg_in_dict(xbps_dictionary_t, const char *);
xbps_dictionary_t HIDDEN xbps_find_virtualpkg_in_dict(struct xbps_handle *, xbps_dictionary_t, const char *);
xbps_dictionary_t HIDDEN xbps_find_pkg_in_array(xbps_array_t, const char *, const char *);
xbps_dictionary_t HIDDEN xbps_find_virtualpkg_in_array(struct xbps_handle *, xbps_array_t, const char *, const char *);
/**
 * @private
 * From lib/transaction_revdeps.c
 */
void HIDDEN xbps_transaction_revdeps(struct xbps_handle *, xbps_array_t);

/**
 * @privuate
 * From lib/transaction_shlibs.c
 */
bool HIDDEN xbps_transaction_shlibs(struct xbps_handle *, xbps_array_t, xbps_array_t);

/**
 * @private
 * From lib/transaction_dictionary.c
 */
int HIDDEN xbps_transaction_init(struct xbps_handle *);

int HIDDEN xbps_transaction_store(struct xbps_handle *, xbps_array_t, xbps_dictionary_t, bool);

/**
 * @private
 * From lib/repo_sync.c
 */
char HIDDEN *xbps_get_remote_repo_string(const char *);
int HIDDEN xbps_repo_sync(struct xbps_handle *, const char *);

/**
 * @private
 * From lib/util_hash.c
 */
int HIDDEN xbps_file_hash_check_dictionary(struct xbps_handle *,
					   xbps_dictionary_t d,
					   const char *,
					   const char *);

/**
 * @private
 * From lib/external/fexec.c
 */
int HIDDEN xbps_file_exec(struct xbps_handle *, const char *, ...);

/**
 * @private
 * From lib/cb_util.c
 */
void HIDDEN xbps_set_cb_fetch(struct xbps_handle *, off_t, off_t, off_t,
			      const char *, bool, bool, bool);
int HIDDEN xbps_set_cb_state(struct xbps_handle *, xbps_state_t, int,
			     const char *, const char *, ...);

/**
 * @private
 * From lib/package_unpack.c
 */
int HIDDEN xbps_unpack_binary_pkg(struct xbps_handle *, xbps_dictionary_t);

int HIDDEN xbps_transaction_package_replace(struct xbps_handle *, xbps_array_t);

/**
 * @private
 * From lib/package_remove.c
 */
int HIDDEN xbps_remove_pkg(struct xbps_handle *, const char *, bool);

/**
 * @private
 * From lib/package_register.c
 */
int HIDDEN xbps_register_pkg(struct xbps_handle *, xbps_dictionary_t);

/**
 * @private
 * From lib/package_conflicts.c
 */
void HIDDEN xbps_pkg_find_conflicts(struct xbps_handle *,
				    xbps_array_t,
				    xbps_dictionary_t);
/**
 * @private
 * From lib/plist_find.c
 */
const char HIDDEN *vpkg_user_conf(struct xbps_handle *, const char *, bool);

#endif /* !_XBPS_API_IMPL_H_ */
