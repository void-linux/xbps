/*-
 * Copyright (c) 2010-2012 Juan Romero Pardines.
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

#ifndef DEBUG
#  define NDEBUG
#endif
#include <assert.h>
#include <xbps_api.h>
#include "compat.h"
#include "queue.h"
#include "fetch.h"

#define ARCHIVE_READ_BLOCKSIZE	10240

#define EXTRACT_FLAGS	ARCHIVE_EXTRACT_SECURE_NODOTDOT | \
			ARCHIVE_EXTRACT_SECURE_SYMLINKS
#define FEXTRACT_FLAGS	ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM | \
			ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_UNLINK | \
			EXTRACT_FLAGS

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))
#endif

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

__BEGIN_DECLS

/**
 * @private
 * From lib/dewey.c
 */
int HIDDEN dewey_match(const char *, const char *);

/**
 * @private
 * From lib/regpkgdb_dictionary.c
 */
int HIDDEN xbps_regpkgdb_dictionary_init(struct xbps_handle *);
void HIDDEN xbps_regpkgdb_dictionary_release(struct xbps_handle *);

/**
 * @private
 * From lib/repository_pool.c
 */
int HIDDEN xbps_repository_pool_init(struct xbps_handle *);
void HIDDEN xbps_repository_pool_release(struct xbps_handle *);

/**
 * @private
 * From lib/download.c
 */
void HIDDEN xbps_fetch_set_cache_connection(int, int);
void HIDDEN xbps_fetch_unset_cache_connection(void);

/**
 * @private
 * From lib/package_config_files.c
 */
int HIDDEN xbps_entry_is_a_conf_file(prop_dictionary_t, const char *);
int HIDDEN xbps_entry_install_conf_file(prop_dictionary_t,
					struct archive_entry *,
					const char *,
					const char *,
					const char *);
/**
 * @private
 * From lib/plist_archive_entry.c
 */
prop_dictionary_t HIDDEN
	xbps_dictionary_from_archive_entry(struct archive *,
					   struct archive_entry *);

/**
 * @private
 * From lib/package_remove_obsoletes.c
 */
int HIDDEN xbps_remove_obsoletes(const char *,
				 const char *,
				 const char *,
				 prop_dictionary_t,
				 prop_dictionary_t);

/**
 * @private
 * From lib/repository_finddeps.c
 */
int HIDDEN xbps_repository_find_pkg_deps(prop_dictionary_t,
					 prop_array_t,
					 prop_dictionary_t);

/**
 * @private
 * From lib/package_requiredby.c
 */
int HIDDEN xbps_requiredby_pkg_add(struct xbps_handle *, prop_dictionary_t);
int HIDDEN xbps_requiredby_pkg_remove(const char *);

/**
 * @private
 * From lib/transaction_sortdeps.c
 */
int HIDDEN xbps_sort_pkg_deps(void);

/**
 * @private
 * From lib/transaction_dictionary.c
 */
prop_dictionary_t HIDDEN xbps_transaction_dictionary_get(void);

/**
 * @private
 * From lib/repository_sync_index.c
 */
char HIDDEN *xbps_get_remote_repo_string(const char *);

/**
 * @private
 * From lib/fexec.c
 */
int HIDDEN xbps_file_exec(const char *, ...);
int HIDDEN xbps_file_exec_skipempty(const char *, ...);
int HIDDEN xbps_file_chdir_exec(const char *, const char *, ...);

/**
 * @private
 * From lib/transaction_package_replace.c
 */
int HIDDEN xbps_transaction_package_replace(prop_dictionary_t);

/**
 * @private
 * From lib/plist_find.c
 */
prop_dictionary_t HIDDEN
	xbps_find_virtualpkg_in_dict_by_name(prop_dictionary_t,
					     const char *,
					     const char *);
prop_dictionary_t HIDDEN
	xbps_find_virtualpkg_in_dict_by_pattern(prop_dictionary_t,
						const char *,
						const char *);
prop_dictionary_t HIDDEN
	xbps_find_virtualpkg_conf_in_array_by_name(prop_array_t, const char *);
prop_dictionary_t HIDDEN
	xbps_find_virtualpkg_conf_in_dict_by_name(prop_dictionary_t,
					     const char *,
					     const char *);
prop_dictionary_t HIDDEN
	xbps_find_virtualpkg_conf_in_array_by_pattern(prop_array_t,
						      const char *);
prop_dictionary_t HIDDEN
	xbps_find_virtualpkg_conf_in_dict_by_pattern(prop_dictionary_t,
						const char *,
						const char *);
/**
 * @private
 * From lib/cb_util.c
 */
void HIDDEN xbps_set_cb_fetch(off_t, off_t, off_t, const char *,
			      bool, bool, bool);
void HIDDEN xbps_set_cb_state(xbps_state_t, int, const char *,
			      const char *, const char *, ...);
void HIDDEN xbps_set_cb_unpack(const char *, int64_t, ssize_t,
			       ssize_t, bool, bool);

/**
 * @private
 * From lib/package_unpack.c
 */
int HIDDEN xbps_unpack_binary_pkg(prop_dictionary_t);

__END_DECLS

#endif /* !_XBPS_API_IMPL_H_ */
