/*-
 * Copyright (c) 2010-2015 Juan Romero Pardines.
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
#include "compat.h"

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(uintptr_t)(const void *)(a))
#endif

#ifndef __arraycount
#define __arraycount(x) (sizeof(x) / sizeof(*x))
#endif

struct archive_entry;

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
int HIDDEN xbps_transaction_internalize(struct xbps_handle *, xbps_object_iterator_t);

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
