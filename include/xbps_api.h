/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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
#include <sys/cdefs.h>
#include <sys/queue.h>
/**
 * @cond
 */
#ifndef DEBUG
#  define NDEBUG
#endif
#include <assert.h>
/**
 * @endcond
 */
#include <prop/proplib.h>
#include <archive.h>
#include <archive_entry.h>

__BEGIN_DECLS

/**
 * @mainpage The X Binary Package System Library API
 * @section intro_sec Introduction
 *
 * XBPS is a new binary package system designed and implemented from
 * scratch, by <b>Juan Romero Pardines</b>. This document describes
 * the API used by the XBPS Library, that is the base to implement
 * a package manager frontend, such as is implemented in the xbps-bin(8)
 * and xbps-repo(8) command line interfaces.
 *
 * XBPS uses extensively NetBSD's proplib, a library that provides an
 * abstract interface for creating and manipulating property lists.
 * Property lists have object types for boolean values, opaque data, numbers,
 * and strings. Structure is provided by the array and dictionary collection
 * types. Property lists can be passed across protection boundaries by
 * translating them to an external representation. This external representation
 * is an XML document whose format is described by the following DTD:
 *
 * http://www.apple.com/DTDs/PropertyList-1.0.dtd
 *
 * NetBSD's proplib has been used because it's fast, extensible, and easy
 * to use. These are the three facts I mentioned:
 *
 * <ul>
 * <li><b>Fast</b> because proplib uses an ultra optimized
 * <em>red-black tree</em> implementation to store and find all its objects,
 * the same implementation has been used in commercial projects by
 * <em>Apple Inc</em>.</li>
 *
 * <li><b>Extensible</b> because you don't have to worry about
 * ABI problems with its objects, arrays and dictionaries can be extended
 * without such problems.</li>
 *
 * <li><b>Easy</b> to use (and learn) because it has a superb documentation
 * available in the form of manual pages.</li>
 * </ul>
 *
 * Not to mention that its arrays and dictionaries can be externalized to
 * files (known as plists) and <b>always are written atomically</b>. You
 * have the whole file or don't have it at all.
 *
 * @file include/xbps_api.h
 * @brief XBPS Library API header
 *
 * This header documents the full API for the XBPS Library.
 */

/**
 * @def XBPS_RELVER
 * Current library release date.
 */
#define XBPS_RELVER		"20100121"

/** 
 * @def XBPS_META_PATH
 * Default root PATH store metadata info.
 */
#define XBPS_META_PATH		"/var/db/xbps"

/** 
 * @def XBPS_CACHE_PATH
 * Default cache PATH to store downloaded binpkgs.
 */
#define XBPS_CACHE_PATH		"/var/cache/xbps"

/**
 * @def XBPS_REPOLIST
 * Filename for the repositories plist file.
 */
#define XBPS_REPOLIST		"repositories.plist"

/** 
 * @def XBPS_REGPKGDB
 * Filename of the packages register database.
 */
#define XBPS_REGPKGDB		"regpkgdb.plist"

/** 
 * @def XBPS_PKGPROPS
 * Package metadata properties file.
 */
#define XBPS_PKGPROPS		"props.plist"

/**
 * @def XBPS_PKGFILES
 * Package metadata files properties file.
 */
#define XBPS_PKGFILES		"files.plist"

/** 
 * @def XBPS_PKGINDEX
 * Filename of the package index plist for a repository.
 */
#define XBPS_PKGINDEX		"pkg-index.plist"

/**
 * @def XBPS_PKGINDEX_VERSION
 * Current version of the package index format.
 */
#define XBPS_PKGINDEX_VERSION	"1.1"

/**
 * @def XBPS_FLAG_VERBOSE
 * Verbose flag used in xbps_unpack_binary_pkg() (for now).
 * Must be set through the xbps_set_flags() function.
 */
#define XBPS_FLAG_VERBOSE	0x00000001

/**
 * @def XBPS_FLAG_FORCE
 * Force flag used in xbps_configure_pkg() (for now).
 * Must be set through the xbps_set_flags() function.
 */
#define XBPS_FLAG_FORCE		0x00000002

/**
 * @cond
 */
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

/*
 * By default all public functions have default visibility, unless
 * gcc >= 4.x and the HIDDEN definition is used.
 */
#if __GNUC__ >= 4
#define HIDDEN __attribute__ ((visibility("hidden")))
#else
#define HIDDEN
#endif
/**
 * @endcond
 */

/**
 * @private
 * From lib/config_files.c
 */
int HIDDEN xbps_config_file_from_archive_entry(prop_dictionary_t,
					       struct archive_entry *,
					       const char *,
					       int *,
					       bool *);

/**
 * @ingroup configure
 *
 * Configure (or force reconfiguration of) a package.
 *
 * @param[in] pkgname Package name to configure.
 * @param[in] version Package version (<b>optional</b>).
 * @param[in] check_state Set it to true to check that package is
 * in unpacked state.
 * @param[in] update Set it to true if this package is being updated.
 *
 * @return 0 on success, or an appropiate errno value otherwise.
 */
int xbps_configure_pkg(const char *pkgname,
		       const char *version,
		       bool check_state,
		       bool update);

/**
 * @ingroup configure
 *
 * Configure (or force reconfiguration of) all packages.
 *
 * @return 0 on success, or an appropiate errno value otherwise.
 */
int xbps_configure_all_pkgs(void);

/**
 * @ingroup vermatch
 *
 * Compares package version strings.
 *
 * The package version is defined by:
 * ${VERSION}[_${PKGREVISION}][-${EPOCH}].
 *
 * ${EPOCH} supersedes ${VERSION} supersedes ${PKGREVISION}.
 *
 * @param[in] pkg1 a package version string.
 * @param[in] pkg2 a package version string.
 *
 * @return -1, 0 or 1 depending if pkg1 is less than, equal to or
 * greater than pkg2.
 */
int xbps_cmpver(const char *pkg1, const char *pkg2);

/**
 * @ingroup download
 *
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
 * @ingroup download
 *
 * Returns last error string reported by xbps_fetch_file().
 *
 * @return A string with the appropiate error message.
 */
const char *xbps_fetch_error_string(void);

/**
 * @ingroup fexec
 *
 * Forks and executes a command in the current working directory
 * with an arbitrary number of arguments.
 *
 * @param[in] arg Arguments passed to execvp(3) when forked, the last
 * argument must be NULL.
 *
 * @return 0 on success, -1 on error and errno set appropiately.
 */
int xbps_file_exec(const char *arg, ...);

/**
 * @ingroup fexec
 *
 * Forks and executes a command in the current working directory
 * with an arbitrary number of arguments.
 *
 * @param[in] arg Arguments passed to execvp(3) when forked, does not need
 * to be terminated with a NULL argument.
 *
 * @return 0 on success, -1 on error and errno set appropiately.
 */
int xbps_file_exec_skipempty(const char *arg, ...);

/**
 * @ingroup fexec
 *
 * Forks and executes a command with an arbitrary number of arguments
 * in a specified path.
 * 
 * If uid==0 and /bin/sh (relative to path) exists, a chroot(2) call
 * will be done, otherwise chdir(2) to path.
 * 
 * @param[in] path Destination path to chroot(2) or chdir(2).
 * @param[in] arg Arguments passed to execvp(3) when forked, the last
 * argument must be NULL.
 *
 * @return 0 on success, -1 on error and errno set appropiately.
 */
int xbps_file_chdir_exec(const char *path, const char *arg, ...);

/* From lib/humanize_number.c */
#define HN_DECIMAL		0x01
#define HN_NOSPACE		0x02
#define HN_B			0x04
#define HN_DIVISOR_1000		0x08
#define HN_GETSCALE		0x10
#define HN_AUTOSCALE		0x20

int xbps_humanize_number(char *, size_t, int64_t, const char *, int, int);

/**
 * @ingroup dircreate
 *
 * Creates a directory (and required components if necessary).
 *
 * @param[in] path Path for final directory.
 * @param[in] mode Mode for final directory (0755 if not specified).
 *
 * @return 0 on success, -1 on error and errno set appropiately.
 */
int xbps_mkpath(char *path, mode_t mode);

/**
 * @ingroup pkg_orphans
 *
 * Finds all package orphans currently installed.
 *
 * @return A proplib array of dictionaries with all orphans found,
 * on error NULL is returned and errno set appropiately.
 */
prop_array_t xbps_find_orphan_packages(void);

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
int xbps_pkgpattern_match(const char *instpkg, char *pattern);

/**
 * @ingroup plist
 *
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
 * @ingroup plist
 *
 * Adds a proplib object into a proplib array.
 *
 * @param[in] array Proplib array to insert the object to.
 * @param[in] obj Proplib object to be inserted.
 * 
 * @return true on success, false otherwise and errno set appropiately.
 */
bool xbps_add_obj_to_array(prop_array_t array, prop_object_t obj);

/**
 * @ingroup plist
 *
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
 * errno value on error.
 */
int xbps_callback_array_iter_in_dict(prop_dictionary_t dict,
			const char *key,
			int (*fn)(prop_object_t, void *, bool *),
			void *arg);

/**
 * @ingroup plist
 *
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
 * errno value on error.
 */
int xbps_callback_array_iter_reverse_in_dict(prop_dictionary_t dict,
			const char *key,
			int (*fn)(prop_object_t, void *, bool *),
			void *arg);

/**
 * @ingroup plist
 *
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
 * @ingroup plist
 *
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
 * @ingroup plist
 *
 * Finds the package's proplib dictionary by looking at it in
 * a plist file.
 *
 * @param[in] plist Path to a plist file.
 * @param[in] pkgname Package name to look for.
 *
 * @return The package's proplib dictionary on success, NULL otherwise and
 * errno is set appropiately.
 */
prop_dictionary_t xbps_find_pkg_from_plist(const char *plist,
					   const char *pkgname);

/**
 * @ingroup plist
 *
 * Finds a package's dictionary searching in the registered packages
 * database by using a package name or a package pattern.
 *
 * @param[in] str Package name or package pattern.
 * @param[in] bypattern Set it to true to find the package dictionary
 * by using a package pattern. If false, \a str is assumed to be a package name.
 *
 * @return The package's dictionary on success, NULL otherwise and
 * errno is set appropiately.
 */
prop_dictionary_t xbps_find_pkg_dict_installed(const char *str,
					       bool bypattern);

/**
 * @ingroup plist
 *
 * Finds a string matching an object in a proplib array.
 *
 * @param[in] array The proplib array where to look for.
 * @param[in] val The value of string to match.
 *
 * @return true on success, false otherwise and errno set appropiately
 * if there was an unexpected error.
 */
bool xbps_find_string_in_array(prop_array_t array, const char *val);

/**
 * @ingroup plist
 *
 * Gets a proplib object iterator associated with an array, contained
 * in a proplib dictionary matching a key.
 *
 * @param[in] dict Proplib dictionary where to look for the array.
 * @param[in] key Key associated with the array.
 *
 * @return A proplib object iterator on success, NULL otherwise and
 * errno is set appropiately.
 */
prop_object_iterator_t xbps_get_array_iter_from_dict(prop_dictionary_t dict,
						     const char *key);

/**
 * @ingroup plist
 *
 * Finds a proplib dictionary in an archive, matching a specific
 * entry on it.
 *
 * @param[in] ar Pointer to an archive object, as returned by libarchive.
 * @param[in] entry Pointer to an archive entry object, as returned by libarchive.
 *
 * @return The proplib dictionary associated with entry, NULL otherwise
 * and errno is set appropiately.
 */
prop_dictionary_t xbps_read_dict_from_archive_entry(struct archive *ar,
				struct archive_entry *entry);

/**
 * @ingroup plist
 *
 * Removes the package's proplib dictionary matching \a pkgname
 * in a plist file.
 *
 * @param[in] pkgname Package name to look for.
 * @param[in] plist Path to a plist file.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_remove_pkg_dict_from_file(const char *pkgname, const char *plist);

/**
 * @ingroup plist
 *
 * Removes the package's proplib dictionary matching \a pkgname,
 * in an array with key \a key stored in a proplib dictionary.
 *
 * @param[in] dict Proplib dictionary storing the proplib array.
 * @param[in] key Key associated with the proplib array.
 * @param[in] pkgname Package name to look for.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_remove_pkg_from_dict(prop_dictionary_t dict,
			      const char *key,
			      const char *pkgname);

/**
 * @ingroup plist
 *
 * Removes a string from a proplib's array.
 *
 * @param[in] array Proplib array where to look for.
 * @param[in] str String to match in the array.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_remove_string_from_array(prop_array_t array, const char *str);

/**
 * @ingroup purge
 *
 * Purge an installed package.
 *
 * @param[in] pkgname Package name to match.
 * @param[in] check_state Set it to true to check that package
 * is in <b>config-files</b> state.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_purge_pkg(const char *pkgname, bool check_state);

/**
 * @ingroup purge
 *
 * Purge all installed packages. Packages that aren't in
 * <b>config-files</b> state will be ignored.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_purge_all_pkgs(void);

/**
 * @ingroup pkg_register
 *
 * Register a package into the installed packages database.
 *
 * @param[in] pkgrd Package proplib dictionary returned by a transaction.
 * @param[in] automatic Set it to true to mark package that has been
 * installed by another package, and not explicitly.
 *
 * @return 0 on success, an errno value otherwise.
 */
int xbps_register_pkg(prop_dictionary_t pkgrd, bool automatic);

/**
 * @ingroup pkg_register
 *
 * Unregister a package from the package database.
 *
 * @param[in] pkgname Package name to match.
 *
 * @return 0 on success, an errno value otherwise.
 */
int xbps_unregister_pkg(const char *pkgname);

/**
 * @ingroup regpkgdb
 *
 * Initialize resources used by the installed packages database.
 *
 * @note This function is reference counted, if the database has
 * been initialized previously, the counter will be increased by one
 * and dictionary stored in memory will be returned.
 *
 * @warning Don't forget to always use xbps_regpkgs_dictionary_release()
 * once that you don't this data to release resources if possible.
 *
 * @return A proplib dictionary with all packages registered currently
 * on success, NULL otherwise and errno is set appropiately.
 */
prop_dictionary_t xbps_regpkgs_dictionary_init(void);

/**
 * @ingroup regpkgdb
 *
 * Release resources used by the installed packages database.
 *
 * @note This function is reference counted, if the database
 * is in use by other callers it won't be released.
 */ 
void xbps_regpkgs_dictionary_release(void);

/**
 * @ingroup pkg_remove
 *
 * Remove an installed package.
 *
 * @param[in] pkgname Package name to match.
 * @param[in] version Package version associated.
 * @param[in] update If true, and depending if \a pkgname is an
 * <em>essential</em> package, some steps will be skipped. See in the
 * detailed description section for more information.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_remove_pkg(const char *pkgname, const char *version, bool update);

/**
 * @ingroup pkg_remove
 *
 * Remove files defined in a proplib array as specified by \a key
 * of an installed package.
 * 
 * @param[in] dict Proplib dictionary internalized from package's
 * "files.plist" metadata plist file.
 * @param[in] key Key where the object files reside on, valid values are:
 * "files", "dirs", "links" and "conf_files".
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_remove_pkg_files(prop_dictionary_t dict, const char *key);

/**
 * @private
 * From lib/remove_obsoletes.c
 */
int HIDDEN xbps_remove_obsoletes(prop_dictionary_t, prop_dictionary_t);

/**
 * @ingroup repo_register
 *
 * Registers a repository into the database.
 *
 * @param[in] uri URI pointing to the repository.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_repository_register(const char *uri);

/**
 * @ingroup repo_register
 *
 * Unregisters a repository from the database.
 *
 * @param[in] uri URI pointing to the repository.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_repository_unregister(const char *uri);

/**
 * @private
 */
int HIDDEN xbps_repository_find_pkg_deps(prop_dictionary_t,
					 prop_dictionary_t);

/**
 * @ingroup repo_pkgs
 *
 * Finds a package by its name or by a pattern and enqueues it into
 * the transaction dictionary for future use. The first repository in
 * the queue that matched the requirement wins.
 *
 * @note The function name might be misleading, but is correct because
 * if package is found, it will be marked as "going to be installed".
 *
 * @param pkg Package name or pattern to find.
 * @param bypattern If true, a package pattern will be used in \a pkg.
 * Otherwise \a pkg will be used as a package name.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_repository_install_pkg(const char *pkg, bool bypattern);

/**
 * @ingroup repo_pkgs
 *
 * Marks a package as "going to be updated" in the transaction dictionary.
 * All repositories in the pool will be used, and if a newer version
 * is available the package dictionary will be enqueued.
 *
 * @param pkgname The package name to update.
 * @param instpkg Installed package dictionary, as returned by
 * xbps_find_pkg_installed_from_plist().
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_repository_update_pkg(const char *pkgname,
			       prop_dictionary_t instpkg);

/**
 * @ingroup repo_pkgs
 *
 * Finds newer versions for all installed packages by looking at the
 * repository pool. If a newer version exists, package will be enqueued
 * into the transaction dictionary.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_repository_update_allpkgs(void);

/**
 * @ingroup repo_pkgs
 *
 * Returns the transaction proplib dictionary with properties for
 * all operations previously done.
 *
 * @note This function won't return anything useful in the proplib
 * dictionary, if either of xbps_repository_install_pkg() or
 * xbps_repository_update_pkg() functions are not called previously.
 *
 * @return The transaction dictionary to install/update/replace
 * a package list.
 */
prop_dictionary_t xbps_repository_get_transaction_dict(void);

/**
 * @ingroup repo_plist
 * 
 * Returns a malloc(3)ed URI string pointing to a binary package file,
 * either from a local or remote repository.
 *
 * @note The caller is responsible to free(3) the returned buffer.
 *
 * @param[in] d Package proplib dictionary as returned by the
 * transaction dictionary, aka xbps_repository_get_transaction_dict().
 * @param[in] uri URI pointing to a repository.
 *
 * @return A string with the full path, NULL otherwise and errno
 * set appropiately.
 */
char *xbps_repository_get_path_from_pkg_dict(prop_dictionary_t d,
					     const char *uri);

/**
 * @ingroup repo_plist
 *
 * Iterate over the the repository pool and search for a plist file
 * in the binary package named 'pkgname'. The plist file will be
 * internalized to a proplib dictionary.
 *
 * The first repository that has it wins and the loop is stopped.
 * This will work locally and remotely, thanks to libarchive and
 * libfetch!
 *
 * @param[in] pkgname Package name to match.
 * @param[in] plistf Plist file name to match.
 *
 * @return An internalized proplib dictionary, otherwise NULL and
 * errno is set appropiately.
 *
 * @note if NULL is returned and errno is ENOENT, that means that
 * binary package file has been found but the plist file could not
 * be found.
 */
prop_dictionary_t xbps_repository_get_pkg_plist_dict(const char *pkgname,
						     const char *plistf);

/**
 * @ingroup repo_plist
 *
 * Finds a plist file in a binary package file stored local or
 * remotely as specified in the URL.
 *
 * @param[in] url URL to binary package file.
 * @param[in] plistf Plist file name to match.
 *
 * @return An internalized proplib dictionary, otherwise NULL and
 * errno is set appropiately.
 */
prop_dictionary_t xbps_repository_get_pkg_plist_dict_from_url(const char *url,
							const char *plistf);

/**
 * @ingroup repopool
 *
 * @struct repository_pool xbps_api.h "xbps_api.h"
 * @brief Repository pool structure
 *
 * Repository object structure registered in the global simple queue
 * \a rp_queue. The simple queue must be initialized through
 * xbps_repository_pool_init(), and released with
 * xbps_repository_pool_release() when it's no longer needed.
 */
struct repository_pool {
	/**
	 * @var rp_entries
	 * 
	 * Structure that connects elements in the simple queue.
	 * For use with the SIMPLEQ macros.
	 */
	SIMPLEQ_ENTRY(repository_pool) rp_entries;
	/**
	 * @var rp_repod
	 * 
	 * Proplib dictionary associated with repository.
	 */
	prop_dictionary_t rp_repod;
	/**
	 * @var rp_uri
	 * 
	 * URI string associated with repository.
	 */
	char *rp_uri;
};
/**
 * @ingroup repopool
 *
 * @var rp_queue
 * @brief Pointer to the head of global simple queue.
 * 
 * A pointer to the head of the global simple queue to
 * use after xbps_repository_pool_init() has been initialized
 * successfully.
 */
SIMPLEQ_HEAD(repopool_queue, repository_pool);
struct repopool_queue rp_queue;

/**
 * @ingroup repopool
 *
 * Initializes the repository pool by creating a global simple queue
 * \a rp_queue with all registered repositories in the database.
 *
 * Once it's initialized, access to the repositories can be done
 * by the global simple queue \a rp_queue and the \a repository_pool
 * structure.
 *
 * @note This function is reference counted, don't forget to call
 * xbps_repository_pool_release() when it's no longer needed.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_repository_pool_init(void);

/**
 * @ingroup repopool
 *
 * Releases the repository pool with all registered repositories
 * in the database.
 *
 * @note This function is reference counted, it won't be released until
 * its reference counter is 0.
 */
void xbps_repository_pool_release(void);

/**
 * @ingroup reposync
 *
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

/**
 * @ingroup reposync
 *
 * Returns a malloc(3)ed string with the local directory name for
 * the package cache directory pointing to a remote URI repository.
 *
 * @note The caller is responsible to free(3) the returned buffer,
 * @param[in] uri URI to a remote repository.
 *
 * @return A buffer with the transformed string, NULL otherwise and
 * errno is set appropiately.
 */
char *xbps_get_remote_repo_string(const char *uri);

/**
 * @private
 * From lib/requiredby.c
 */
int HIDDEN xbps_requiredby_pkg_add(prop_array_t, prop_dictionary_t);
int HIDDEN xbps_requiredby_pkg_remove(const char *);

/**
 * @private
 * From lib/sortdeps.c
 */
int HIDDEN xbps_sort_pkg_deps(prop_dictionary_t);

/**
 * @ingroup pkgstates
 *
 * @enum pkg_state_t
 *
 * Integer representing a state on which a package may be. Possible
 * values for this are:
 *
 * <b>XBPS_PKG_STATE_UNPACKED</b>: Package has been unpacked correctly
 * but has not been configured due to unknown reasons.
 *
 * <b>XBPS_PKG_STATE_INSTALLED</b>: Package has been installed successfully.
 *
 * <b>XBPS_PKG_STATE_BROKEN</b>: not yet used.
 *
 * <b>XBPS_PKG_STATE_CONFIG_FILES</b>: Package has been removed but not
 * yet purged.
 *
 * <b>XBPS_PKG_STATE_NOT_INSTALLED</b>: Package going to be installed in
 * a transaction dictionary but that has not been yet unpacked.
 */
typedef enum pkg_state {
	XBPS_PKG_STATE_UNPACKED = 1,
	XBPS_PKG_STATE_INSTALLED,
	XBPS_PKG_STATE_BROKEN,
	XBPS_PKG_STATE_CONFIG_FILES,
	XBPS_PKG_STATE_NOT_INSTALLED
} pkg_state_t;

/**
 * @ingroup pkgstates
 *
 * Gets package state from package \a pkgname, and sets its state
 * into \a state.
 * 
 * @param[in] pkgname Package name.
 * @param[out] state Package state returned.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_get_pkg_state_installed(const char *pkgname, pkg_state_t *state);

/**
 * @ingroup pkgstates
 *
 * Gets package state from a package dictionary \a dict, and sets its
 * state into \a state.
 *
 * @param[in] dict Package dictionary.
 * @param[out] state Package state returned.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_get_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t *state);

/**
 * @ingroup pkgstates
 *
 * Sets package state \a state in package \a pkgname.
 *
 * @param[in] pkgname Package name.
 * @param[in] state Package state to be set.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_set_pkg_state_installed(const char *pkgname, pkg_state_t state);

/**
 * @ingroup pkgstates
 *
 * Sets package state \a state in package dictionary \a dict.
 *
 * @param[in] dict Package dictionary.
 * @param[in] state Package state to be set.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_set_pkg_state_dictionary(prop_dictionary_t dict, pkg_state_t state);

/**
 * @ingroup unpack
 *
 * Unpacks a binary package into specified root directory.
 *
 * @param[in] dict Package proplib dictionary as returned by a transaction.
 *
 * @return 0 on success, or an errno value otherwise.
 */
int xbps_unpack_binary_pkg(prop_dictionary_t dict);

/**
 * @ingroup util
 *
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
 * @ingroup util
 *
 * Returns a string with the sha256 hash for the file specified
 * by \a file.
 *
 * @param[in] file Path to a file.
 * @return A pointer to a malloc(3)ed string, NULL otherwise and errno
 * is set appropiately. The pointer should be free(3)d when it's no
 * longer needed.
 */
char *xbps_get_file_hash(const char *file);

/**
 * @ingroup util
 *
 * Compares the sha256 hash of the file \a file with the sha256
 * string specified by \a sha256.
 *
 * @param[in] file Path to a file.
 * @param[in] sha256 SHA256 hash to compare.
 *
 * @return 0 if \a file and \a sha256 have the same hash, ERANGE
 * if it differs, or any other errno value on error.
 */
int xbps_check_file_hash(const char *file, const char *sha256);

/**
 * @ingroup util
 *
 * Checks if a package is currently installed by matching a package
 * pattern string.
 *
 * @param[in] pkg Package pattern used to find the package.
 *
 * @return -1 on error (errno set appropiately), 0 if package pattern
 * didn't match installed package, 1 if \a pkg pattern fully
 * matched installed package.
 */
int xbps_check_is_installed_pkg(const char *pkg);

/**
 * @ingroup util
 *
 * Checks if package \a pkgname is currently installed.
 *
 * @param[in] pkgname Package name.
 *
 * @return True if \a pkgname is installed, false otherwise.
 */
bool xbps_check_is_installed_pkgname(const char *pkgname);

/**
 * @ingroup util
 *
 * Checks if the URI specified by \a uri is remote or local.
 *
 * @param[in] uri URI string.
 * 
 * @return true if URI is remote, false if local.
 */
bool xbps_check_is_repo_string_remote(const char *uri);

/**
 * @ingroup util
 *
 * Gets the full path to a binary package file as returned by a
 * package transaction dictionary \a pkgd, by looking at the
 * repository location \a repoloc.
 *
 * @param[in] pkgd Package dictionary stored in a transaction dictionary.
 * @param[in] repoloc Repository location as returned by the object
 * <em>repository</em> in the package dictionary of a transaction
 * dictionary.
 *
 * @return A pointer to a malloc(3)ed string, NULL otherwise and
 * errno is set appropiately. The pointer should be free(3)d when it's
 * no longer needed.
 */ 
char *xbps_get_binpkg_local_path(prop_dictionary_t pkgd, const char *repoloc);

/**
 * @ingroup util
 *
 * Gets the full path to a repository package index plist file, as
 * specified by \a uri.
 *
 * @param[in] uri Repository URI.
 *
 * @return A pointer to a malloc(3)d string, NULL otherwise and
 * errno is set appropiately. The pointer should be free(3)d when it's
 * no longer needed.
 */
char *xbps_get_pkg_index_plist(const char *uri);

/**
 * @ingroup util
 *
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
char *xbps_get_pkg_name(const char *pkg);

/**
 * @ingroup util
 *
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
char *xbps_get_pkgpattern_name(const char *pattern);

/**
 * @ingroup util
 *
 * Gets the package version in a package string, i.e <b>foo-2.0</b>.
 * 
 * @param[in] pkg Package string.
 *
 * @return A string with the version string, NULL if it couldn't
 * find the version component.
 */
const char *xbps_get_pkg_version(const char *pkg);

/**
 * @ingroup util
 *
 * Gets the package version of a package pattern string specified by
 * the \a pattern argument.
 *
 * @param[in] pattern A package pattern. The same rules in
 * xbps_get_pkgpattern_name() apply here.
 *
 * @return A string with the pattern version, NULL otherwise and
 * errno is set appropiately.
 */
const char *xbps_get_pkgpattern_version(const char *pattern);

/**
 * @ingroup util
 *
 * Gets the package version revision in a package string.
 *
 * @param[in] pkg Package string, i.e <b>foo-2.0_1</b>.
 *
 * @return A string with the revision number, NULL if it couldn't
 * find the revision component.
 */
const char *xbps_get_pkg_revision(const char *pkg);

/**
 * @ingroup util
 *
 * Checks if a package has run dependencies.
 *
 * @param[in] dict Package dictionary.
 *
 * @return True if package has run dependencies, false otherwise.
 */
bool xbps_pkg_has_rundeps(prop_dictionary_t dict);

/**
 * @ingroup util
 *
 * Sets the global root directory.
 *
 * @param[in] path Destination directory.
 */
void xbps_set_rootdir(const char *path);

/**
 * @ingroup util
 *
 * Gets the global root directory.
 *
 * @return A string with full path to the root directory.
 */
const char *xbps_get_rootdir(void);

/**
 * @ingroup util
 *
 * Sets globally the cache directory to store downloaded binary
 * packages. Any full path without rootdir is valid.
 *
 * @param[in] cachedir Directory to be set.
 */
void xbps_set_cachedir(const char *cachedir);

/**
 * @ingroup util
 *
 * Gets the cache directory currently used to store downloaded
 * binary packages.
 *
 * @return The path to a directory.
 */
const char *xbps_get_cachedir(void);

/**
 * @ingroup util
 *
 * Sets the flag specified in \a flags for internal use.
 *
 * @param[in] flags Flags to be set globally.
 */
void xbps_set_flags(int flags);

/**
 * @ingroup util
 *
 * Gets the flags currently set internally.
 *
 * @return An integer with flags
 */
int xbps_get_flags(void);

bool xbps_yesno(const char *, ...);
bool xbps_noyes(const char *, ...);

__END_DECLS

#endif /* !_XBPS_API_H_ */
