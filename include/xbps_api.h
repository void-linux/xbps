/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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

#ifndef _XBPS_API_H_
#define _XBPS_API_H_

#include <stdio.h>
#include <inttypes.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#ifndef DEBUG
#  define NDEBUG
#endif
#include <assert.h>

#include <prop/proplib.h>
#include <archive.h>
#include <archive_entry.h>

/* Current release version */
#define XBPS_RELVER		"20091207"

/* Default root PATH for xbps to store metadata info. */
#define XBPS_META_PATH		"/var/db/xbps"

/* Default cache PATH for xbps to store downloaded binpkgs. */
#define XBPS_CACHE_PATH		"/var/cache/xbps"

/* Filename for the repositories plist file. */
#define XBPS_REPOLIST		"repositories.plist"

/* Filename of the package index plist for a repository. */
#define XBPS_PKGINDEX		"pkg-index.plist"

/* Filename of the packages register. */
#define XBPS_REGPKGDB		"regpkgdb.plist"

/* Package metadata files. */
#define XBPS_PKGPROPS		"props.plist"
#define XBPS_PKGFILES		"files.plist"

/* Current version of the package index format. */
#define XBPS_PKGINDEX_VERSION	"1.1"

#define XBPS_FLAG_VERBOSE	0x00000001
#define XBPS_FLAG_FORCE		0x00000002

#define ARCHIVE_READ_BLOCKSIZE	10240

#define EXTRACT_FLAGS	ARCHIVE_EXTRACT_SECURE_NODOTDOT | \
			ARCHIVE_EXTRACT_SECURE_SYMLINKS | \
			ARCHIVE_EXTRACT_NO_OVERWRITE | \
			ARCHIVE_EXTRACT_NO_OVERWRITE_NEWER
#define FEXTRACT_FLAGS	ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM | \
			ARCHIVE_EXTRACT_TIME | EXTRACT_FLAGS

#ifndef __UNCONST
#define __UNCONST(a)	((void *)(unsigned long)(const void *)(a))
#endif

#ifdef DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#if __GNUC__ >= 4
#define SYMEXPORT __attribute__ ((visibility("default")))
#else
#define SYMEXPORT
#endif

__BEGIN_DECLS

/* From lib/config_files.c */
int		xbps_config_file_from_archive_entry(prop_dictionary_t,
						    struct archive_entry *,
						    const char *,
						    int *,
						    bool *);

/* From lib/configure.c */
int SYMEXPORT	xbps_configure_pkg(const char *, const char *, bool);
int SYMEXPORT	xbps_configure_all_pkgs(void);

/* from lib/cmpver.c */
int SYMEXPORT	xbps_cmpver(const char *, const char *);

/* From lib/download.c */
int SYMEXPORT	xbps_fetch_file(const char *, const char *, bool, const char *);
const char SYMEXPORT	*xbps_fetch_error_string(void);

/* From lib/fexec.c */
int SYMEXPORT	xbps_file_exec(const char *, ...);
int SYMEXPORT	xbps_file_exec_skipempty(const char *, ...);
int SYMEXPORT	xbps_file_chdir_exec(const char *, const char *, ...);

/* From lib/humanize_number.c */
#define HN_DECIMAL		0x01
#define HN_NOSPACE		0x02
#define HN_B			0x04
#define HN_DIVISOR_1000		0x08
#define HN_GETSCALE		0x10
#define HN_AUTOSCALE		0x20

int SYMEXPORT	xbps_humanize_number(char *, size_t, int64_t, const char *,
				     int, int);

/* From lib/mkpath.c */
int SYMEXPORT	xbps_mkpath(char *, mode_t);

/* From lib/orphans.c */
prop_array_t SYMEXPORT	xbps_find_orphan_packages(void);

/* From lib/pkgmatch.c */
int SYMEXPORT	xbps_pkgdep_match(const char *, char *);

/* From lib/plist.c */
bool SYMEXPORT	xbps_add_obj_to_dict(prop_dictionary_t, prop_object_t,
				     const char *);
bool SYMEXPORT	xbps_add_obj_to_array(prop_array_t, prop_object_t);

int SYMEXPORT	xbps_callback_array_iter_in_dict(prop_dictionary_t,
			const char *,
			int (*fn)(prop_object_t, void *, bool *),
			void *);
int SYMEXPORT	xbps_callback_array_iter_reverse_in_dict(prop_dictionary_t,
			const char *,
			int (*fn)(prop_object_t, void *, bool *),
			void *);

prop_dictionary_t SYMEXPORT	xbps_find_pkg_in_dict(prop_dictionary_t,
					      const char *, const char *);
prop_dictionary_t SYMEXPORT	xbps_find_pkg_from_plist(const char *,
						const char *);
prop_dictionary_t SYMEXPORT
		xbps_find_pkg_installed_from_plist(const char *);
bool SYMEXPORT	xbps_find_string_in_array(prop_array_t, const char *);

prop_object_iterator_t	SYMEXPORT
	xbps_get_array_iter_from_dict(prop_dictionary_t, const char *);

prop_dictionary_t SYMEXPORT
	xbps_read_dict_from_archive_entry(struct archive *,
					  struct archive_entry *);

int SYMEXPORT	xbps_remove_pkg_dict_from_file(const char *, const char *);
int SYMEXPORT
	xbps_remove_pkg_from_dict(prop_dictionary_t, const char *,
				  const char *);
int SYMEXPORT	xbps_remove_string_from_array(prop_array_t, const char *);

/* From lib/purge.c */
int SYMEXPORT	xbps_purge_pkg(const char *, bool);
int SYMEXPORT	xbps_purge_all_pkgs(void);

/* From lib/register.c */
int SYMEXPORT	xbps_register_pkg(prop_dictionary_t, bool);
int SYMEXPORT	xbps_unregister_pkg(const char *);

/* From lib/regpkgs_dictionary.c */
prop_dictionary_t SYMEXPORT	xbps_regpkgs_dictionary_init(void);
void SYMEXPORT	xbps_regpkgs_dictionary_release(void);

/* From lib/remove.c */
int SYMEXPORT	xbps_remove_pkg(const char *, const char *, bool);
int SYMEXPORT	xbps_remove_pkg_files(prop_dictionary_t, const char *);

/* From lib/remove_obsoletes.c */
int		xbps_remove_obsoletes(prop_dictionary_t, prop_dictionary_t);

/* From lib/repository.c */
int SYMEXPORT	xbps_repository_register(const char *);
int SYMEXPORT	xbps_repository_unregister(const char *);

/* From lib/repository_finddeps.c */
int SYMEXPORT	xbps_repository_find_pkg_deps(prop_dictionary_t,
					      prop_dictionary_t);

/* From lib/repository_findpkg.c */
int SYMEXPORT	xbps_repository_install_pkg(const char *);
int SYMEXPORT	xbps_repository_update_pkg(const char *, prop_dictionary_t);
int SYMEXPORT	xbps_repository_update_allpkgs(void);
prop_dictionary_t SYMEXPORT	xbps_repository_get_transaction_dict(void);

/* From lib/repository_plist.c */
char SYMEXPORT	*xbps_repository_get_path_from_pkg_dict(prop_dictionary_t,
							const char *);
prop_dictionary_t SYMEXPORT
	xbps_repository_get_pkg_plist_dict(const char *, const char *);
prop_dictionary_t SYMEXPORT
	xbps_repository_get_pkg_plist_dict_from_url(const char *, const char *);

/* From lib/repository_pool.c */
struct repository_pool {
	SIMPLEQ_ENTRY(repository_pool) chain;
	prop_dictionary_t rp_repod;
	char *rp_uri;
};
SYMEXPORT SIMPLEQ_HEAD(, repository_pool) repopool_queue;

int SYMEXPORT	xbps_repository_pool_init(void);
void SYMEXPORT	xbps_repository_pool_release(void);

/* From lib/repository_sync_index.c */
int SYMEXPORT	xbps_repository_sync_pkg_index(const char *);
char SYMEXPORT	*xbps_get_remote_repo_string(const char *);

/* From lib/requiredby.c */
int SYMEXPORT	xbps_requiredby_pkg_add(prop_array_t, prop_dictionary_t);
int SYMEXPORT	xbps_requiredby_pkg_remove(const char *);

/* From lib/sortdeps.c */
int SYMEXPORT	xbps_sort_pkg_deps(prop_dictionary_t);

/* From lib/state.c */
typedef enum pkg_state {
	XBPS_PKG_STATE_UNPACKED = 1,
	XBPS_PKG_STATE_INSTALLED,
	XBPS_PKG_STATE_BROKEN,
	XBPS_PKG_STATE_CONFIG_FILES,
	XBPS_PKG_STATE_NOT_INSTALLED
} pkg_state_t;

int SYMEXPORT	xbps_get_pkg_state_installed(const char *, pkg_state_t *);
int SYMEXPORT	xbps_get_pkg_state_dictionary(prop_dictionary_t, pkg_state_t *);
int SYMEXPORT	xbps_set_pkg_state_installed(const char *, pkg_state_t);
int SYMEXPORT	xbps_set_pkg_state_dictionary(prop_dictionary_t, pkg_state_t);

/* From lib/unpack.c */
int SYMEXPORT	xbps_unpack_binary_pkg(prop_dictionary_t);

/* From lib/util.c */
char SYMEXPORT	*xbps_xasprintf(const char *, ...);
char SYMEXPORT	*xbps_get_file_hash(const char *);
int SYMEXPORT	xbps_check_file_hash(const char *, const char *);
int SYMEXPORT	xbps_check_is_installed_pkg(const char *);
bool SYMEXPORT	xbps_check_is_installed_pkgname(const char *);
bool SYMEXPORT	xbps_check_is_repo_string_remote(const char *);
char SYMEXPORT	*xbps_get_binpkg_local_path(prop_dictionary_t, const char *);
char SYMEXPORT	*xbps_get_pkg_index_plist(const char *);
char SYMEXPORT	*xbps_get_pkg_name(const char *);
char SYMEXPORT	*xbps_get_pkgdep_name(const char *);
const char SYMEXPORT	*xbps_get_pkg_version(const char *);
const char SYMEXPORT	*xbps_get_pkgdep_version(const char *);
const char SYMEXPORT	*xbps_get_pkg_revision(const char *);
const char SYMEXPORT	*xbps_get_pkgver_from_dict(prop_dictionary_t);
bool SYMEXPORT		xbps_pkg_has_rundeps(prop_dictionary_t);
void SYMEXPORT		xbps_set_rootdir(const char *);
const char SYMEXPORT	*xbps_get_rootdir(void);
void SYMEXPORT		xbps_set_cachedir(const char *);
const char SYMEXPORT	*xbps_get_cachedir(void);
void SYMEXPORT		xbps_set_flags(int);
int SYMEXPORT		xbps_get_flags(void);
bool SYMEXPORT		xbps_yesno(const char *, ...);
bool SYMEXPORT		xbps_noyes(const char *, ...);

__END_DECLS

#endif /* !_XBPS_API_H_ */
