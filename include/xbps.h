/*-
 * Copyright (c) 2008-2020 Juan Romero Pardines <xtraeme@gmail.com>
 * Copyright (c) 2014-2019 Enno Boland <gottox@voidlinux.org>
 * Copyright (c) 2016-2019 Duncan Overbruck <mail@duncano.de>
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

#ifndef _XBPS_H_
#define _XBPS_H_

#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

#include <xbps/xbps_array.h>
#include <xbps/xbps_bool.h>
#include <xbps/xbps_data.h>
#include <xbps/xbps_dictionary.h>
#include <xbps/xbps_number.h>
#include <xbps/xbps_string.h>

#define XBPS_MAXPATH	512
#define XBPS_NAME_SIZE	64

/**
 * @file include/xbps.h
 * @brief XBPS Library API header
 *
 * This header documents the full API for the XBPS Library.
 */
#define XBPS_API_VERSION	"20200423"

#ifndef XBPS_VERSION
 #define XBPS_VERSION		"UNSET"
#endif
#ifndef XBPS_GIT
 #define XBPS_GIT		"UNSET"
#endif
/**
 * @def XBPS_RELVER
 * Current library release date.
 */
#define XBPS_RELVER		"XBPS: " XBPS_VERSION \
				" API: " XBPS_API_VERSION \
				" GIT: " XBPS_GIT

/**
 * @def XBPS_SYSCONF_PATH
 * Default configuration PATH to find XBPS_CONF_PLIST.
 */
#define XBPS_SYSDIR            "/xbps.d"
#ifndef XBPS_SYSCONF_PATH
# define XBPS_SYSCONF_PATH      "/etc" XBPS_SYSDIR
#endif
#ifndef XBPS_SYSDEFCONF_PATH
# define XBPS_SYSDEFCONF_PATH	"/usr/share" XBPS_SYSDIR
#endif

/** 
 * @def XBPS_META_PATH
 * Default root PATH to store metadata info.
 */
#ifndef XBPS_META_PATH
#define XBPS_META_PATH		"var/db/xbps"
#endif

/** 
 * @def XBPS_CACHE_PATH
 * Default cache PATH to store downloaded binpkgs.
 */
#define XBPS_CACHE_PATH		"var/cache/xbps"

/**
 * @def XBPS_PKGDB
 * Filename for the package database.
 */
#define XBPS_PKGDB		"pkgdb-0.38.plist"

/**
 * @def XBPS_PKGPROPS
 * Filename for package metadata property list.
 */
#define XBPS_PKGPROPS		"props.plist"

/**
 * @def XBPS_PKGFILES
 * Filename for package metadata files property list.
 */
#define XBPS_PKGFILES		"files.plist"

/** 
 * @def XBPS_REPOIDX
 * Filename for the repository index property list.
 */
#define XBPS_REPOIDX		"index.plist"

/**
 * @def XBPS_REPOIDX_META
 * Filename for the repository index metadata property list.
 */
#define XBPS_REPOIDX_META 	"index-meta.plist"

/**
 * @def XBPS_FLAG_VERBOSE
 * Verbose flag that can be used in the function callbacks to alter
 * its behaviour. Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_VERBOSE		0x00000001

/**
 * @def XBPS_FLAG_FORCE_CONFIGURE
 * Force flag used in xbps_configure_pkg(), if set the package(s)
 * will be reconfigured even if its state is XBPS_PKG_STATE_INSTALLED.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_FORCE_CONFIGURE	0x00000002

/**
 * @def XBPS_FLAG_FORCE_REMOVE_FILES
 * Force flag used in xbps_remove_pkg_files(), if set the package
 * files will be removed even if its SHA256 hash don't match.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_FORCE_REMOVE_FILES	0x00000004

/**
 * @def XBPS_FLAG_INSTALL_AUTO
 * Enabled automatic install mode for a package and all dependencies
 * installed direct and indirectly.
 */
#define XBPS_FLAG_INSTALL_AUTO		0x00000010

/**
 * @def XBPS_FLAG_DEBUG
 * Enable debug mode to output debugging printfs to stdout/stderr.
 */
#define XBPS_FLAG_DEBUG 		0x00000020

/**
 * @def XBPS_FLAG_FORCE_UNPACK
 * Force flag used in xbps_unpack_binary_pkg(). If set its package
 * files will be unpacked overwritting the current ones.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_FORCE_UNPACK 		0x00000040

/**
 * @def XBPS_FLAG_DISABLE_SYSLOG
 * Disable syslog logging, enabled by default.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_DISABLE_SYSLOG 	0x00000080

/**
 * @def XBPS_FLAG_BESTMATCH
 * Enable pkg best matching when resolving packages.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_BESTMATCH 		0x00000100

/**
 * @def XBPS_FLAG_IGNORE_CONF_REPOS
 * Ignore repos defined in configuration files.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_IGNORE_CONF_REPOS 	0x00000200

/**
 * @def XBPS_FLAG_REPOS_MEMSYNC
 * Fetch and store repodata in memory, ignoring on-disk metadata.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_REPOS_MEMSYNC 	0x00000400

/**
 * @def XBPS_FLAG_FORCE_REMOVE_REVDEPS
 * Continue with transaction even if there are broken reverse
 * dependencies, due to unresolved shared libraries or dependencies.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_FORCE_REMOVE_REVDEPS 	0x00000800

/**
 * @def XBPS_FLAG_UNPACK_ONLY
 * Do not configure packages in the transaction, just unpack them.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_UNPACK_ONLY 		0x00001000

/**
 * @def XBPS_FLAG_DOWNLOAD_ONLY
 * Only download packages to the cache, do not do any installation steps.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_DOWNLOAD_ONLY		0x00002000

/*
 * @def XBPS_FLAG_IGNORE_FILE_CONFLICTS
 * Continue with transaction even if there are file conflicts.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_IGNORE_FILE_CONFLICTS	0x00004000

/**
 * @def XBPS_FLAG_INSTALL_REPRO
 * Enabled reproducible mode; skips adding the "install-date"
 * and "repository" objs into pkgdb.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_INSTALL_REPRO		0x00008000

/**
 * @def XBPS_FLAG_KEEP_CONFIG
 * Don't overwrite configuration files that have not changed since
 * installation.
 * Must be set through the xbps_handle::flags member.
 */
#define XBPS_FLAG_KEEP_CONFIG 		0x00010000

/**
 * @def XBPS_FETCH_CACHECONN
 * Default (global) limit of cached connections used in libfetch.
 */
#define XBPS_FETCH_CACHECONN            32

/**
 * @def XBPS_FETCH_CACHECONN_HOST
 * Default (per host) limit of cached connections used in libfetch.
 */
#define XBPS_FETCH_CACHECONN_HOST       16

/**
 * @def XBPS_FETCH_TIMEOUT
 * Default timeout limit (in seconds) to wait for stalled connections.
 */
#define XBPS_FETCH_TIMEOUT		30

/**
 * @def XBPS_SHA256_DIGEST_SIZE
 * The size for a binary SHA256 digests.
 */
#define XBPS_SHA256_DIGEST_SIZE		32

/**
 * @def XBPS_SHA256_SIZE
 * The size for a hex string SHA256 hash.
 */
#define XBPS_SHA256_SIZE		(XBPS_SHA256_DIGEST_SIZE*2)+1

#ifdef __cplusplus
extern "C" {
#endif

/** @addtogroup initend */ 
/**@{*/

/**
 * @enum xbps_state_t
 *
 * Integer representing the xbps callback returned state. Possible values:
 *
 * - XBPS_STATE_UKKNOWN: state hasn't been prepared or unknown error.
 * - XBPS_STATE_TRANS_DOWNLOAD: transaction is downloading binary packages.
 * - XBPS_STATE_TRANS_VERIFY: transaction is verifying binary package integrity.
 * - XBPS_STATE_TRANS_RUN: transaction is performing operations: install, update, remove, replace.
 * - XBPS_STATE_TRANS_CONFIGURE: transaction is configuring all unpacked packages.
 * - XBPS_STATE_TRANS_FAIL: transaction has failed.
 * - XBPS_STATE_DOWNLOAD: a binary package is being downloaded.
 * - XBPS_STATE_VERIFY: a binary package is being verified.
 * - XBPS_STATE_REMOVE: a package is being removed.
 * - XBPS_STATE_REMOVE_DONE: a package has been removed successfully.
 * - XBPS_STATE_REMOVE_FILE: a package file is being removed.
 * - XBPS_STATE_REMOVE_OBSOLETE: an obsolete package file is being removed.
 * - XBPS_STATE_REPLACE: a package is being replaced.
 * - XBPS_STATE_INSTALL: a package is being installed.
 * - XBPS_STATE_INSTALL_DONE: a package has been installed successfully.
 * - XBPS_STATE_UPDATE: a package is being updated.
 * - XBPS_STATE_UPDATE_DONE: a package has been updated successfully.
 * - XBPS_STATE_UNPACK: a package is being unpacked.
 * - XBPS_STATE_CONFIGURE: a package is being configured.
 * - XBPS_STATE_CONFIGURE_DONE: a package has been configured successfully.
 * - XBPS_STATE_CONFIG_FILE: a package configuration file is being processed.
 * - XBPS_STATE_REPOSYNC: a remote repository's package index is being synchronized.
 * - XBPS_STATE_VERIFY_FAIL: binary package integrity has failed.
 * - XBPS_STATE_DOWNLOAD_FAIL: binary package download has failed.
 * - XBPS_STATE_REMOVE_FAIL: a package removal has failed.
 * - XBPS_STATE_REMOVE_FILE_FAIL: a package file removal has failed.
 * - XBPS_STATE_REMOVE_FILE_HASH_FAIL: a package file removal has failed due to hash.
 * - XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL: an obsolete package file removal has failed.
 * - XBPS_STATE_CONFIGURE_FAIL: package configure has failed.
 * - XBPS_STATE_CONFIG_FILE_FAIL: package configuration file operation has failed.
 * - XBPS_STATE_UPDATE_FAIL: package update has failed.
 * - XBPS_STATE_UNPACK_FAIL: package unpack has failed.
 * - XBPS_STATE_REPOSYNC_FAIL: syncing remote repositories has failed.
 * - XBPS_STATE_REPO_KEY_IMPORT: repository is signed and needs to import pubkey.
 * - XBPS_STATE_INVALID_DEP: package has an invalid dependency.
 * - XBPS_STATE_SHOW_INSTALL_MSG: package must show a post-install message.
 * - XBPS_STATE_SHOW_REMOVE_MSG: package must show a pre-remove message.
 * - XBPS_STATE_ALTGROUP_ADDED: package has registered an alternative group.
 * - XBPS_STATE_ALTGROUP_REMOVED: package has unregistered an alternative group.
 * - XBPS_STATE_ALTGROUP_SWITCHED: alternative group has been switched.
 * - XBPS_STATE_ALTGROUP_LINK_ADDED: link added by an alternative group.
 * - XBPS_STATE_ALTGROUP_LINK_REMOVED: link removed by an alternative group.
 * - XBPS_STATE_UNPACK_FILE_PRESERVED: package unpack preserved a file.
 * - XBPS_STATE_PKGDB: pkgdb upgrade in progress.
 * - XBPS_STATE_PKGDB_DONE: pkgdb has been upgraded successfully.
 */
typedef enum xbps_state {
	XBPS_STATE_UNKNOWN = 0,
	XBPS_STATE_TRANS_DOWNLOAD,
	XBPS_STATE_TRANS_VERIFY,
	XBPS_STATE_TRANS_FILES,
	XBPS_STATE_TRANS_RUN,
	XBPS_STATE_TRANS_CONFIGURE,
	XBPS_STATE_TRANS_FAIL,
	XBPS_STATE_DOWNLOAD,
	XBPS_STATE_VERIFY,
	XBPS_STATE_FILES,
	XBPS_STATE_REMOVE,
	XBPS_STATE_REMOVE_DONE,
	XBPS_STATE_REMOVE_FILE,
	XBPS_STATE_REMOVE_FILE_OBSOLETE,
	XBPS_STATE_PURGE,
	XBPS_STATE_PURGE_DONE,
	XBPS_STATE_REPLACE,
	XBPS_STATE_INSTALL,
	XBPS_STATE_INSTALL_DONE,
	XBPS_STATE_UPDATE,
	XBPS_STATE_UPDATE_DONE,
	XBPS_STATE_UNPACK,
	XBPS_STATE_CONFIGURE,
	XBPS_STATE_CONFIG_FILE,
	XBPS_STATE_REPOSYNC,
	XBPS_STATE_VERIFY_FAIL,
	XBPS_STATE_FILES_FAIL,
	XBPS_STATE_DOWNLOAD_FAIL,
	XBPS_STATE_REMOVE_FAIL,
	XBPS_STATE_REMOVE_FILE_FAIL,
	XBPS_STATE_REMOVE_FILE_HASH_FAIL,
	XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL,
	XBPS_STATE_PURGE_FAIL,
	XBPS_STATE_CONFIGURE_FAIL,
	XBPS_STATE_CONFIG_FILE_FAIL,
	XBPS_STATE_UPDATE_FAIL,
	XBPS_STATE_UNPACK_FAIL,
	XBPS_STATE_REPOSYNC_FAIL,
	XBPS_STATE_CONFIGURE_DONE,
	XBPS_STATE_REPO_KEY_IMPORT,
	XBPS_STATE_INVALID_DEP,
	XBPS_STATE_SHOW_INSTALL_MSG,
	XBPS_STATE_SHOW_REMOVE_MSG,
	XBPS_STATE_UNPACK_FILE_PRESERVED,
	XBPS_STATE_PKGDB,
	XBPS_STATE_PKGDB_DONE,
	XBPS_STATE_TRANS_ADDPKG,
	XBPS_STATE_ALTGROUP_ADDED,
	XBPS_STATE_ALTGROUP_REMOVED,
	XBPS_STATE_ALTGROUP_SWITCHED,
	XBPS_STATE_ALTGROUP_LINK_ADDED,
	XBPS_STATE_ALTGROUP_LINK_REMOVED
} xbps_state_t;

/**
 * @struct xbps_state_cb_data xbps.h "xbps.h"
 * @brief Structure to be passed as argument to the state function callback.
 * All members are read-only and set internally by libxbps.
 */
struct xbps_state_cb_data {
	/**
	 * @var xhp
	 *
	 * Pointer to our struct xbps_handle passed to xbps_init().
	 */
	struct xbps_handle *xhp;
	/**
	 * @var desc
	 *
	 * Current state string description.
	 */
	const char *desc;
	/**
	 * @var arg
	 *
	 * State string argument. String set on this
	 * variable may change depending on \a state.
	 */
	const char *arg;
	/**
	 * @var err
	 *
	 * Current state error value (set internally, read-only).
	 */
	int err;
	/**
	 * @var state
	 *
	 * Current state.
	 */
	xbps_state_t state;
};

/**
 * @struct xbps_fetch_cb_data xbps.h "xbps.h"
 * @brief Structure to be passed to the fetch function callback.
 *
 * This structure is passed as argument to the fetch progress function
 * callback and its members will be updated when there's any progress.
 * All members marked as read-only in this struct are set internally by
 * xbps_unpack_binary_pkg() and shouldn't be modified in the passed
 * function callback.
 */
struct xbps_fetch_cb_data {
	/**
	 * @var xhp
	 *
	 * Pointer to our struct xbps_handle passed to xbps_init().
	 */
	struct xbps_handle *xhp;
	/**
	 * @var file_size
	 *
	 * Filename size for the file to be fetched.
	 */
	off_t file_size;
	/**
	 * @var file_offset
	 *
	 * Current offset for the filename being fetched.
	 */
	off_t file_offset;
	/**
	 * @var file_dloaded
	 *
	 * Bytes downloaded for the file being fetched.
	 */
	off_t file_dloaded;
	/**
	 * @var file_name
	 *
	 * File name being fetched.
	 */
	const char *file_name;
	/**
	 * @var cb_start
	 *
	 * If true the function callback should be prepared to start
	 * the transfer progress.
	 */
	bool cb_start;
	/**
	 * @var cb_update
	 *
	 * If true the function callback should be prepared to
	 * update the transfer progress.
	 */
	bool cb_update;
	/**
	 * @var cb_end
	 *
	 * If true the function callback should be prepated to
	 * end the transfer progress.
	 */
	bool cb_end;
};

/**
 * @struct xbps_unpack_cb_data xbps.h "xbps.h"
 * @brief Structure to be passed to the unpack function callback.
 *
 * This structure is passed as argument to the unpack progress function
 * callback and its members will be updated when there's any progress.
 * All members in this struct are set internally by libxbps
 * and should be used in read-only mode in the supplied function
 * callback.
 */
struct xbps_unpack_cb_data {
	/**
	 * @var xhp
	 *
	 * Pointer to our struct xbps_handle passed to xbps_init().
	 */
	struct xbps_handle *xhp;
	/**
	 * @var pkgver
	 *
	 * Package name/version string of package being unpacked.
	 */
	const char *pkgver;
	/**
	 * @var entry
	 *
	 * Entry pathname string.
	 */
	const char *entry;
	/**
	 * @var entry_size
	 *
	 * Entry file size.
	 */
	int64_t entry_size;
	/**
	 * @var entry_extract_count
	 *
	 * Total number of extracted entries.
	 */
	ssize_t entry_extract_count;
	/**
	 * @var entry_total_count
	 *
	 * Total number of entries in package.
	 */
	ssize_t entry_total_count;
	/**
	 * @var entry_is_conf
	 *
	 * If true "entry" is a configuration file.
	 */
	bool entry_is_conf;
};

/**
 * @struct xbps_handle xbps.h "xbps.h"
 * @brief Generic XBPS structure handler for initialization.
 *
 * This structure sets some global properties for libxbps, to set some
 * function callbacks and data to the fetch, transaction and unpack functions,
 * the root and cache directory, flags, etc.
 */
struct xbps_handle {
	/**
	 * @private
	 */
	xbps_array_t preserved_files;
	xbps_array_t ignored_pkgs;
	xbps_array_t noextract;
	/**
	 * @var repositories
	 *
	 * Proplib array of strings with repositories, overriding the list
	 * in the configuration file.
	 */
	xbps_array_t repositories;
	/**
	 * @private
	 */
	xbps_dictionary_t pkgdb_revdeps;
	xbps_dictionary_t vpkgd;
	xbps_dictionary_t vpkgd_conf;
	/**
	 * @var pkgdb
	 *
	 * Proplib dictionary with the master package database
	 * stored in XBPS_META_PATH/XBPS_PKGDB.
	 */
	xbps_dictionary_t pkgdb;
	/**
	 * @var transd
	 *
	 * Proplib dictionary with transaction objects, required by
	 * xbps_transaction_commit().
	 */
	xbps_dictionary_t transd;
	/**
	 * Pointer to the supplifed function callback to be used
	 * in the XBPS possible states.
	 */
	int (*state_cb)(const struct xbps_state_cb_data *, void *);
	/**
	 * @var state_cb_data
	 *
	 * Pointer to user supplied data to be passed as argument to
	 * the \a xbps_state_cb function callback.
	 */
	void *state_cb_data;
	/**
	 * Pointer to the supplied function callback to be used in
	 * xbps_unpack_binary_pkg().
	 */
	void (*unpack_cb)(const struct xbps_unpack_cb_data *, void *);
	/**
	 * @var unpack_cb_data
	 *
	 * Pointer to user supplied data to be passed as argument to
	 * the \a xbps_unpack_cb function callback.
	 */
	void *unpack_cb_data;
	/**
	 * Pointer to the supplied function callback to be used in
	 * xbps_fetch_file().
	 */
	void (*fetch_cb)(const struct xbps_fetch_cb_data *, void *);
	/**
	 * @var fetch_cb_data
	 *
	 * Pointer to user supplied data to be passed as argument to
	 * the \a xbps_fetch_cb function callback.
	 */
	void *fetch_cb_data;
	/**
	 * @var pkgdb_plist;
	 *
	 * Absolute pathname to the pkgdb plist file.
	 */
	char *pkgdb_plist;
	/**
	 * @var target_arch
	 *
	 * Target architecture, as set by XBPS_TARGET_ARCH from environment.
	 */
	const char *target_arch;
	/**
	 * @var confdir
	 *
	 * Full path to the xbps configuration directory.
	 */
	char confdir[XBPS_MAXPATH+sizeof(XBPS_SYSCONF_PATH)];
	/**
	 * @var confdir
	 *
	 * Full path to the xbps configuration directory.
	 */
	char sysconfdir[XBPS_MAXPATH+sizeof(XBPS_SYSDEFCONF_PATH)];
	/**
	 * @var rootdir
	 *
	 * Root directory for all operations in XBPS.
	 * If unset,  defaults to '/'.
	 */
	char rootdir[XBPS_MAXPATH];
	/**
	 * @var cachedir
	 *
	 * Cache directory to store downloaded binary packages.
	 * If unset, defaults to \a XBPS_CACHE_PATH (relative to rootdir).
	 */
	char cachedir[XBPS_MAXPATH+sizeof(XBPS_CACHE_PATH)];
	/**
	 * @var metadir
	 *
	 * Metadata directory for all operations in XBPS.
	 * If unset, defaults to \a XBPS_CACHE_PATH (relative to rootdir).
	 */
	char metadir[XBPS_MAXPATH+sizeof(XBPS_META_PATH)];
	/**
	 * @var native_arch
	 *
	 * Machine architecture, defaults to uname(2) machine
	 * if XBPS_ARCH is not set from environment.
	 */
	char native_arch[64];
	/**
	 * @var flags
	 *
	 * Flags to be set globally by ORing them, possible value:
	 *
	 * 	- XBPS_FLAG_* (see above)
	 */
	int flags;
};

extern int xbps_debug_level;

void xbps_dbg_printf(const char *, ...) __attribute__ ((format (printf, 1, 2)));
void xbps_dbg_printf_append(const char *, ...)__attribute__ ((format (printf, 1, 2)));
void xbps_error_printf(const char *, ...)__attribute__ ((format (printf, 1, 2)));
void xbps_warn_printf(const char *, ...)__attribute__ ((format (printf, 1, 2)));

/**
 * Initialize the XBPS library with the following steps:
 *
 *   - Set function callbacks for fetching and unpacking.
 *   - Set default cache connections for libfetch.
 *   - Parse configuration file.
 *
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @note It's assumed that \a xhp is a valid pointer.
 *
 * @return 0 on success, an errno value otherwise.
 */
int xbps_init(struct xbps_handle *xhp);

/**
 * Releases all resources used by libxbps.
 *
 * @param[in] xhp Pointer to an xbps_handle struct.
 */
void xbps_end(struct xbps_handle *xhp);

/**@}*/

/** @addtogroup configure */
/**@{*/

/**
 * Configure (or force reconfiguration of) a package.
 *
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] pkgname Package name to configure.
 * @param[in] check_state Set it to true to check that package is
 * in unpacked state.
 * @param[in] update Set it to true if this package is being updated.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_configure_pkg(struct xbps_handle *xhp, const char *pkgname,
		bool check_state, bool update);

/**
 * Configure (or force reconfiguration of) all packages.
 *
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] ignpkgs Proplib array of strings with pkgname or pkgvers to ignore.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_configure_packages(struct xbps_handle *xhp, xbps_array_t ignpkgs);

/**@}*/

/** @addtogroup download */
/**@{*/

/**
 * Download a file from a remote URL to current working directory.
 * 
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] uri Remote URI string.
 * @param[in] flags Flags passed to libfetch's fetchXget().
 * 
 * @return -1 on error, 0 if not downloaded (because local/remote size/mtime
 * do not match) and 1 if downloaded successfully.
 **/
int xbps_fetch_file(struct xbps_handle *xhp, const char *uri,
		    const char *flags);

/**
 * Download and digest a file from a remote URL to current working directory.
 * 
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] uri Remote URI string.
 * @param[in] flags Flags passed to libfetch's fetchXget().
 * @param[out] digest SHA256 digest buffer for the downloaded file or NULL.
 * @param[in] digestlen Size of \a digest if specified; must be at least
 * XBPS_SHA256_DIGEST_SIZE.
 * 
 * @return -1 on error, 0 if not downloaded (because local/remote size/mtime
 * do not match) and 1 if downloaded successfully.
 **/
int xbps_fetch_file_sha256(struct xbps_handle *xhp, const char *uri,
		           const char *flags, unsigned char *digest,
			   size_t digestlen);

/**
 * Download a file from a remote URL to current working directory,
 * and writing file to \a filename.
 * 
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] uri Remote URI string.
 * @param[in] filename Local filename to safe the file
 * @param[in] flags Flags passed to libfetch's fetchXget().
 * 
 * @return -1 on error, 0 if not downloaded (because local/remote size/mtime
 * do not match) and 1 if downloaded successfully.
 **/
int xbps_fetch_file_dest(struct xbps_handle *xhp, const char *uri,
		    const char *filename, const char *flags);

/**
 * Download and digest a file from a remote URL to current working directory,
 * and writing file to \a filename.
 * 
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] uri Remote URI string.
 * @param[in] filename Local filename to safe the file
 * @param[in] flags Flags passed to libfetch's fetchXget().
 * @param[out] digest SHA256 digest buffer of the downloaded file or NULL.
 * @param[in] digestlen Size of \a digest if specified; must be at least
 * XBPS_SHA256_DIGEST_SIZE.
 * 
 * @return -1 on error, 0 if not downloaded (because local/remote size/mtime
 * do not match) and 1 if downloaded successfully.
 **/
int xbps_fetch_file_dest_sha256(struct xbps_handle *xhp, const char *uri,
				const char *filename, const char *flags,
				unsigned char *digest, size_t digestlen);

/**
 * Returns last error string reported by xbps_fetch_file().
 *
 * @return A string with the appropiate error message.
 */
const char *xbps_fetch_error_string(void);

/**@}*/

/**
 * @ingroup pkg_orphans
 *
 * Finds all package orphans currently installed.
 *
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] orphans Proplib array of strings with package names of
 * packages that should be treated as they were already removed (optional).
 *
 * @return A proplib array of dictionaries with all orphans found,
 * on error NULL is returned and errno is set appropiately.
 */
xbps_array_t xbps_find_pkg_orphans(struct xbps_handle *xhp, xbps_array_t orphans);

/** @addtogroup pkgdb */
/**@{*/

/**
 * Locks the pkgdb to allow a write transaction.
 *
 * This routine should be called before a write transaction is the target:
 * install, remove or update.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @return 0 on success, otherwise an errno value.
 */
int xbps_pkgdb_lock(struct xbps_handle *xhp);

/**
 * Unlocks the pkgdb after a write transaction.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 */
void xbps_pkgdb_unlock(struct xbps_handle *xhp);

/**
 * Executes a function callback per a package dictionary registered
 * in the package database (pkgdb) plist.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] fn Function callback to run for any pkg dictionary.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise
 * the value returned by the function callback.
 */
int xbps_pkgdb_foreach_cb(struct xbps_handle *xhp,
	int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
	void *arg);

/**
 * Executes a function callback per a package dictionary registered
 * in the package database (pkgdb) plist.
 *
 * This is a multithreaded implementation spawning a thread per core. Each
 * thread processes a fraction of total objects in the pkgdb dictionary.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] fn Function callback to run for any pkg dictionary.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise
 * the value returned by the function callback.
 */
int xbps_pkgdb_foreach_cb_multi(struct xbps_handle *xhp,
	int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
	void *arg);

/**
 * Returns a package dictionary from the package database (pkgdb),
 * matching pkgname or pkgver object in \a pkg.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package name or name-version to match.
 *
 * @return The matching proplib package dictionary, NULL otherwise.
 */
xbps_dictionary_t xbps_pkgdb_get_pkg(struct xbps_handle *xhp,
				     const char *pkg);

/**
 * Returns a package dictionary from the package database (pkgdb),
 * matching virtual pkgname or pkgver object in \a pkg.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package name or name-version to match.
 *
 * @return The matching proplib package dictionary, NULL otherwise.
 */
xbps_dictionary_t xbps_pkgdb_get_virtualpkg(struct xbps_handle *xhp,
					    const char *pkg);

/**
 * Returns the package dictionary with all files for \a pkg.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package expression to match.
 *
 * @return The matching package dictionary, NULL otherwise.
 */
xbps_dictionary_t xbps_pkgdb_get_pkg_files(struct xbps_handle *xhp,
					   const char *pkg);

/**
 * Returns a proplib array of strings with reverse dependencies
 * for \a pkg. The array is generated dynamically based on the list
 * of packages currently installed.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package expression to match.
 *
 * @return A proplib array of strings with reverse dependencies for \a pkg,
 * NULL otherwise.
 */
xbps_array_t xbps_pkgdb_get_pkg_revdeps(struct xbps_handle *xhp,
					const char *pkg);

/**
 * Returns a proplib array of strings with a proper sorted list
 * of packages of a full dependency graph for \a pkg.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package expression to match.
 *
 * @return A proplib array of strings with the full dependency graph for \a pkg,
 * NULL otherwise.
 */
xbps_array_t xbps_pkgdb_get_pkg_fulldeptree(struct xbps_handle *xhp,
					const char *pkg);

/**
 * Updates the package database (pkgdb) with new contents from the
 * cached memory copy to disk.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] flush If true the pkgdb plist contents in memory will
 * be flushed atomically to storage.
 * @param[in] update If true, the pkgdb plist stored on disk will be re-read
 * and the in memory copy will be refreshed.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_pkgdb_update(struct xbps_handle *xhp, bool flush, bool update);

/**
 * Creates a temporary file and executes it in rootdir.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] blob The buffer pointer where the data is stored.
 * @param[in] blobsiz The size of the buffer data.
 * @param[in] pkgver The package name/version associated.
 * @param[in] action The action to execute on the temporary file.
 * @param[in] update Set to true if package is being updated.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_pkg_exec_buffer(struct xbps_handle *xhp,
			 const void *blob,
			 const size_t blobsiz,
			 const char *pkgver,
			 const char *action,
			 bool update);

/**
 * Creates a temporary file and executes it in rootdir.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] d Package dictionary where the script data is stored.
 * @param[in] script Key associated with the script in dictionary.
 * @param[in] action The action to execute on the temporary file.
 * @param[in] update Set to true if package is being updated.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_pkg_exec_script(struct xbps_handle *xhp,
			 xbps_dictionary_t d,
			 const char *script,
			 const char *action,
			 bool update);

/**@}*/

/** @addtogroup alternatives */
/**@{*/

/**
 * Sets all alternatives provided by this \a pkg, or only those
 * set by a \a group.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package name to match.
 * @param[in] group Alternatives group to match.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_alternatives_set(struct xbps_handle *xhp, const char *pkg, const char *group);

/**
 * Registers all alternative groups provided by a package.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkgd Package dictionary as stored in the transaction dictionary.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_alternatives_register(struct xbps_handle *xhp, xbps_dictionary_t pkgd);

/**
 * Unregisters all alternative groups provided by a package.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkgd Package dictionary as stored in the transaction dictionary.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_alternatives_unregister(struct xbps_handle *xhp, xbps_dictionary_t pkgd);

/**@}*/

/** @addtogroup plist */
/**@{*/

/**
 * Executes a function callback (\a fn) per object in the proplib array \a array.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] array The proplib array to traverse.
 * @param[in] dict The dictionary associated with the array.
 * @param[in] fn Function callback to run for any pkg dictionary.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise
 * the value returned by the function callback.
 */
int xbps_array_foreach_cb(struct xbps_handle *xhp,
		xbps_array_t array,
		xbps_dictionary_t dict,
		int (*fn)(struct xbps_handle *, xbps_object_t obj, const char *, void *arg, bool *done),
		void *arg);

/**
 * Executes a function callback (\a fn) per object in the proplib array \a array.
 * This is a multithreaded implementation spawning a thread per core. Each
 * thread processes a fraction of total objects in the array.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] array The proplib array to traverse.
 * @param[in] dict The dictionary associated with the array.
 * @param[in] fn Function callback to run for any pkg dictionary.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise
 * the value returned by the function callback.
 */
int xbps_array_foreach_cb_multi(struct xbps_handle *xhp,
		xbps_array_t array,
		xbps_dictionary_t dict,
		int (*fn)(struct xbps_handle *, xbps_object_t obj, const char *, void *arg, bool *done),
		void *arg);

/**
 * Match a virtual package name or pattern by looking at proplib array
 * of strings.
 *
 * @param[in] array Proplib array of strings.
 * @param[in] str Virtual package name or package pattern to match.
 *
 * @return True if \a str matches a virtual package in \a array, false
 * otherwise.
 */
bool xbps_match_virtual_pkg_in_array(xbps_array_t array, const char *str);

/**
 * Match a virtual package name or pattern by looking at package's
 * dictionary "provides" array object.
 *
 * @param[in] pkgd Package dictionary.
 * @param[in] str Virtual package name or package pattern to match.
 *
 * @return True if \a str matches a virtual package in \a pkgd, false
 * otherwise.
 */
bool xbps_match_virtual_pkg_in_dict(xbps_dictionary_t pkgd, const char *str);

/**
 * Match any virtual package from array \a provides in they array \a rundeps
 * with dependencies.
 *
 * @param[in] rundeps Proplib array with dependencies as strings, i.e foo>=2.0.
 * @param[in] provides Proplib array of strings with virtual pkgdeps, i.e
 * foo-1.0 blah-2.0.
 *
 * @return True if \a any virtualpkg has been matched, false otherwise.
 */
bool xbps_match_any_virtualpkg_in_rundeps(xbps_array_t rundeps, xbps_array_t provides);

/**
 * Match a package name in the specified array of strings.
 *
 * @param[in] array The proplib array to search on.
 * @param[in] pkgname The package name to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_pkgname_in_array(xbps_array_t array, const char *pkgname);

/**
 * Match a package name/version in the specified array of strings with pkgnames.
 *
 * @param[in] array The proplib array to search on.
 * @param[in] pkgver The package name/version to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_pkgver_in_array(xbps_array_t array, const char *pkgver);

/**
 * Match a package pattern in the specified array of strings.
 *
 * @param[in] array The proplib array to search on.
 * @param[in] pattern The package pattern to match, i.e `foo>=0' or `foo<1'.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_pkgpattern_in_array(xbps_array_t array, const char *pattern);

/**
 * Match a package dependency against any package pattern in the specified
 * array of strings.
 *
 * @param[in] array The proplib array to search on.
 * @param[in] pkgver The package name-version to match, i.e `foo-1.0_1'.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_pkgdep_in_array(xbps_array_t array, const char *pkgver);

/**
 * Match a string (exact match) in the specified array of strings.
 *
 * @param[in] array The proplib array to search on.
 * @param[in] val The string to be matched.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_string_in_array(xbps_array_t array, const char *val);

/**
 * Returns a proplib object iterator associated with an array, contained
 * in a proplib dictionary matching the specified key.
 *
 * @param[in] dict Proplib dictionary where to look for the array.
 * @param[in] key Key associated with the array.
 *
 * @return A proplib object iterator on success, NULL otherwise and
 * errno is set appropiately.
 */
xbps_object_iterator_t xbps_array_iter_from_dict(xbps_dictionary_t dict, const char *key);

/**@}*/

/** @addtogroup transaction */
/**@{*/

/**
 * Finds a package by name or by pattern and enqueues it into
 * the transaction dictionary for future use. The first repository
 * matching \a pkg wins.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] pkg Package name, package/version or package pattern to match, i.e
 * `foo', `foo-1.0_1' or `foo>=1.2'.
 * @param[in] force If true, package will be queued (if \a str matches)
 * even if package is already installed or in hold mode.
 *
 * @return 0 on success, otherwise an errno value.
 * @retval EEXIST Package is already installed (reinstall wasn't enabled).
 * @retval ENOENT Package not matched in repository pool.
 * @retval ENOTSUP No repositories are available.
 * @retval ENXIO Package depends on invalid dependencies.
 * @retval EINVAL Any other error ocurred in the process.
 * @retval EBUSY The xbps package must be updated.
 */
int xbps_transaction_install_pkg(struct xbps_handle *xhp, const char *pkg, bool force);

/**
 * Marks a package as "going to be updated" in the transaction dictionary.
 * The first repository that contains an updated version wins.
 *
 * If bestmaching is enabled (see \a XBPS_FLAG_BESTMATCH),
 * all repositories in the pool will be used, and newest version
 * available will be enqueued if it's greater than current installed
 * version.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] pkgname The package name to update.
 * @param[in] force If true, package will be queued (if \a str matches)
 * even if package is already installed or in hold mode.
 *
 * @return 0 on success, otherwise an errno value.
 * @retval EEXIST Package is already up-to-date.
 * @retval ENOENT Package not matched in repository pool.
 * @retval ENOTSUP No repositories are available.
 * @retval ENXIO Package depends on invalid dependencies.
 * @retval EINVAL Any other error ocurred in the process.
 * @retval EBUSY The xbps package must be updated.
 */
int xbps_transaction_update_pkg(struct xbps_handle *xhp, const char *pkgname, bool force);

/**
 * Finds newer versions for all installed packages by looking at the
 * repository pool. If a newer version exists, package will be enqueued
 * into the transaction dictionary.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 *
 * @return 0 on success, otherwise an errno value.
 * @retval EBUSY The xbps package must be updated.
 * @retval EEXIST All installed packages are already up-to-date.
 * @retval ENOENT No packages currently register.
 * @retval ENOTSUP No repositories currently installed.
 * @retval EINVAL Any other error ocurred in the process.
 */
int xbps_transaction_update_packages(struct xbps_handle *xhp);

/**
 * Removes a package currently installed. The package dictionary will
 * be added into the transaction dictionary.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] pkgname Package name to be removed.
 * @param[in] recursive If true, all packages that are currently depending
 * on the package to be removed, and if they are orphans, will be added.
 *
 * @retval 0 success.
 * @retval ENOENT Package is not installed.
 * @retval EEXIST Package has reverse dependencies.
 * @retval EINVAL
 * @retval ENXIO A problem ocurred in the process.
 */
int xbps_transaction_remove_pkg(struct xbps_handle *xhp, const char *pkgname, bool recursive);

/**
 * Finds all package orphans currently installed and adds them into
 * the transaction dictionary.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 *
 * @retval 0 success.
 * @retval ENOENT No package orphans were found.
 * @retval ENXIO
 * @retval EINVAL A problem ocurred in the process.
 */
int xbps_transaction_autoremove_pkgs(struct xbps_handle *xhp);

/**
 * Returns the transaction dictionary, as shown above in the image.
 * Before returning the package list is sorted in the correct order
 * and total installed/download size for the transaction is computed.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 *
 * @retval 0 success.
 * @retval ENXIO if transaction dictionary and missing deps array were not created,
 *  due to xbps_transaction_install_pkg() or xbps_transaction_update_pkg() not
 *  previously called.
 * @retval ENODEV if there are missing dependencies in transaction ("missing_deps"
 *  array of strings object in xhp->transd dictionary).
 * @retval ENOEXEC if there are unresolved shared libraries in transaction ("missing_shlibs"
 *  array of strings object in xhp->transd dictionary).
 * @retval EAGAIN if there are package conflicts in transaction ("conflicts"
 *  array of strings object in xhp->transd dictionary).
 * @retval ENOSPC Not enough free space on target rootdir to continue with the
 *  transaction.
 * @retval EINVAL There was an error sorting packages or computing the transaction
 * sizes.
 */
int xbps_transaction_prepare(struct xbps_handle *xhp);

/**
 * Commit a transaction. The transaction dictionary in xhp->transd contains all
 * steps to be executed in the transaction, as prepared by
 * xbps_transaction_prepare().
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @return 0 on success, otherwise an errno value.
 */
int xbps_transaction_commit(struct xbps_handle *xhp);

/**
 * @enum xbps_trans_type_t
 *
 * uint8_t representing the pkg transaction type in transaction dictionary.
 *
 *  - XBPS_TRANS_UNKNOWN: Unknown type
 *  - XBPS_TRANS_INSTALL: pkg will be installed
 *  - XBPS_TRANS_REINSTALL: pkg will be reinstalled
 *  - XBPS_TRANS_UPDATE: pkg will be updated
 *  - XBPS_TRANS_CONFIGURE: pkg will be configured
 *  - XBPS_TRANS_REMOVE: pkg will be removed
 *  - XBPS_TRANS_HOLD: pkg won't be updated (on hold mode)
 *  - XBPS_TRANS_DOWNLOAD: pkg will be downloaded
 */
typedef enum xbps_trans_type {
	XBPS_TRANS_UNKNOWN = 0,
	XBPS_TRANS_INSTALL,
	XBPS_TRANS_REINSTALL,
	XBPS_TRANS_UPDATE,
	XBPS_TRANS_CONFIGURE,
	XBPS_TRANS_REMOVE,
	XBPS_TRANS_HOLD,
	XBPS_TRANS_DOWNLOAD
} xbps_trans_type_t;

/**
 * Returns the transaction type associated with \a pkg_repod.
 *
 * See \a xbps_trans_type_t for possible values.
 *
 * @param[in] pkg_repod Package dictionary stored in a repository.
 *
 * @return The transaction type associated.
 */
xbps_trans_type_t xbps_transaction_pkg_type(xbps_dictionary_t pkg_repod);

/**
 * Sets the transaction type associated with \a pkg_repod.
 *
 * See \a xbps_trans_type_t for possible values.
 *
 * @param[in] pkg_repod Package dictionary stored in a repository.
 * @param[in] type The transaction type to set.
 *
 * @return Returns true on success, false otherwise.
 */

bool xbps_transaction_pkg_type_set(xbps_dictionary_t pkg_repod, xbps_trans_type_t type);

/**@}*/

/** @addtogroup plist_fetch */
/**@{*/

/**
 * Returns a buffer of a file stored in an archive locally or
 * remotely as specified in the url \a url.
 *
 * @param[in] url Full URL to binary package file (local or remote path).
 * @param[in] fname File name to match.
 *
 * @return A malloc(3)ed buffer with the contents of \a fname, NULL otherwise
 * and errno is set appropiately.
 */
char *xbps_archive_fetch_file(const char *url, const char *fname);

/**
 * Returns a file stored in an archive locally or
 * remotely as specified in the url \a url and stores it into the
 * file descriptor \a fd.
 *
 * @param[in] url Full URL to binary package file (local or remote path).
 * @param[in] fname File name to match.
 * @param[in] fd An open file descriptor to put the file into.
 *
 * @return 0 on success, an errno value otherwise.
 */
int xbps_archive_fetch_file_into_fd(const char *url, const char *fname, int fd);

/**
 * Internalizes a plist file in an archive stored locally or
 * remotely as specified in the url \a url.
 *
 * @param[in] url Full URL to binary package file (local or remote path).
 * @param[in] p Proplist file name to internalize as a dictionary.
 *
 * @return An internalized proplib dictionary, otherwise NULL and
 * errno is set appropiately.
 */
xbps_dictionary_t xbps_archive_fetch_plist(const char *url, const char *p);

/**@}*/

/** @addtogroup repopool */
/**@{*/

/**
 * @struct xbps_repo xbps.h "xbps.h"
 * @brief Repository structure
 *
 * Repository object structure registered in a private simple queue.
 * The structure contains repository data: uri and dictionaries associated.
 */
struct xbps_repo {
	/**
	 * @private
	 */
	struct {
	        struct xbps_repo *sqe_next;  /* next element */
	} entries;
	struct archive *ar;
	/**
	 * @var xhp
	 *
	 * Pointer to our xbps_handle struct passed to xbps_rpool_foreach.
	 */
	struct xbps_handle *xhp;
	/**
	 * @var idx
	 *
	 * Proplib dictionary associated with the repository index.
	 */
	xbps_dictionary_t idx;
	/**
	 * @var idxmeta
	 *
	 * Proplib dictionary associated with the repository index-meta.
	 */
	xbps_dictionary_t idxmeta;
	/**
	 * @var uri
	 * 
	 * URI string associated with repository.
	 */
	const char *uri;
	/**
	 * @private
	 */
	int fd;
	/**
	 * var is_remote
	 *
	 * True if repository is remote, false if it's a local repository.
	 */
	bool is_remote;
	/**
	 * var is_signed
	 *
	 * True if this repository has been signed, false otherwise.
	 */
	bool is_signed;
};

void xbps_rpool_release(struct xbps_handle *xhp);

/**
 * Synchronizes repository data for all remote repositories
 * as specified in the configuration file or if \a uri argument is
 * set, just sync for that repository.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] uri Repository URI to match for sync (optional).
 *
 * @return 0 on success, ENOTSUP if no repositories were found in
 * the configuration file.
 */
int xbps_rpool_sync(struct xbps_handle *xhp, const char *uri);

/**
 * Iterates over the repository pool and executes the \a fn function
 * callback passing in the void * \a arg argument to it. The bool pointer
 * argument can be used in the callbacks to stop immediately the loop if
 * set to true, otherwise it will only be stopped if it returns a
 * non-zero value.
 * Removes repos failed to open from pool. This condition causes function
 * to return error, but don't break the loop.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] fn Function callback to execute for every repository registered in
 * the pool.
 * @param[in] arg Opaque data passed in to the \a fn function callback for
 * client data.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_rpool_foreach(struct xbps_handle *xhp,
	       int (*fn)(struct xbps_repo *, void *, bool *),
	       void *arg);

/**
 * Returns a pointer to a struct xbps_repo matching \a url.
 *
 * @param[in] url Repository url to match.
 * @return The matched xbps_repo pointer, NULL otherwise.
 */
struct xbps_repo *xbps_rpool_get_repo(const char *url);

/**
 * Finds a package dictionary in the repository pool by specifying a
 * package pattern or a package name. This function does not take into
 * account virtual packages, just matches real packages.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] pkg Package pattern, exact pkg or pkg name.
 *
 * @return The package dictionary if found, NULL otherwise.
 * @note When returned dictionary is no longer needed, you must release it
 * with xbps_object_release(3).
 */
xbps_dictionary_t xbps_rpool_get_pkg(struct xbps_handle *xhp, const char *pkg);

/**
 * Finds a package dictionary in repository pool by specifying a
 * virtual package pattern or a package name.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] pkg Virtual package pattern or name to match.
 *
 * @return The package dictionary if found, NULL otherwise.
 * @note When returned dictionary is no longer needed, you must release it
 * with xbps_object_release(3).
 */
xbps_dictionary_t xbps_rpool_get_virtualpkg(struct xbps_handle *xhp, const char *pkg);

/**
 * Returns a proplib array of strings with reverse dependencies of all
 * registered repositories matching the expression \a pkg.
 *
 * @param[in] xhp Pointer to the xbps_handle structure.
 * @param[in] pkg Package expression to match in this repository index.
 *
 * @return The array of strings on success, NULL otherwise and errno is
 * set appropiately.
 */
xbps_array_t xbps_rpool_get_pkg_revdeps(struct xbps_handle *xhp, const char *pkg);

/**
 * Returns a proplib array of strings with a proper sorted list
 * of packages of a full dependency graph for \a pkg.
 *
 * @param[in] xhp The pointer to the xbps_handle struct.
 * @param[in] pkg Package expression to match.
 *
 * @return A proplib array of strings with the full dependency graph for \a pkg,
 * NULL otherwise.
 */
xbps_array_t xbps_rpool_get_pkg_fulldeptree(struct xbps_handle *xhp, const char *pkg);

/**@}*/

/** @addtogroup repo */
/**@{*/

/**
 * Stores repository \a url into the repository pool.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] url Repository URI to store.
 *
 * @return True on success, false otherwise.
 */
bool xbps_repo_store(struct xbps_handle *xhp, const char *url);

/**
 * Removes repository \a url from the repository pool.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] url Repository URI to remove.
 *
 * @return True on success, false otherwise.
 */
bool xbps_repo_remove(struct xbps_handle *xhp, const char *url);

/**
 * Creates a lock for a local repository to obtain exclusive access (write).
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] uri Repository URI to match.
 * @param[out] lockfd Lock file descriptor assigned.
 * @param[out] lockfname Lock filename assigned.
 *
 * @return True on success and lockfd/lockfname are assigned appropiately.
 * otherwise false and lockfd/lockfname aren't set.
 */
bool xbps_repo_lock(struct xbps_handle *xhp, const char *uri, int *lockfd, char **lockfname);

/**
 * Unlocks a local repository and removes its lock file.
 *
 * @param[in] lockfd Lock file descriptor.
 * @param[in] lockfname Lock filename.
 */
void xbps_repo_unlock(int lockfd, char *lockfname);

/**
 * Opens a repository and returns a xbps_repo object.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] uri Repository URI to match.
 *
 * @return The matching repository object, NULL otherwise.
 */
struct xbps_repo *xbps_repo_open(struct xbps_handle *xhp, const char *uri);

/**
 * Opens a staging repository and returns a xbps_repo object.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] uri Repository URI to match.
 *
 * @return The matching repository object, NULL otherwise.
 */
struct xbps_repo *xbps_repo_stage_open(struct xbps_handle *xhp, const char *uri);

/**
 * Opens a repository and returns a xbps_repo object.
 *
 * @param[in] xhp Pointer to the xbps_handle struct.
 * @param[in] uri Repository URI to match.
 *
 * @return The matching repository object, NULL otherwise.
 */
struct xbps_repo *xbps_repo_public_open(struct xbps_handle *xhp, const char *uri);

/**
 * Closes a repository object, its archive associated is
 * closed and those resources released.
 *
 * @param[in] repo The repository object to close.
 */
void xbps_repo_close(struct xbps_repo *repo);

/**
 * This calls xbps_repo_close() and releases all resources
 * associated with this repository object.
 *
 * @param[in] repo The repository object to release.
 */
void xbps_repo_release(struct xbps_repo *repo);

/**
 *
 * Returns a heap-allocated string with the repository local path.
 *
 * @param[in] xhp The xbps_handle object.
 * @param[in] url The repository URL to match.
 *
 * @return A heap allocated string that must be free(3)d when it's unneeded.
 */
char *xbps_repo_path(struct xbps_handle *xhp, const char *url);

/**
 *
 * Returns a heap-allocated string with the repository local path.
 *
 * @param[in] xhp The xbps_handle object.
 * @param[in] url The repository URL to match.
 * @param[in] name The repository name (stage or repodata)
 *
 * @return A heap allocated string that must be free(3)d when it's unneeded.
 */
char *xbps_repo_path_with_name(struct xbps_handle *xhp, const char *url, const char *name);

/**
 * Remotely fetch repository data and keep it in memory.
 *
 * @param[in] repo A struct xbps_repo pointer to be filled in.
 * @param[in] url Full url to the target remote repository data archive.
 *
 * @return True on success, false otherwise and errno is set appropiately.
 */
bool xbps_repo_fetch_remote(struct xbps_repo *repo, const char *url);

/**
 * Returns a pkg dictionary from a repository \a repo matching
 * the expression \a pkg.
 *
 * @param[in] repo Pointer to an xbps_repo structure.
 * @param[in] pkg Package expression to match in this repository index.
 *
 * @return The pkg dictionary on success, NULL otherwise.
 */
xbps_dictionary_t xbps_repo_get_pkg(struct xbps_repo *repo, const char *pkg);

/**
 * Returns a pkg dictionary from a repository \a repo matching
 * the expression \a pkg. On match the first package matching the virtual
 * package expression will be returned.
 *
 * @param[in] repo Pointer to an xbps_repo structure.
 * @param[in] pkg Package expression to match in this repository index.
 *
 * @return The pkg dictionary on success, NULL otherwise.
 */
xbps_dictionary_t xbps_repo_get_virtualpkg(struct xbps_repo *repo, const char *pkg);

/**
 * Returns a proplib array of strings with reverse dependencies from
 * repository \a repo matching the expression \a pkg.
 *
 * @param[in] repo Pointer to an xbps_repo structure.
 * @param[in] pkg Package expression to match in this repository index.
 *
 * @return The array of strings on success, NULL otherwise and errno is
 * set appropiately.
 */
xbps_array_t xbps_repo_get_pkg_revdeps(struct xbps_repo *repo, const char *pkg);

/**
 * Imports the RSA public key of target repository. The repository must be
 * signed properly for this to work.
 *
 * @param[in] repo Pointer to the target xbps_repo structure.
 *
 * @return 0 on success, an errno value otherwise.
 */
int xbps_repo_key_import(struct xbps_repo *repo);

/**@}*/

/** @addtogroup archive_util */
/**@{*/

/**
 * Appends a file to the \a ar archive by using a memory buffer \a buf of
 * size \a sizelen.
 *
 * @param[in] ar The archive object.
 * @param[in] buf The memory buffer to be used as file data.
 * @param[in] buflen The size of the memory buffer.
 * @param[in] fname The filename to be used for the entry.
 * @param[in] mode The mode to be used in the entry.
 * @param[in] uname The user name to be used in the entry.
 * @param[in] gname The group name to be used in the entry.
 *
 * @return 0 on success, or any negative or errno value otherwise.
 */
int xbps_archive_append_buf(struct archive *ar, const void *buf,
		const size_t buflen, const char *fname, const mode_t mode,
		const char *uname, const char *gname);

/**@}*/

/** @addtogroup pkgstates */
/**@{*/

/**
 * @enum pkg_state_t
 *
 * Integer representing a state on which a package may be. Possible
 * values for this are:
 *
 * - XBPS_PKG_STATE_UNPACKED: Package has been unpacked correctly
 * but has not been configured due to unknown reasons.
 * - XBPS_PKG_STATE_INSTALLED: Package has been installed successfully.
 * - XBPS_PKG_STATE_BROKEN: not yet used.
 * - XBPS_PKG_STATE_HALF_REMOVED: Package has been removed but not
 * completely: the purge action in REMOVE script wasn't executed, pkg
 * metadata directory still exists and is registered in package database.
 * - XBPS_PKG_STATE_NOT_INSTALLED: Package going to be installed in
 * a transaction dictionary but that has not been yet unpacked.
 */
typedef enum pkg_state {
	XBPS_PKG_STATE_UNPACKED = 1,
	XBPS_PKG_STATE_INSTALLED,
	XBPS_PKG_STATE_BROKEN,
	XBPS_PKG_STATE_HALF_REMOVED,
	XBPS_PKG_STATE_NOT_INSTALLED,
} pkg_state_t;

/**
 * Gets package state from package \a pkgname, and sets its state
 * into \a state.
 * 
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgname Package name.
 * @param[out] state Package state returned.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_pkg_state_installed(struct xbps_handle *xhp, const char *pkgname, pkg_state_t *state);

/**
 * Gets package state from a package dictionary \a dict, and sets its
 * state into \a state.
 *
 * @param[in] dict Package dictionary.
 * @param[out] state Package state returned.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_pkg_state_dictionary(xbps_dictionary_t dict, pkg_state_t *state);

/**
 * Sets package state \a state in package \a pkgname.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgver Package name/version to match.
 * @param[in] state Package state to be set.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_set_pkg_state_installed(struct xbps_handle *xhp,
				 const char *pkgver,
				 pkg_state_t state);

/**
 * Sets package state \a state in package dictionary \a dict.
 *
 * @param[in] dict Package dictionary.
 * @param[in] state Package state to be set.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_set_pkg_state_dictionary(xbps_dictionary_t dict, pkg_state_t state);

/**@}*/

/** @addtogroup util */
/**@{*/

/**
 * Removes a string object matching \a pkgname in
 * the \a array array of strings.
 *
 * @param[in] array Proplib array of strings.
 * @param[in] pkgname pkgname string object to remove.
 *
 * @return true on success, false otherwise.
 */
bool xbps_remove_pkgname_from_array(xbps_array_t array, const char *pkgname);

/**
 * Removes a string object matching \a str in the
 * \a array array of strings.
 *
 * @param[in] array Proplib array of strings.
 * @param[in] str string object to remove.
 *
 * @return true on success, false otherwise.
 */
bool xbps_remove_string_from_array(xbps_array_t array, const char *str);

/**
 * Creates a directory (and required components if necessary).
 *
 * @param[in] path Path for final directory.
 * @param[in] mode Mode for final directory (0755 if not specified).
 *
 * @return 0 on success, -1 on error and errno set appropiately.
 */
int xbps_mkpath(const char *path, mode_t mode);

/**
 * Returns a string by concatenating its variable argument list
 * as specified by the format string \a fmt.
 *
 * @param[in] fmt Format string, see printf(3).
 * @return A pointer to a malloc(3)ed string, NULL otherwise and errno
 * is set appropiately. The pointer should be free(3)d when it's
 * no longer needed.
 */
char *xbps_xasprintf(const char *fmt, ...)__attribute__ ((format (printf, 1, 2)));

/**
 * Creates a memory mapped object from file \a file into \a mmf
 * with size \a mmflen, and file size to \a filelen;
 *
 * @param[in] file Path to a file.
 * @param[out] mmf Memory mapped object.
 * @param[out] mmflen Length of memory mapped object.
 * @param[out] filelen File size length.
 *
 * @return True on success, false otherwise and errno
 * is set appropiately. The mmaped object should be munmap()ed when it's
 * not longer needed.
 */
bool xbps_mmap_file(const char *file, void **mmf, size_t *mmflen, size_t *filelen);

/**
 * Computes a sha256 hex digest into \a dst of size \a len
 * from file \a file.
 *
 * @param[out] dst Destination buffer.
 * @param[in] len Size of \a dst must be at least XBPS_SHA256_LENGTH.
 * @param[in] file The file to read.
 *
 * @return true on success, false otherwise.
 */
bool xbps_file_sha256(char *dst, size_t len, const char *file);

/**
 * Computes a sha256 binary digest into \a dst of size \a len
 * from file \a file.
 *
 * @param[out] dst Destination buffer.
 * @param[in] len Size of \a dst must be at least XBPS_SHA256_DIGEST_SIZE_LENGTH.
 * @param[in] file The file to read.
 *
 * @return true on success, false otherwise.
 */
bool xbps_file_sha256_raw(unsigned char *dst, size_t len, const char *file);

/**
 * Compares the sha256 hash of the file \a file with the sha256
 * string specified by \a sha256.
 *
 * @param[in] file Path to a file.
 * @param[in] sha256 SHA256 hash to compare.
 *
 * @return 0 if \a file and \a sha256 have the same hash, ERANGE
 * if it differs, or any other errno value on error.
 */
int xbps_file_sha256_check(const char *file, const char *sha256);

/**
 * Verifies the RSA signature \a sigfile against \a digest with the
 * RSA public-key associated in \a repo.
 *
 * @param[in] repo Repository to use with the RSA public key associated.
 * @param[in] sigfile The signature file name used to verify \a digest.
 * @param[in] digest The digest to verify.
 *
 * @return True if the signature is valid, false otherwise.
 */
bool xbps_verify_signature(struct xbps_repo *repo, const char *sigfile,
		unsigned char *digest);

/**
 * Verifies the RSA signature of \a fname with the RSA public-key associated
 * in \a repo.
 *
 * @param[in] repo Repository to use with the RSA public key associated.
 * @param[in] fname The filename to verify, the signature file must have a .sig2
 * extension, i.e `<fname>.sig2`.
 *
 * @return True if the signature is valid, false otherwise.
 */
bool xbps_verify_file_signature(struct xbps_repo *repo, const char *fname);

/**
 * Checks if a package is currently installed in pkgdb by matching \a pkg.
 * To be installed, the pkg must be in "installed" or "unpacked" state.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkg Package name, version pattern or exact pkg to match.
 *
 * @return -1 on error (errno set appropiately), 0 if \a pkg
 * didn't match installed package, 1 if \a pkg pattern fully
 * matched installed package.
 */
int xbps_pkg_is_installed(struct xbps_handle *xhp, const char *pkg);

/**
 * Checks if a package is currently ignored by matching \a pkg.
 * To be ignored, the pkg must be ignored by the users configuration.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkg Package name, version pattern or exact pkg to match.
 *
 * @return True if the package is ignored, false otherwise.
 */
bool xbps_pkg_is_ignored(struct xbps_handle *xhp, const char *pkg);

/**
 * Returns true if binary package exists in cachedir or in a local repository,
 * false otherwise.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgd Package dictionary returned by rpool.
 *
 * @return true if exists, false otherwise.
 */
bool xbps_binpkg_exists(struct xbps_handle *xhp, xbps_dictionary_t pkgd);

/**
 * Returns true if binary package and signature exists in cachedir,
 * false otherwise.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgd Package dictionary returned by rpool.
 *
 * @return true if exists, false otherwise.
 */
bool xbps_remote_binpkg_exists(struct xbps_handle *xhp, xbps_dictionary_t pkgd);

/**
 * Checks if the URI specified by \a uri is remote or local.
 *
 * @param[in] uri URI string.
 * 
 * @return true if URI is remote, false if local.
 */
bool xbps_repository_is_remote(const char *uri);

/**
 * Returns an allocated string with the full path to the binary package
 * matching \a pkgd.
 *
 * The \a pkgd dictionary must contain the following objects:
 *  - architecture (string)
 *  - pkgver (string)
 *  - repository (string)
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgd The package dictionary to match.
 *
 * @return A malloc(3)ed buffer with the full path, NULL otherwise.
 * The buffer should be free(3)d when it's no longer needed.
 */
char *xbps_repository_pkg_path(struct xbps_handle *xhp, xbps_dictionary_t pkgd);

/**
 * Put the path to the binary package \a pkgd into \a dst.
 *
 * The \a pkgd dictionary must contain the following objects:
 *  - architecture (string)
 *  - pkgver (string)
 *  - repository (string)
 *
 * @param[in] xhp Pointer to an xbps_handle struct.
 * @param[in] dst Destination buffer.
 * @param[in] dstsz Destination buffer size.
 * @param[in] pkgd Package dictionary.
 *
 * @return The length of the path or a negative errno on error.
 * @retval -EINVAL Missing required dictionary entry.
 * @retval -ENOBUFS The path would exceed the supplied \a dst buffer.
 */
ssize_t xbps_pkg_path(struct xbps_handle *xhp, char *dst, size_t dstsz, xbps_dictionary_t pkgd);

/**
 * Put the url to the binary package \a pkgd into \a dst.
 *
 * The \a pkgd dictionary must contain the following objects:
 *  - architecture (string)
 *  - pkgver (string)
 *  - repository (string)
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgd The package dictionary to match.
 *
 * @return The length of the path or a negative errno on error.
 * @retval -EINVAL Missing required dictionary entry.
 * @retval -ENOBUFS The path would exceed the supplied \a dst buffer.
 */
ssize_t xbps_pkg_url(struct xbps_handle *xhp, char *dst, size_t dstsz, xbps_dictionary_t pkgd);

/**
 * Put the url or path (if cached) to the binary package \a pkgd into \a dst.
 *
 * The \a pkgd dictionary must contain the following objects:
 *  - architecture (string)
 *  - pkgver (string)
 *  - repository (string)
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] pkgd The package dictionary to match.
 *
 * @return The length of the path or a negative errno on error.
 * @retval -EINVAL Missing required dictionary entry.
 * @retval -ENOBUFS The path would exceed the supplied \a dst buffer.
 */
ssize_t xbps_pkg_path_or_url(struct xbps_handle *xhp, char *dst, size_t dstsz, xbps_dictionary_t pkgd);

/**
 * Gets the name of a package string. Package strings are composed
 * by a @<pkgname@>/@<version@> pair and separated by the *minus*
 * sign, i.e `foo-2.0`.
 *
 * @param[out] dst Destination buffer to store result.
 * @param[in] len Length of \a dst.
 * @param[in] pkg Package version string.
 *
 * @return true on success, false otherwise.
 */
bool xbps_pkg_name(char *dst, size_t len, const char *pkg);

/**
 * Gets a the package name of a package pattern string specified by
 * the \a pattern argument.
 *
 * Package patterns are composed of the package name and
 * either a *equals* (`foo=2.0`) constraint or a *greater than* (`foo>2.0`) or
 * *greater equals* (`foo>=2.0`) or *lower than* (`foo<2.0`) or *lower equals*
 * (`foo<=2.0`) or a combination of both (`foo>=1.0<2.0`).
 *
 * @param[out] dst Destination buffer to store result.
 * @param[in] len Length of \a dst.
 * @param[in] pattern A package pattern.
 *
 * @return true on success, false otherwise.
 */
bool xbps_pkgpattern_name(char *dst, size_t len, const char *pattern);

/**
 * Gets the package version in a package string, i.e `foo-2.0`.
 * 
 * @param[in] pkg Package string.
 *
 * @return A string with the version string, NULL if it couldn't
 * find the version component.
 */
const char *xbps_pkg_version(const char *pkg);

/**
 * Gets the pkgname/version componentn of a binary package string,
 * i.e `foo-2.0_1.<arch>.xbps`.
 *
 * @param[in] pkg Package string.
 *
 * @return A pointer to a malloc(ed) string with the pkgver component,
 * NULL if it couldn't find the version component. The pointer should
 * be free(3)d when it's no longer needed.
 */
char *xbps_binpkg_pkgver(const char *pkg);

/**
 * Gets the architecture component of a binary package string,
 * i.e `<pkgver>.<arch>.xbps`.
 *
 * @param[in] pkg Package string.
 *
 * @return A pointer to a malloc(ed) string with the architecture component,
 * NULL if it couldn't find the version component. The pointer should
 * be free(3)d when it's no longer needed.
 */
char *xbps_binpkg_arch(const char *pkg);

/**
 * Gets the package version of a package pattern string specified by
 * the \a pattern argument.
 *
 * @param[in] pattern A package pattern. The same rules in
 * xbps_get_pkgpattern_name() apply here.
 *
 * @return A string with the pattern version, NULL otherwise and
 * errno is set appropiately.
 */
const char *xbps_pkgpattern_version(const char *pattern);

/**
 * Package pattern matching.
 *
 * There are 3 strategies for version matching:
 *  - simple compare: pattern equals to pkgver.
 *  - shell wildcards: see fnmatch(3).
 *  - relational dewey matching: '>' '<' '>=' '<='.
 *
 * @param[in] pkgver Package name/version, i.e `foo-1.0'.
 * @param[in] pattern Package pattern to match against \a pkgver.
 *
 * @return 1 if \a pkgver is matched against \a pattern, 0 if no match.
 */
int xbps_pkgpattern_match(const char *pkgver, const char *pattern);

/**
 * Gets the package version revision in a package string.
 *
 * @param[in] pkg Package string, i.e `foo-2.0_1`.
 *
 * @return A string with the revision number, NULL if it couldn't
 * find the revision component.
 */
const char *xbps_pkg_revision(const char *pkg);

/**
 * Returns true if provided string is valid for target architecture.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] orig Architecture to match.
 * @param[in] target If not NULL, \a orig will be matched against it
 * rather than returned value of uname(2).
 *
 * @return True on match, false otherwise.
 */
bool xbps_pkg_arch_match(struct xbps_handle *xhp, const char *orig, const char *target);

/**
 * Converts the 64 bits signed number specified in \a bytes to
 * a human parsable string buffer pointed to \a buf.
 *
 * @param[out] buf Buffer to store the resulting string. At least
 * it should have space for 6 chars.
 * @param[in] bytes 64 bits signed number to convert.
 *
 * @return A negative number is returned on error, 0 otherwise.
 */
int xbps_humanize_number(char *buf, int64_t bytes);

/**
 * Append the string \a src to the end of \a dst.
 *
 * @param[out] dst Buffer to store the resulting string.
 * @param[in] src Source string.
 * @param[in] dstsize Size of the \a dst buffer.
 *
 * @return The total length of the created string, if the return
 * value is >= \a dstsize, the output string has been truncated.
 */
size_t xbps_strlcat(char *dst, const char *src, size_t dstsize);

/**
 * Copy up to \a dstsize - 1 from the string \a src to \a dest,
 * NUL-terminating the result if \a dstsize is not 0.
 *
 * @param[out] dst Buffer to store the resulting string.
 * @param[in] src Source string.
 * @param[in] dstsize Size of the \a dst buffer.
 *
 * @return The total length of the created string, if the return
 * value is >= \a dstsize, the output string has been truncated.
 */
size_t xbps_strlcpy(char *dst, const char *src, size_t dstsize);

/**
 * Tests if pkgver is reverted by pkg
 *
 * The package version is defined by:
 * ${NAME}-{${VERSION}_${REVISION}.
 *
 * the name part is ignored.
 *
 * @param[in] pkg a package which is a candidate to revert pkgver.
 * @param[in] pkgver a package version string
 *
 * @return true if pkg reverts pkgver, false otherwise.
 */
bool xbps_pkg_reverts(xbps_dictionary_t pkg, const char *pkgver);

/**
 * Compares package version strings.
 *
 * The package version is defined by:
 * ${VERSION}[_${REVISION}].
 *
 * @param[in] pkg1 a package version string.
 * @param[in] pkg2 a package version string.
 *
 * @return -1, 0 or 1 depending if pkg1 is less than, equal to or
 * greater than pkg2.
 */
int xbps_cmpver(const char *pkg1, const char *pkg2);

/**
 * Converts a RSA public key in PEM format to a hex fingerprint.
 *
 * @param[in] pubkey The public-key in PEM format as xbps_data_t.
 *
 * @return The OpenSSH fingerprint in hexadecimal.
 * The returned buffer must be free(3)d when necessary.
 */
char *xbps_pubkey2fp(xbps_data_t pubkey);

/**
 * Returns a buffer with a sanitized path from \a src.
 * This removes multiple slashes.
 *
 * @param[in] src path component.
 *
 * @return The sanitized path in a buffer.
 * The returned buffer must be free(3)d when it's no longer necessary.
 */
char *xbps_sanitize_path(const char *src);

/**
 * Turns the path in \a path into the shortest path equivalent to \a path
 * by purely lexical processing.
 *
 * @param[in,out] path The path to clean.
 *
 * @return The length of the path or -1 on error.
 */
ssize_t xbps_path_clean(char *path);

/**
 * Returns the relative path from \a from to \a to.
 *
 * @param[out] dst Destination buffer to store result.
 * @param[in] len Length of \a dst.
 * @param[in] from The base path.
 * @param[in] to The path that becomes relative to \a from.
 *
 * @return The length of the path or -1 on error.
 */
ssize_t xbps_path_rel(char *dst, size_t len, const char *from, const char *to);

/**
 * Joins multiple path components into the \a dst buffer.
 *
 * The last argument has to be (char *)NULL.
 *
 * @param[out] dst Destination buffer to store result.
 * @param[in] len Length of \a dst.
 *
 * @return The length of the path or -1 on error.
 */
ssize_t xbps_path_join(char *dst, size_t len, ...);

/**
 * Adds \a rootdir and \a path to the \a dst buffer.
 *
 * @param[out] dst Destination buffer to store result.
 * @param[in] len Length of \a dst.
 * @param[in] suffix Suffix to append.
 *
 * @return The length of the path or -1 on error.
 */
ssize_t xbps_path_append(char *dst, size_t len, const char *suffix);

/**
 * Adds \a rootdir and \a path to the \a dst buffer.
 *
 * @param[out] dst Destination buffer to store result.
 * @param[in] len Length of \a dst.
 * @param[in] prefix Prefix to prepend.
 *
 * @return The length of the path or -1 on error.
 */
ssize_t xbps_path_prepend(char *dst, size_t len, const char *prefix);

/**
 * Returns a sanitized target file of \a path without the rootdir component.
 *
 * @param[in] xhp The pointer to an xbps_handle struct.
 * @param[in] path path component.
 * @param[in] target The stored target file of a symlink.
 *
 * @return The sanitized path in a buffer.
 * The returned buffer must be free(3)d when it's no longer necessary.
 */
char *xbps_symlink_target(struct xbps_handle *xhp, const char *path, const char *target);

/**
 * Returns true if any of the fnmatch patterns in \a patterns matches
 * and is not negated by a later match.
 *
 * @param[in] patterns The patterns to match against.
 * @param[in] path The path that is matched against the patterns.
 *
 * @return true if any pattern matches, false otherwise.
 * The returned buffer must be free(3)d when it's no longer necessary.
 */
bool xbps_patterns_match(xbps_array_t patterns, const char *path);

/**
 * Internalizes a plist file declared in \a path and returns a proplib array.
 *
 * @param[in] path The file path.
 *
 * @return The internalized proplib array, NULL otherwise.
 */
xbps_array_t
xbps_plist_array_from_file(const char *path);

/**
 * Internalizes a plist file declared in \a path and returns a proplib dictionary.
 *
 * @param[in] path The file path.
 *
 * @return The internalized proplib array, NULL otherwise.
 */
xbps_dictionary_t
xbps_plist_dictionary_from_file(const char *path);

/**@}*/

#ifdef __cplusplus
}
#endif

#endif /* !_XBPS_H_ */
