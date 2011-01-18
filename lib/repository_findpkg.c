/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

/**
 * @file lib/repository_findpkg.c
 * @brief Repository transaction handling routines
 * @defgroup repo_pkgs Repository transaction handling functions
 *
 * The following image shows off the full transaction dictionary returned
 * by xbps_repository_get_transaction_dict().
 *
 * @image html images/xbps_transaction_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: The transaction dictionary.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *  - <b>Green bg box</b>: possible value set in the object, only one of them
 *    will be set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static prop_dictionary_t trans_dict;
static bool trans_dict_initialized;

/*
 * This creates the transaction dictionary with two arrays, one for
 * dependencies not yet sorted and another one for missing dependencies.
 *
 * Before returning the dictionary to the caller, package dependencies in
 * the "unsorted_deps" array will be sorted and moved to another
 * array called "packages". If there are no missing dependencies, the
 * "missing_deps" array will be removed.
 *
 */
static int
create_transaction_dictionary(void)
{
	prop_array_t unsorted, missing;
	int rv = 0;

	if (trans_dict_initialized)
		return 0;

	trans_dict = prop_dictionary_create();
	if (trans_dict == NULL)
		return ENOMEM;

	missing = prop_array_create();
	if (missing == NULL) {
		rv = ENOMEM;
		goto fail;
	}

        unsorted = prop_array_create();
        if (unsorted == NULL) {
		rv = ENOMEM;
		goto fail2;
	}

	if (!xbps_add_obj_to_dict(trans_dict, missing, "missing_deps")) {
		rv = EINVAL;
		goto fail3;
        }
        if (!xbps_add_obj_to_dict(trans_dict, unsorted, "unsorted_deps")) {
		rv = EINVAL;
		goto fail3;
	}

	trans_dict_initialized = true;

	return rv;

fail3:
	prop_object_release(unsorted);
fail2:
	prop_object_release(missing);
fail:
	prop_object_release(trans_dict);

	return rv;
}

static int
compute_transaction_sizes(void)
{
	prop_object_iterator_t iter;
	prop_object_t obj;
	uint64_t tsize = 0, dlsize = 0, instsize = 0;
	int rv = 0;
	const char *tract;

	iter = xbps_get_array_iter_from_dict(trans_dict, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "trans-action", &tract);
		/*
		 * Skip pkgs that need to be configured.
		 */
		if (strcmp(tract, "configure") == 0)
			continue;

		prop_dictionary_get_uint64(obj, "filename-size", &tsize);
		dlsize += tsize;
		tsize = 0;
		prop_dictionary_get_uint64(obj, "installed_size", &tsize);
		instsize += tsize;
		tsize = 0;
	}

	/*
	 * Add object in transaction dictionary with total installed
	 * size that it will take.
	 */
	if (!prop_dictionary_set_uint64(trans_dict,
	    "total-installed-size", instsize)) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Add object in transaction dictionary with total download
	 * size that needs to be sucked in.
	 */
	if (!prop_dictionary_set_uint64(trans_dict,
	    "total-download-size", dlsize)) {
		rv = EINVAL;
		goto out;
	}
out:
	prop_object_iterator_release(iter);

	return rv;
}

static int
set_pkg_state(prop_dictionary_t pkgd, const char *pkgname)
{
	pkg_state_t state = 0;
	int rv = 0;

	rv = xbps_set_pkg_state_dictionary(pkgd, XBPS_PKG_STATE_NOT_INSTALLED);
	if (rv != 0)
		return rv;
	/*
	 * Overwrite package state in dictionary if it was unpacked
	 * previously.
	 */
	rv = xbps_get_pkg_state_installed(pkgname, &state);
	if (rv == 0) {
		if (state == XBPS_PKG_STATE_INSTALLED)
			return 0;

		if ((rv = xbps_set_pkg_state_dictionary(pkgd, state)) != 0)
			return rv;
	} else if (rv == ENOENT)
		rv = 0;
	
	return rv;
}

prop_dictionary_t
xbps_repository_get_transaction_dict(void)
{
	int rv = 0;

	if (trans_dict_initialized == false) {
		errno = ENXIO;
		return NULL;
	}

	/*
	 * Sort package dependencies if necessary.
	 */
	if ((rv = xbps_sort_pkg_deps(trans_dict)) != 0) {
		errno = rv;
		/*
		 * If there are missing deps (ENOENT)
		 * return the dictionary, the client should always
		 * check if that's the case.
		 */
		return trans_dict;
	}

	/*
	 * Add total transaction installed/download sizes
	 * to the transaction dictionary.
	 */
	if ((rv = compute_transaction_sizes()) != 0) {
		errno = rv;
		return NULL;
	}

	/*
	 * Remove the "missing_deps" array now that it's not needed.
	 */
	prop_dictionary_remove(trans_dict, "missing_deps");

	return trans_dict;
}

int
xbps_repository_update_allpkgs(void)
{
	prop_dictionary_t dict;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname;
	int rv = 0;
	bool newpkg_found = false;

	/*
	 * Prepare dictionary with all registered packages.
	 */
	dict = xbps_regpkgdb_dictionary_get();
	if (dict == NULL)
		return errno;

	iter = xbps_get_array_iter_from_dict(dict, "packages");
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Find out if there is a newer version for all currently
	 * installed packages.
	 */
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		if ((rv = xbps_repository_update_pkg(pkgname)) != 0) {
			if (rv == ENOENT || rv == EEXIST)
				continue;

			xbps_dbg_printf("[update-all] '%s' returned: %s\n",
			    pkgname, strerror(rv));
			goto out;
		}
		newpkg_found = true;
	}
	prop_object_iterator_release(iter);

	if (newpkg_found)
		rv = 0;
	else
		rv = ENXIO;

out:
	xbps_regpkgdb_dictionary_release();

	return rv;
}

int
xbps_repository_update_pkg(const char *pkgname)
{
	prop_array_t unsorted;
	prop_dictionary_t pkg_repod;
	int rv = 0;

	assert(pkgname != NULL);
	/*
	 * Check if package is not installed.
	 */
	pkg_repod = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkg_repod == NULL) {
		rv = ENODEV;
		goto out;
	}
	prop_object_release(pkg_repod);

	/*
	 * Find out if a new package version exists in repositories.
	 */
	pkg_repod = xbps_repository_pool_find_pkg(pkgname, false, true);
	xbps_dbg_printf("xbps_repository_pool_find_pkg returned %s for %s\n",
	    strerror(errno), pkgname);
	if (pkg_repod == NULL) {
		rv = errno;
		errno = 0;
		goto out;
	}
	/*
	 * Create the transaction dictionary.
	 */
	if ((rv = create_transaction_dictionary()) != 0)
		goto out;

	/*
	 * Construct the dependency chain for this package.
	 */
	rv = xbps_repository_find_pkg_deps(trans_dict, pkg_repod);
	if (rv != 0)
		goto out;

	/*
	 * Add required package dictionary into the packages
	 * dictionary.
	 */
	unsorted = prop_dictionary_get(trans_dict, "unsorted_deps");
	if (unsorted == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Always set "not-installed" package state. Will be overwritten
	 * to its correct state later.
	 */
	if ((rv = set_pkg_state(pkg_repod, pkgname)) != 0)
		goto out;

	/*
	 * Set trans-action obj in pkg dictionary to "update".
	 */
	if (!prop_dictionary_set_cstring_nocopy(pkg_repod,
	    "trans-action", "update")) {
		rv = errno;
		goto out;
	}

	/*
	 * Added package dictionary from repository into the "unsorted"
	 * array in the transaction dictionary.
	 */
	if (!prop_array_add(unsorted, pkg_repod)) {
		rv = errno;
		goto out;
	}

out:
	if (pkg_repod)
		prop_object_release(pkg_repod);

	return rv;
}

int
xbps_repository_install_pkg(const char *pkg)
{
	prop_dictionary_t pkg_repod = NULL, origin_pkgrd = NULL;
	prop_array_t unsorted;
	const char *pkgname;
	int rv = 0;

	assert(pkg != NULL);

	/*
	 * Get the package dictionary from current repository.
	 * If it's not there, pass to the next repository.
	 */
	pkg_repod = xbps_repository_pool_find_pkg(pkg, true, false);
	if (pkg_repod == NULL) {
		/*
		 * Package couldn't be found in repository pool... EAGAIN.
		 */
		rv = EAGAIN;
		goto out;
	}
	/*
	 * Create the transaction dictionary.
	 */
	if ((rv = create_transaction_dictionary()) != 0) 
		goto out;

	origin_pkgrd = prop_dictionary_copy(pkg_repod);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	/*
	 * Check that this pkg hasn't been added previously into
	 * the transaction.
	 */
	if (xbps_find_pkg_in_dict_by_pattern(trans_dict,
	    "unsorted_deps", pkg)) {
		xbps_dbg_printf("package '%s' already queued in transaction\n",
		    pkg);
		goto out;
	}
	/*
	 * Prepare required package dependencies and add them into the
	 * "unsorted" array in transaction dictionary.
	 */
	rv = xbps_repository_find_pkg_deps(trans_dict, origin_pkgrd);
	if (rv != 0)
		goto out;

	if ((rv = set_pkg_state(origin_pkgrd, pkgname)) != 0)
		goto out;

	/*
	 * Set trans-action obj in pkg dictionary to "install".
	 */
	if (!prop_dictionary_set_cstring_nocopy(origin_pkgrd,
	    "trans-action", "install")) {
		rv = EINVAL;
		goto out;
	}

	/*
	 * Add required package dictionary into the unsorted array and
	 * set package state as not yet installed.
	 */
	unsorted = prop_dictionary_get(trans_dict, "unsorted_deps");
	if (unsorted == NULL) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Add the pkg dictionary from repository's index dictionary into
	 * the "unsorted" array in transaction dictionary.
	 */
	if (!prop_array_add(unsorted, origin_pkgrd)) {
		rv = errno;
		goto out;
	}

out:
	if (pkg_repod)
		prop_object_release(pkg_repod);
	if (origin_pkgrd)
		prop_object_release(origin_pkgrd);

	xbps_dbg_printf("%s: returned %s for '%s'\n\n",
	    __func__, strerror(rv), pkg);

	return rv;
}
