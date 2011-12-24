/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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

#ifndef _XBPS_API_H_
#define _XBPS_API_H_

#include <stdio.h>
#include <inttypes.h>

#include <archive.h>
#include <archive_entry.h>
#include <prop/proplib.h>
#include <confuse.h>

#ifdef  __cplusplus
# define __BEGIN_DECLS  extern "C" {
# define __END_DECLS    }
#else
# define __BEGIN_DECLS
# define __END_DECLS
#endif

/**
 * @file include/xbps_api.h
 * @brief XBPS Library API header
 *
 * This header documents the full API for the XBPS Library.
 */

/**
 * @def XBPS_PKGINDEX_VERSION
 * Current version for the repository package index format.
 */
#define XBPS_PKGINDEX_VERSION	"1.3"

#define XBPS_API_VERSION	"20111224-2"
#define XBPS_VERSION		"0.12"

/**
 * @def XBPS_RELVER
 * Current library release date.
 */
#define XBPS_RELVER		"XBPS: " XBPS_VERSION \
				" API: " XBPS_API_VERSION \
				" INDEX: " XBPS_PKGINDEX_VERSION

/** 
 * @def XBPS_META_PATH
 * Default root PATH to store metadata info.
 */
#define XBPS_META_PATH		"var/db/xbps"

/** 
 * @def XBPS_CACHE_PATH
 * Default cache PATH to store downloaded binpkgs.
 */
#define XBPS_CACHE_PATH		"var/cache/xbps"

/** 
 * @def XBPS_REGPKGDB
 * Filename for the global package register database.
 */
#define XBPS_REGPKGDB		"regpkgdb.plist"

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
 * @def XBPS_PKGINDEX
 * Filename for the repository package index property list.
 */
#define XBPS_PKGINDEX		"index.plist"

/**
 * @def XBPS_SYSCONF_PATH
 * Default configuration PATH to find XBPS_CONF_PLIST.
 */
#define XBPS_SYSDIR            "/xbps"
#ifndef XBPS_SYSCONF_PATH
#define XBPS_SYSCONF_PATH      "/etc" XBPS_SYSDIR
#endif

/**
 * @def XBPS_CONF_PLIST
 * Filename for the XBPS plist configuration file.
 */
#define XBPS_CONF_DEF		XBPS_SYSCONF_PATH "/xbps.conf"

/**
 * @def XBPS_FLAG_VERBOSE
 * Verbose flag that can be used in the function callbacks to alter
 * its behaviour. Must be set through the xbps_init::flags member.
 */
#define XBPS_FLAG_VERBOSE		0x00000001

/**
 * @def XBPS_FLAG_FORCE_CONFIGURE
 * Force flag used in xbps_configure_pkg(), if set the package(s)
 * will be reconfigured even if its state is XBPS_PKG_STATE_INSTALLED.
 * Must be set through the xbps_init::flags member.
 */
#define XBPS_FLAG_FORCE_CONFIGURE	0x00000002

/**
 * @def XBPS_FLAG_FORCE_REMOVE_FILES
 * Force flag used in xbps_remove_pkg_files(), if set the package
 * files will be removed even if its SHA256 hash don't match.
 * Must be set through the xbps_init::flags member.
 */
#define XBPS_FLAG_FORCE_REMOVE_FILES	0x00000004

/**
 * @def XBPS_FETCH_CACHECONN
 * Default (global) limit of cached connections used in libfetch.
 */
#define XBPS_FETCH_CACHECONN            6

/**
 * @def XBPS_FETCH_CACHECONN_HOST
 * Default (per host) limit of cached connections used in libfetch.
 */
#define XBPS_FETCH_CACHECONN_HOST       2

/**
 * @def XBPS_FETCH_TIMEOUT
 * Default timeout limit (in seconds) to wait for stalled connections.
 */
#define XBPS_FETCH_TIMEOUT		30

/**
 * @def XBPS_TRANS_FLUSH
 * Default number of packages to be processed in a transaction to
 * trigger a flush to the master package database XBPS_REGPKGDB.
 */
#define XBPS_TRANS_FLUSH		5

__BEGIN_DECLS

void		xbps_dbg_printf(const char *, ...);
void		xbps_dbg_printf_append(const char *, ...);
void		xbps_error_printf(const char *, ...);
void		xbps_warn_printf(const char *, ...);

/** @addtogroup initend */ 
/*@{*/

/**
 * @enum xbps_state_t
 *
 * Integer representing the xbps callback returned state. Possible values:
 *
 * XBPS_STATE_UKKNOWN: state hasn't been prepared or unknown error.
 * XBPS_STATE_TRANS_DOWNLOAD: transaction is downloading binary packages.
 * XBPS_STATE_TRANS_VERIFY: transaction is verifying binary package integrity.
 * XBPS_STATE_TRANS_RUN: transaction is performing operations:
 * install, update, remove and replace.
 * XBPS_STATE_TRANS_CONFIGURE: transaction is configuring all
 * unpacked packages.
 * XBPS_STATE_DOWNLOAD: a binary package is being downloaded.
 * XBPS_STATE_VERIFY: a binary package is being verified.
 * XBPS_STATE_REMOVE: a package is being removed.
 * XBPS_STATE_REMOVE_DONE: a package has been removed successfully.
 * XBPS_STATE_REMOVE_FILE: a package file is being removed.
 * XBPS_STATE_REMOVE_OBSOLETE: an obsolete package file is being removed.
 * XBPS_STATE_REPLACE: a package is being replaced.
 * XBPS_STATE_INSTALL: a package is being installed.
 * XBPS_STATE_INSTALL_DONE: a package has been installed successfully.
 * XBPS_STATE_UPDATE: a package is being updated.
 * XBPS_STATE_UPDATE_DONE: a package has been updated successfully.
 * XBPS_STATE_UNPACK: a package is being unpacked.
 * XBPS_STATE_CONFIGURE: a package is being configured.
 * XBPS_STATE_CONFIG_FILE: a package configuration file is being processed.
 * XBPS_STATE_REGISTER: a package is being registered.
 * XBPS_STATE_UNREGISTER: a package is being unregistered.
 * XBPS_STATE_REPOSYNC: a remote repository's package index is being
 * synchronized.
 * XBPS_STATE_VERIFY_FAIL: binary package integrity has failed.
 * XBPS_STATE_DOWNLOAD_FAIL: binary package download has failed.
 * XBPS_STATE_REMOVE_FAIL: a package removal has failed.
 * XBPS_STATE_REMOVE_FILE_FAIL: a package file removal has failed.
 * XBPS_STATE_REMOVE_FILE_HASH_FAIL: a package file removal due to
 * its hash has failed.
 * XBPS_STATE_REMOVE_FILE_OBSOLETE_FAIL: an obsolete package file
 * removal has failed.
 * XBPS_STATE_CONFIGURE_FAIL: package configure has failed.
 * XBPS_STATE_CONFIG_FILE_FAIL: package configuration file operation
 * has failed.
 * XBPS_STATE_UPDATE_FAIL: package update has failed.
 * XBPS_STATE_UNPACK_FAIL: package unpack has failed.
 * XBPS_STATE_REGISTER_FAIL: package register has failed.
 * XBPS_STATE_UNREGISTER_FAIL: package unregister has failed.
 * XBPS_STATE_REPOSYNC_FAIL: syncing remote repositories has failed.
 */
typedef enum xbps_state {
	XBPS_STATE_UNKNOWN = 0,
	XBPS_STATE_TRANS_DOWNLOAD,
	XBPS_STATE_TRANS_VERIFY,
	XBPS_STATE_TRANS_RUN,
	XBPS_STATE_TRANS_CONFIGURE,
	XBPS_STATE_DOWNLOAD,
	XBPS_STATE_VERIFY,
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
	XBPS_STATE_REGISTER,
	XBPS_STATE_UNREGISTER,
	XBPS_STATE_REPOSYNC,
	XBPS_STATE_VERIFY_FAIL,
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
	XBPS_STATE_REGISTER_FAIL,
	XBPS_STATE_UNREGISTER_FAIL,
	XBPS_STATE_REPOSYNC_FAIL
} xbps_state_t;

/**
 * @struct xbps_cb_data xbps_api.h "xbps_api.h"
 * @brief Structure to be passed as argument to the state
 * function callbacks.
 */
struct xbps_state_cb_data {
	/**
	 * @var state
	 *
	 * Returned xbps state (set internally, read-only).
	 */
	xbps_state_t state;
	/**
	 * @var desc
	 *
	 * XBPS state string description (set internally, read-only).
	 */
	const char *desc;
	/**
	 * @var pkgname
	 *
	 * Package name string (set internally, read-only).
	 */
	const char *pkgname;
	/**
	 * @var version
	 *
	 * Package version string (set internally, read-only).
	 */
	const char *version;
	/**
	 * @var err
	 *
	 * XBPS state error value (set internally, read-only).
	 */
	int err;
};

/**
 * @struct xbps_fetch_cb_data xbps_api.h "xbps_api.h"
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
 * @struct xbps_unpack_cb_data xbps_api.h "xbps_api.h"
 * @brief Structure to be passed to the unpack function callback.
 *
 * This structure is passed as argument to the unpack progress function
 * callback and its members will be updated when there's any progress.
 * All members in this struct are set internally by xbps_unpack_binary_pkg()
 * and should be used in read-only mode in the function callback.
 * The \a cookie member can be used to pass user supplied data.
 */
struct xbps_unpack_cb_data {
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
	 * @var entry_is_metadata
	 *
	 * If true "entry" is a metadata file.
	 */
	bool entry_is_metadata;
	/**
	 * @var entry_is_conf
	 *
	 * If true "entry" is a configuration file.
	 */
	bool entry_is_conf;
};

/**
 * @struct xbps_handle xbps_api.h "xbps_api.h"
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
	cfg_t *cfg;
	/**
	 * @private regpkgdb.
	 *
	 * Internalized proplib dictionary with the registed package database
	 * stored in XBPS_META_PATH/XBPS_REGPKGDB.
	 */
	prop_dictionary_t regpkgdb;
	/**
	 * @private
	 *
	 * Array of dictionaries with all registered repositories.
	 */
	prop_array_t repo_pool;
	/**
	 * @var xbps_state_cb
	 *
	 * Pointer to the supplifed function callback to be used
	 * in the XBPS possible states.
	 */
	void (*state_cb)(const struct xbps_state_cb_data *, void *);
	/**
	 * @var state_cb_data
	 *
	 * Pointer to user supplied data to be passed as argument to
	 * the \a xbps_state_cb function callback.
	 */
	void *state_cb_data;
	/**
	 * @var xbps_unpack_cb
	 *
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
	 * @var xbps_fetch_cb
	 *
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
	 * @var rootdir
	 *
	 * Root directory for all operations in XBPS. If NULL,
	 * by default it's set to /.
	 */
	const char *rootdir;
	/**
	 * @var cachedir
	 *
	 * Cache directory to store downloaded binary packages.
	 * If NULL default value in \a XBPS_CACHE_PATH is used.
	 */
	const char *cachedir;
	/**
	 * @private
	 */
	char *cachedir_priv;
	/**
	 * @var confdir
	 *
	 * Full path to the XBPS_SYSCONF_PATH directory.
	 */
	const char *conffile;
	/**
	 * @var fetch_timeout
	 *
	 * Unsigned integer to specify libfetch's timeout limit.
	 * If not set, it defaults to 30 (in seconds). This is set internally
	 * by the API from a setting in configuration file.
	 */
	uint16_t fetch_timeout;
	/**
	 * @var transaction_frequency_flush
	 *
	 * Number of packages to be processed in a transaction to
	 * trigger a flush to the master databases.
	 */
	uint16_t transaction_frequency_flush;
	/**
	 * @var flags
	 *
	 * Flags to be set globally, possible values:
	 *
	 * - XBPS_FLAG_VERBOSE
	 * - XBPS_FLAG_FORCE
	 */
	int flags;
	/**
	 * @var debug
	 *
	 * Set to true to enable debugging messages to stderr.
	 */
	bool debug;
	/**
	 * @var install_reason_auto
	 *
	 * Set to true to make installed or updated target package
	 * (and its dependencies) marked with automatic installation,
	 * thus it will be found as orphan if no packages are depending
	 * on it.
	 */
	bool install_reason_auto;
	/**
	 * @var install_reason_manual
	 *
	 * Set to true to make installed or updated target package
	 * (and its dependencies) marked with manual installation, thus
	 * it will never will be found as orphan.
	 */
	bool install_reason_manual;
	/**
	 * @var syslog_enabled
	 *
	 * Set to true to make the client aware that some operations
	 * shall be sent to the syslog daemon if the option has been
	 * enabled in configuration file.
	 */
	bool syslog_enabled;
};

/**
 * Initialize the XBPS library with the following steps:
 *
 *   - Set function callbacks for fetching and unpacking.
 *   - Set default cache connections for libfetch.
 *   - Parse configuration file.
 *
 * @param[in] xhp The xbps_handle structure previously allocated
 * by \a xbps_handle_alloc().
 * @note It's assumed that \a xhp is a valid pointer.
 *
 * @return 0 on success, an errno value otherwise.
 */
int xbps_init(struct xbps_handle *xhp);

/**
 * Releases all resources used by libxbps.
 *
 * @param[in] xhp Pointer to an xbps_handle structure, as returned
 * by \a xbps_handle_alloc().
 * @note It's assumed that \a xhp is a valid pointer.
 */
void xbps_end(struct xbps_handle *xhp);

/**
 * Allocated an xbps_handle structure.
 *
 * @return A pointer to the allocated xbps_handle structure, NULL
 * otherwise.
 */
struct xbps_handle *xbps_handle_alloc(void);

/**
 * Returns a pointer to the xbps_handle structure set by xbps_init().
 */
struct xbps_handle *xbps_handle_get(void);

/*@}*/

/** @addtogroup configure */
/*@{*/

/**
 * Configure (or force reconfiguration of) a package.
 *
 * @param[in] pkgname Package name to configure.
 * @param[in] version Package version (<b>optional</b>).
 * @param[in] check_state Set it to true to check that package is
 * in unpacked state.
 * @param[in] update Set it to true if this package is being updated.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_configure_pkg(const char *pkgname,
		       const char *version,
		       bool check_state,
		       bool update);

/**
 * Configure (or force reconfiguration of) all packages.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_configure_packages(void);

/*@}*/

/**
 * @ingroup vermatch
 *
 * Compares package version strings.
 *
 * The package version is defined by:
 * ${VERSION}[_${REVISION}].
 *
 * ${VERSION} supersedes ${REVISION}.
 *
 * @param[in] pkg1 a package version string.
 * @param[in] pkg2 a package version string.
 *
 * @return -1, 0 or 1 depending if pkg1 is less than, equal to or
 * greater than pkg2.
 */
int xbps_cmpver(const char *pkg1, const char *pkg2);


/** @addtogroup download */
/*@{*/


/**
 * Download a file from a remote URL.
 * 
 * @param[in] uri Remote URI string.
 * @param[in] outputdir Directory string to store downloaded file.
 * @param[in] refetch If true and local/remote size/mtime do not match,
 * fetch the file from scratch.
 * @param[in] flags Flags passed to libfetch's fetchXget().
 * 
 * @return -1 on error, 0 if not downloaded (because local/remote size/mtime
 * do not match) and 1 if downloaded successfully.
 **/
int xbps_fetch_file(const char *uri,
		    const char *outputdir,
		    bool refetch,
		    const char *flags);

/**
 * Returns last error string reported by xbps_fetch_file().
 *
 * @return A string with the appropiate error message.
 */
const char *xbps_fetch_error_string(void);

/*@}*/

/**
 * @ingroup pkg_orphans
 *
 * Finds all package orphans currently installed.
 *
 * @param[in] orphans Proplib array of strings with package names of
 * packages that should be treated as they were already removed (optional).
 *
 * @return A proplib array of dictionaries with all orphans found,
 * on error NULL is returned and errno is set appropiately.
 */
prop_array_t xbps_find_pkg_orphans(prop_array_t orphans);

/**
 * @ingroup vermatch
 *
 * Package pattern matching.
 *
 * @param[in] instpkg Package/version string of an installed package.
 * @param[in] pattern Pattern required for \a instpkg to match.
 *
 * @return 1 if \a instpkg is matched against \a pattern, 0 if no match.
 */
int xbps_pkgpattern_match(const char *instpkg, const char *pattern);

/** @addtogroup plist */
/*@{*/

/**
 * Adds a proplib object into a proplib dictionary with specified key.
 *
 * @param[in] dict Proplib dictionary to insert the object to.
 * @param[in] obj Proplib object to be inserted.
 * @param[in] key Key associated with \a obj.
 *
 * @return true on success, false otherwise and errno set appropiately.
 */
bool xbps_add_obj_to_dict(prop_dictionary_t dict,
			  prop_object_t obj,
			  const char *key);

/**
 * Adds a proplib object into a proplib array.
 *
 * @param[in] array Proplib array to insert the object to.
 * @param[in] obj Proplib object to be inserted.
 * 
 * @return true on success, false otherwise and xbps_errno set appropiately.
 */
bool xbps_add_obj_to_array(prop_array_t array, prop_object_t obj);

/**
 * Executes a function callback specified in \a fn with \a arg paassed
 * as its argument into they array \a array.
 *
 * @param[in] array Proplib array to iterate.
 * @param[in] fn Function callback to run on every object in the array.
 * While running the function callback, the hird parameter (a pointer to
 * a boolean) can be set to true to stop immediately the loop.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success, otherwise an errno value is set appropiately.
 */
int xbps_callback_array_iter(prop_array_t array,
			     int (*fn)(prop_object_t, void *, bool *),
			     void *arg);

/**
 * Executes a function callback into the array associated with key \a key,
 * contained in a proplib dictionary.
 *
 * @param[in] dict Proplib dictionary where the array resides.
 * @param[in] key Key associated with array.
 * @param[in] fn Function callback to run on every
 * object in the array. While running the function callback, the third
 * parameter (a pointer to a boolean) can be set to true to stop
 * immediately the loop.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise an
 * errno value is set appropiately.
 */
int xbps_callback_array_iter_in_dict(prop_dictionary_t dict,
			const char *key,
			int (*fn)(prop_object_t, void *, bool *),
			void *arg);

/**
 * Executes a function callback (in reverse order) into the array
 * associated with key \a key, contained in a proplib dictionary.
 *
 * @param[in] dict Proplib dictionary where the array resides.
 * @param[in] key Key associated with array.
 * @param[in] fn Function callback to run on every
 * object in the array. While running the function callback, the third
 * parameter (a pointer to a boolean) can be set to true to stop
 * immediately the loop.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise an
 * errno value is set appropiately.
 */
int xbps_callback_array_iter_reverse_in_dict(prop_dictionary_t dict,
			const char *key,
			int (*fn)(prop_object_t, void *, bool *),
			void *arg);

/**
 * Executes a function callback per a package dictionary registered
 * in "regpkgdb" plist (downwards).
 *
 * @param[in] fn Function callback to run for any pkg dictionary.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise an
 * errno value.
 */
int xbps_regpkgdb_foreach_pkg_cb(int (*fn)(prop_object_t, void *, bool *),
				 void *arg);

/**
 * Executes a function callback per a package dictionary registered
 * in "regpkgdb" plist, in reverse order (upwards).
 *
 * @param[in] fn Function callback to run for any pkg dictionary.
 * @param[in] arg Argument to be passed to the function callback.
 *
 * @return 0 on success (all objects were processed), otherwise an
 * errno value.
 */
int xbps_regpkgdb_foreach_reverse_pkg_cb(
		int (*fn)(prop_object_t, void *, bool *),
		void *arg);

/**
 * Returns a package dictionary from regpkgdb plist, matching pkgname or
 * pkgver specified in \a pkg.
 *
 * @param[in] pkg Package name or name-version tuple to match.
 * @param[in] bypattern If false \a pkg must be a pkgname, otherwise a pkgver.
 *
 * @return The matching proplib package dictionary from regpkgdb copied
 * with \a prop_dictionary_copy() so it must be released when not required
 * anymore with prop_object_release().
 */
prop_dictionary_t xbps_regpkgdb_get_pkgd(const char *pkg, bool bypattern);

/**
 * Removes a package dictionary from regpkgdb plist matching the key
 * \a pkgname.
 *
 * @param[in] pkgname Package name to match in a dictionary.
 *
 * @return true on success, false otherwise.
 */
bool xbps_regpkgdb_remove_pkgd(const char *pkgname);

/**
 * Updates the regpkgdb plist with new contents from disk to the cached copy
 * in memory.
 *
 * @param[in] xhp Pointer to our xbps_handle struct, as returned by
 * \a xbps_handle_get() or xbps_handle_alloc().
 * @param[in] flush If true the regpkgdb plist contents in memory will
 * be flushed atomically to disk.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_regpkgdb_update(struct xbps_handle *xhp, bool flush);

/**
 * Finds the proplib's dictionary associated with a package, by looking
 * it via its name in a proplib dictionary.
 *
 * @param[in] dict Proplib dictionary to look for the package dictionary.
 * @param[in] key Key associated with the array that stores package's dictionary.
 * @param[in] pkgname Package name to look for.
 *
 * @return The package's proplib dictionary on success, NULL otherwise and
 * errno is set appropiately.
 */
prop_dictionary_t xbps_find_pkg_in_dict_by_name(prop_dictionary_t dict,
						const char *key,
						const char *pkgname);

/**
 * Finds the proplib's dictionary associated with a package, by looking
 * at it via a package pattern in a proplib dictionary.
 *
 * @param[in] dict Proplib dictionary to look for the package dictionary.
 * @param[in] key Key associated with the array storing the package's dictionary.
 * @param[in] pattern Package pattern to match.
 *
 * @return The package's proplib dictionary on success, NULL otherwise
 * and errno is set appropiately.
 */
prop_dictionary_t xbps_find_pkg_in_dict_by_pattern(prop_dictionary_t dict,
						   const char *key,
						   const char *pattern);

/**
 * Finds the package's proplib dictionary in a plist file by specifying
 * a package name.
 *
 * @param[in] plist Path to a plist file.
 * @param[in] key Proplib array's key name.
 * @param[in] pkgname Package name to match in array.
 *
 * @return The package's proplib dictionary on success, NULL otherwise and
 * errno is set appropiately. Returned dictionary is copied via
 * prop_dictionary_copy(), which means that caller is responsible to
 * release the object with prop_object_release() when done.
 */
prop_dictionary_t xbps_find_pkg_dict_from_plist_by_name(const char *plist,
							const char *key,
							const char *pkgname);

/**
 * Finds the package's proplib dictionary in a plist file by specifying
 * a package pattern.
 *
 * @param[in] plist Path to a plist file.
 * @param[in] key Proplib array's key name.
 * @param[in] pattern Package pattern to match in array.
 *
 * @return The package's proplib dictionary on success, NULL otherwise and
 * errno is set appropiately. Returned dictionary should be released with
 * prop_object_release() when it's not any longer needed.
 */
prop_dictionary_t xbps_find_pkg_dict_from_plist_by_pattern(const char *plist,
							   const char *key,
							   const char *pattern);

/**
 * Finds a package's dictionary searching in the registered packages
 * database by using a package name or a package pattern.
 *
 * @param[in] str Package name or package pattern.
 * @param[in] bypattern Set it to true to find the package dictionary
 * by using a package pattern. If false, \a str is assumed to be a package name.
 *
 * @return The package's dictionary on success, NULL otherwise and
 * errno is set appropiately. Returned dictionary is copied via
 * prop_dictionary_copy(), which means that caller is responsible to
 * release the object with prop_object_release() when done.
 */
prop_dictionary_t xbps_find_pkg_dict_installed(const char *str,
					       bool bypattern);


/**
 * Finds a virtual package's dictionary searching in the registered packages
 * database by using a package name or a package pattern.
 *
 * @param[in] str Virtual package name or package pattern to match.
 * @param[in] bypattern Set it to true to find the package dictionary
 * by using a package pattern. If false, \a str is assumed to be a package name.
 *
 * @return The virtual package's dictionary on success, NULL otherwise and
 * errno is set appropiately. Returned dictionary is copied via
 * prop_dictionary_copy(), which means that caller is responsible to
 * release the object with prop_object_release() when done.
 */
prop_dictionary_t xbps_find_virtualpkg_dict_installed(const char *str,
						      bool bypattern);

/**
 * Match a virtual package name or pattern by looking at package's
 * dictionary "provides" array object.
 *
 * @param[in] pkgd Package dictionary.
 * @param[in] str Virtual package name or package pattern to match.
 * @param[in] bypattern If true, \a str should be a package name,
 * otherwise it should be a package pattern.
 *
 * @return True if \a str matches a virtual package in \a pkgd, false
 * otherwise.
 */
bool xbps_match_virtual_pkg_in_dict(prop_dictionary_t pkgd,
				   const char *str,
				   bool bypattern);

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
bool xbps_match_any_virtualpkg_in_rundeps(prop_array_t rundeps,
					  prop_array_t provides);

/**
 * Finds a package dictionary in a proplib array by matching a package name.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] name The package name to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
prop_dictionary_t xbps_find_pkg_in_array_by_name(prop_array_t array,
						 const char *name);

/**
 * Finds a package dictionary in a proplib array by matching a package pattern.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] pattern The package pattern to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
prop_dictionary_t xbps_find_pkg_in_array_by_pattern(prop_array_t array,
						    const char *pattern);

/**
 * Finds a virtual package dictionary in a proplib array by matching a
 * package name.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] name The virtual package name to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
prop_dictionary_t xbps_find_virtualpkg_in_array_by_name(prop_array_t array,
							const char *name);

/**
 * Finds a virtual package dictionary in a proplib array by matching a
 * package pattern.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] pattern The virtual package pattern to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
prop_dictionary_t xbps_find_virtualpkg_in_array_by_pattern(prop_array_t array,
							   const char *pattern);
/**
 * Match a package name in the specified array of strings.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] pkgname The package name to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_pkgname_in_array(prop_array_t array, const char *pkgname);

/**
 * Match a package pattern in the specified array of strings.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] pattern The package pattern to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_pkgpattern_in_array(prop_array_t array, const char *pattern);

/**
 * Match a string (exact match) in the specified array of strings.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] val The value of string to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_match_string_in_array(prop_array_t array, const char *val);

/**
 * Gets a proplib object iterator associated with an array, contained
 * in a proplib dictionary matching a key.
 *
 * @param[in] dict Proplib dictionary where to look for the array.
 * @param[in] key Key associated with the array.
 *
 * @return A proplib object iterator on success, NULL otherwise and
 * errno is set appropiately.
 */
prop_object_iterator_t xbps_array_iter_from_dict(prop_dictionary_t dict,
						 const char *key);

/**
 * Get a proplib object dictionary associated with the installed package
 * \a pkgname, by internalizing its plist file defined by \a plist.
 *
 * @param[in] pkgname Package name of installed package.
 * @param[in] plist Package metadata property list file.
 *
 * @return The proplib object dictionary on success, NULL otherwise and
 * errno is set appropiately.
 */
prop_dictionary_t xbps_dictionary_from_metadata_plist(const char *pkgname,
						      const char *plist);

/**
 * Removes the package's proplib dictionary matching \a pkgname
 * in a plist file.
 *
 * @param[in] name Package name to match in plist's dictionary.
 * @param[in] plist Path to a plist file.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_remove_pkg_dict_from_plist_by_name(const char *name,
					     const char *plist);

/**
 * Removes the package's proplib dictionary matching \a pkgname
 * in a proplib array.
 *
 * @param[in] array Proplib array where to look for.
 * @param[in] name Package name to match in the array.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_remove_pkg_from_array_by_name(prop_array_t array, const char *name);

/**
 * Removes the package's proplib dictionary matching \a pkgname,
 * in an array with key \a key stored in a proplib dictionary.
 *
 * @param[in] dict Proplib dictionary storing the proplib array.
 * @param[in] key Key associated with the proplib array.
 * @param[in] name Package name to look for.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_remove_pkg_from_dict_by_name(prop_dictionary_t dict,
				       const char *key,
				       const char *name);

/**
 * Removes a string from a proplib's array.
 *
 * @param[in] array Proplib array where to look for.
 * @param[in] str String to match in the array.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_remove_string_from_array(prop_array_t array, const char *str);

/**
 * Removes a string from a proplib's array matched by a package name.
 *
 * @param[in] array Proplib array where to look for.
 * @param[in] name Package name to match.
 *
 * @return true on success, false otherwise and errno is set appropiately.
 */
bool xbps_remove_pkgname_from_array(prop_array_t array, const char *name);

/**
 * Replaces a dictionary with another dictionary in \a dict, in the
 * array \array by matching its "pkgname" object with \a pkgname.
 *
 * @param[in] array Proplib array where to look for.
 * @param[in] dict Proplib dictionary to be added in \a array.
 * @param[in] pkgname Package name to be matched.
 *
 * @return 0 on success, EINVAL if dictionary couldn't be set in
 * array or ENOENT if no match.
 */
int xbps_array_replace_dict_by_name(prop_array_t array,
				    prop_dictionary_t dict,
				    const char *pkgname);

/*@}*/

/** @addtogroup pkg_register */
/*@{*/

/**
 * Register a package into the installed packages database.
 *
 * @param[in] pkg_dict A dictionary with the following objects:
 * \a pkgname, \a version, \a pkgver, \a short_desc (string),
 * \a automatic-install (bool) and optionally \a provides (array of strings).
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_register_pkg(prop_dictionary_t pkg_dict);

/**
 * Unregister a package from the package database.
 *
 * @param[in] pkgname Package name.
 * @param[in] version Package version.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_unregister_pkg(const char *pkgname, const char *version);

/*@}*/

/** @addtogroup pkg_remove */
/*@{*/

/**
 * Remove an installed package.
 *
 * @param[in] pkgname Package name to match.
 * @param[in] version Package version associated.
 * @param[in] update If true, some steps will be skipped. See in the
 * detailed description above for more information.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_remove_pkg(const char *pkgname, const char *version, bool update);

/**
 * Remove files defined in a proplib array as specified by \a key
 * of an installed package.
 * 
 * @param[in] dict Proplib dictionary internalized from package's
 * <b>XBPS_PKGFILES</b> definition in package's metadata directory.
 * The image in Detailed description shows off its structure.
 * @param[in] key Key of the array object to match, valid values are:
 * <b>files</b>, <b>dirs</b>, <b>links</b> and <b>conf_files</b>.
 * @param[in] pkgver Package/version string matching package dictionary.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_remove_pkg_files(prop_dictionary_t dict,
			  const char *key,
			  const char *pkgver);

/*@}*/

/** @addtogroup transdict */
/*@{*/

/**
 * Finds a package by a pattern and enqueues it into
 * the transaction dictionary for future use. The first repository in
 * the pool that matched the pattern wins.
 *
 * @param pkgpattern Package name or pattern to find.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_transaction_install_pkg(const char *pkgpattern);

/**
 * Marks a package as "going to be updated" in the transaction dictionary.
 * All repositories in the pool will be used, and if a newer version
 * is available the package dictionary will be enqueued.
 *
 * @param pkgname The package name to update.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_transaction_update_pkg(const char *pkgname);

/**
 * Finds newer versions for all installed packages by looking at the
 * repository pool. If a newer version exists, package will be enqueued
 * into the transaction dictionary.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_transaction_update_packages(void);

/**
 * Removes a package currently installed. The package dictionary will
 * be added into the transaction dictionary.
 *
 * @param[in] pkgname Package name to be removed.
 * @param[in] recursive If true, all packages that are currently depending
 * on the package to be removed, and if they are orphans, will be added.
 *
 * @return 0 on success, ENOENT if pkg is not installed, EEXIST if package
 * has reverse dependencies, EINVAL or ENXIO if a problem ocurred in the
 * process.
 */
int xbps_transaction_remove_pkg(const char *pkgname,
				bool recursive);

/**
 * Finds all package orphans currently installed and adds them into
 * the transaction dictionary.
 *
 * @return 0 on succcess, ENOENT if no package orphans were found, ENXIO
 * or EINVAL if a problem ocurred in the process.
 */
int xbps_transaction_autoremove_pkgs(void);

/**
 * Returns the transaction dictionary, as shown above in the image.
 * Before returning the package list is sorted in the correct order
 * and total installed/download size for the transaction is computed.
 *
 * @return The proplib transaction dictionary on success, otherwise NULL
 * and errno is set appropiately. ENXIO if the transaction
 * dictionary and the missing deps array were not created. ENODEV if
 * there are missing dependencies or any other if there was an error
 * while sorting packages or computing the transaction size.
 *
 * @note
 *  - This function will set errno to ENXIO if xbps_transaction_install_pkg()
 *    or xbps_transaction_update_pkg() functions were not called previously.
 */
prop_dictionary_t xbps_transaction_prepare(void);

/**
 * Commit a transaction. The transaction dictionary contains all steps
 * to be executed in the transaction, as returned by xbps_transaction_prepare().
 *
 * @param[in] transd The transaction dictionary.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_transaction_commit(prop_dictionary_t transd);

/**
 * Returns the missing deps array if xbps_transaction_install_pkg()
 * or xbps_transaction_update_pkg() failed to find required packages
 * in registered repositories.
 *
 * @return The proplib array, NULL if it couldn't be created.
 */
prop_array_t xbps_transaction_missingdeps_get(void);

/*@}*/

/** @addtogroup plist_fetch */
/*@{*/

/**
 * Internalizes a plist file in a binary package file stored locally or
 * remotely as specified in the URL.
 *
 * @param[in] url URL to binary package file (full local or remote path).
 * @param[in] plistf Plist file name to internalize.
 *
 * @return An internalized proplib dictionary, otherwise NULL and
 * errno is set appropiately.
 */
prop_dictionary_t xbps_dictionary_metadata_plist_by_url(const char *url,
							const char *plistf);

/*@}*/

/** @addtogroup repopool */
/*@{*/

/**
 * @struct repository_pool_index xbps_api.h "xbps_api.h"
 * @brief Repository pool dictionary structure
 *
 * Repository index object structure registered in a private simple queue.
 * The structure contains a dictionary and the URI associated with the
 * registered repository index.
 */
struct repository_pool_index {
	/**
	 * @var rpi_repod
	 * 
	 * Internalized Proplib dictionary of the index plist file
	 * associated with repository.
	 */
	prop_dictionary_t rpi_repod;
	/**
	 * @var rpi_uri
	 * 
	 * URI string associated with repository.
	 */
	const char *rpi_uri;
	/**
	 * @var rpi_index
	 *
	 * Repository index in pool.
	 */
	uint16_t rpi_index;
};

/**
 * Synchronizes the package index file for all remote repositories
 * as specified in the configuration file, repositories.plist.
 *
 * @return 0 on success, ENOTSUP if no repositories were found in
 * the configuration file.
 */
int xbps_repository_pool_sync(const struct xbps_handle *xhp);

/**
 * Iterates over the repository pool and executes the \a fn function
 * callback passing in the void * \a arg argument to it. The bool pointer
 * argument can be used in the callbacks to stop immediately the loop if
 * set to true, otherwise it will only be stopped if it returns a
 * non-zero value.
 *
 * @param[in] fn Function callback to execute for every repository registered in
 * the pool.
 * @param[in] arg Opaque data passed in to the \a fn function callback for
 * client data.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_repository_pool_foreach(
		int (*fn)(struct repository_pool_index *, void *, bool *),
		void *arg);

/**
 * Finds a package dictionary in the repository pool by specifying a
 * package pattern or a package name. This function does not take into
 * account virtual packages set in configuration file.
 *
 * @param[in] pkg Package pattern or name.
 * @param[in] bypattern True if \a pkg is a pattern, false if it is a pkgname.
 * @param[in] best True to find the best version available in repo, false to
 * fetch the first package found matching its pkgname.
 *
 * @return The package dictionary if found, NULL otherwise.
 */
prop_dictionary_t
	xbps_repository_pool_find_pkg(const char *pkg, bool bypattern, bool best);

/**
 * Finds a package dictionary in repository pool by specifying a
 * package pattern or a package name. Only virtual packages set in
 * configuration file will be matched.
 *
 * @param[in] pkg Virtual package pattern or name.
 * @param[in] bypattern True if \a pkg is a pattern, false if it is a pkgname.
 * @param[in] best True to find the best version available in repo, false to
 * fetch the first package found matching its pkgname.
 *
 * @return The package dictionary if found, NULL otherwise.
 */
prop_dictionary_t
	xbps_repository_pool_find_virtualpkg(const char *pkg,
					     bool bypattern, bool best);

/**
 * Iterate over the the repository pool and search for a metadata plist
 * file in a binary package named 'pkgname'. If a package is matched by
 * \a pkgname, the plist file \a plistf will be internalized into a
 * proplib dictionary.
 *
 * The first repository that has it wins and the loop is stopped.
 * This will work locally and remotely, thanks to libarchive and
 * libfetch!
 *
 * @param[in] pkgname Package name to match.
 * @param[in] plistf Plist file name to match.
 *
 * @return An internalized proplib dictionary of \a plistf, otherwise NULL
 * and errno is set appropiately.
 *
 * @note if NULL is returned and errno is ENOENT, that means that
 * binary package file has been found but the plist file could not
 * be found.
 */
prop_dictionary_t
	xbps_repository_pool_dictionary_metadata_plist(const char *pkgname,
						       const char *plistf);

/*@}*/

/** @addtogroup reposync */
/*@{*/

/**
 * Syncs the package index file for a remote repository as specified
 * by the \a uri argument (if necessary).
 *
 * @param[in] uri URI to a remote repository.
 *
 * @return -1 on error (errno is set appropiately), 0 if transfer was
 * not necessary (local/remote size/mtime matched) or 1 if
 * downloaded successfully.
 */
int xbps_repository_sync_pkg_index(const char *uri);

/*@}*/

/** @addtogroup pkgstates */
/*@{*/

/**
 * @enum pkg_state_t
 *
 * Integer representing a state on which a package may be. Possible
 * values for this are:
 *
 * <b>XBPS_PKG_STATE_HALF_UNPACKED</b>: Package was being unpacked
 * but didn't finish properly.
 *
 * <b>XBPS_PKG_STATE_UNPACKED</b>: Package has been unpacked correctly
 * but has not been configured due to unknown reasons.
 *
 * <b>XBPS_PKG_STATE_INSTALLED</b>: Package has been installed successfully.
 *
 * <b>XBPS_PKG_STATE_BROKEN</b>: not yet used.
 *
 * <b>XBPS_PKG_STATE_HALF_REMOVED</b>: Package has been removed but not
 * completely: the purge action in REMOVE script wasn't executed, pkg
 * metadata directory still exists and is registered in package database.
 *
 * <b>XBPS_PKG_STATE_NOT_INSTALLED</b>: Package going to be installed in
 * a transaction dictionary but that has not been yet unpacked.
 */
typedef enum pkg_state {
	XBPS_PKG_STATE_UNPACKED = 1,
	XBPS_PKG_STATE_INSTALLED,
	XBPS_PKG_STATE_BROKEN,
	XBPS_PKG_STATE_HALF_REMOVED,
	XBPS_PKG_STATE_NOT_INSTALLED,
	XBPS_PKG_STATE_HALF_UNPACKED
} pkg_state_t;

/**
 * Gets package state from package \a pkgname, and sets its state
 * into \a state.
 * 
 * @param[in] pkgname Package name.
 * @param[out] state Package state returned.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_pkg_state_installed(const char *pkgname, pkg_state_t *state);

/**
 * Gets package state from a package dictionary \a dict, and sets its
 * state into \a state.
 *
 * @param[in] dict Package dictionary.
 * @param[out] state Package state returned.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t *state);

/**
 * Sets package state \a state in package \a pkgname.
 *
 * @param[in] pkgname Package name.
 * @param[in] version Package version (optional).
 * @param[in] pkgver Package name/version touple (optional).
 * @param[in] state Package state to be set.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_set_pkg_state_installed(const char *pkgname,
				 const char *version,
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
int xbps_set_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t state);

/*@}*/

/** @addtogroup unpack */
/*@{*/


/**
 * Unpacks a binary package into specified root directory.
 *
 * @param[in] trans_pkg_dict Package proplib dictionary as stored in the
 * \a packages array returned by the transaction dictionary.
 *
 * @return 0 on success, otherwise an errno value.
 */
int xbps_unpack_binary_pkg(prop_dictionary_t trans_pkg_dict);

/*@}*/

/** @addtogroup util */
/*@{*/

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
char *xbps_xasprintf(const char *fmt, ...);

/**
 * Returns a string with the sha256 hash for the file specified
 * by \a file.
 *
 * @param[in] file Path to a file.
 * @return A pointer to a malloc(3)ed string, NULL otherwise and errno
 * is set appropiately. The pointer should be free(3)d when it's no
 * longer needed.
 */
char *xbps_file_hash(const char *file);

/**
 * Returns a string with the sha256 hash for the file specified
 * by \a file in an array with key \a key in the proplib dictionary
 * \a d.
 *
 * @param[in] d Proplib dictionary to look in.
 * @param[in] key Array key to match in dictionary.
 * @param[in] file Pathname to a file.
 *
 * @return The sha256 hash string if found, NULL otherwise
 * and errno is set appropiately.
 */
const char *xbps_file_hash_dictionary(prop_dictionary_t d,
				      const char *key,
				      const char *file);

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
int xbps_file_hash_check(const char *file, const char *sha256);

/**
 * Checks if \a file matches the sha256 hash specified in the array
 * with key \a key in the proplib dictionary \a d.
 *
 * @param[in] d Proplib dictionary to look in.
 * @param[in] key Proplib array key to match for file.
 * @param[in] file Pathname to a file.
 *
 * @return 0 if hash is matched, -1 on error and 1 if no match.
 */
int xbps_file_hash_check_dictionary(prop_dictionary_t d,
				    const char *key,
				    const char *file);

/**
 * Checks if a package is currently installed by matching a package
 * pattern string.
 *
 * @param[in] pkg Package pattern used to find the package.
 *
 * @return -1 on error (errno set appropiately), 0 if package pattern
 * didn't match installed package, 1 if \a pkg pattern fully
 * matched installed package.
 */
int xbps_check_is_installed_pkg_by_pattern(const char *pkg);

/**
 * Checks if package \a pkgname is currently installed.
 *
 * @param[in] pkgname Package name.
 *
 * @return True if \a pkgname is installed, false otherwise.
 */
bool xbps_check_is_installed_pkg_by_name(const char *pkgname);

/**
 * Checks if the URI specified by \a uri is remote or local.
 *
 * @param[in] uri URI string.
 * 
 * @return true if URI is remote, false if local.
 */
bool xbps_check_is_repository_uri_remote(const char *uri);

/**
 * Gets the full URI to a binary package file as returned by a
 * package dictionary from a repository in \a pkgd, by looking at the
 * repository location object "repository" in its dictionary.
 *
 * @param[in] pkgd Package dictionary stored in a transaction dictionary.
 * @param[in] repoloc Repository URL location string.
 *
 * @return A pointer to a malloc(3)ed string, NULL otherwise and
 * errno is set appropiately. The pointer should be free(3)d when it's
 * no longer needed.
 */ 
char *xbps_path_from_repository_uri(prop_dictionary_t pkgd, const char *repoloc);

/**
 * Gets the full path to a repository package index plist file, as
 * specified by \a uri.
 *
 * @param[in] uri Repository URI.
 *
 * @return A pointer to a malloc(3)d string, NULL otherwise and
 * errno is set appropiately. The pointer should be free(3)d when it's
 * no longer needed.
 */
char *xbps_pkg_index_plist(const char *uri);

/**
 * Gets the name of a package string. Package strings are composed
 * by a @<pkgname@>/@<version@> pair and separated by the <em>minus</em>
 * sign, i.e <b>foo-2.0</b>.
 *
 * @param[in] pkg Package string.
 *
 * @return A pointer to a malloc(3)d string, NULL otherwise and
 * errno is set appropiately. The pointer should be free(3)d when it's
 * no longer needed.
 */
char *xbps_pkg_name(const char *pkg);

/**
 * Gets a the package name of a package pattern string specified by
 * the \a pattern argument.
 *
 * @param[in] pattern A package pattern. Package patterns are composed
 * by looking at <b>'><='</b> to split components, i.e <b>foo>=2.0</b>,
 * <b>blah<1.0</b>, <b>blob==2.0</b>, etc.
 *
 * @return A pointer to a malloc(3)ed string with the package name,
 * NULL otherwise and errno is set appropiately. The pointer should be
 * free(3)d when it's no longer needed.
 */
char *xbps_pkgpattern_name(const char *pattern);

/**
 * Gets the package version in a package string, i.e <b>foo-2.0</b>.
 * 
 * @param[in] pkg Package string.
 *
 * @return A string with the version string, NULL if it couldn't
 * find the version component.
 */
const char *xbps_pkg_version(const char *pkg);

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
 * Gets the package version revision in a package string.
 *
 * @param[in] pkg Package string, i.e <b>foo-2.0_1</b>.
 *
 * @return A string with the revision number, NULL if it couldn't
 * find the revision component.
 */
const char *xbps_pkg_revision(const char *pkg);

/**
 * Checks if a package has run dependencies.
 *
 * @param[in] dict Package dictionary.
 *
 * @return True if package has run dependencies, false otherwise.
 */
bool xbps_pkg_has_rundeps(prop_dictionary_t dict);

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

/*@}*/

__END_DECLS

#endif /* !_XBPS_API_H_ */
