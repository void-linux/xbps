/*-
 * Copyright (c) 2009-2010 Juan Romero Pardines.
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
		if (rv == ENOENT)
			return trans_dict;

		return NULL;
	}

	/*
	 * Add total transaction installed/download sizes
	 * to the transaction dictionary.
	 */
	if (compute_transaction_sizes() != 0)
		return NULL;

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

struct rpool_index_data {
	prop_dictionary_t pkg_repod;
	const char *pkgname;
	const char *repo_uri;
	bool newpkgfound;
};

static int
repo_find_updated_pkg_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct rpool_index_data *rid = arg;
	prop_dictionary_t instpkgd;
	const char *repover, *instver;

	/*
	 * Get the package dictionary from current repository.
	 * If it's not there, pass to the next repository.
	 */
	rid->pkg_repod = xbps_find_pkg_in_dict_by_name(rpi->rpi_repod,
	    "packages", rid->pkgname);
	if (rid->pkg_repod == NULL) {
		if (errno && errno != ENOENT)
			return errno;

		xbps_dbg_printf("Package '%s' not found in repository "
		    "'%s'.\n", rid->pkgname, rpi->rpi_uri);
	} else {
		/*
		 * Check if version in repository is greater than
		 * the version currently installed.
		 */
		instpkgd = xbps_find_pkg_dict_installed(rid->pkgname, false);
		prop_dictionary_get_cstring_nocopy(instpkgd,
		    "version", &instver);
		prop_dictionary_get_cstring_nocopy(rid->pkg_repod,
		    "version", &repover);
		prop_object_release(instpkgd);

		if (xbps_cmpver(repover, instver) > 0) {
			xbps_dbg_printf("Found '%s-%s' (installed: %s) "
			    "in repository '%s'.\n", rid->pkgname, repover,
			    instver, rpi->rpi_uri);
			/*
			 * New package version found, exit from the loop.
			 */
			rid->newpkgfound = true;
			rid->repo_uri = rpi->rpi_uri;
			*done = true;
			return 0;
		}
		xbps_dbg_printf("Skipping '%s-%s' (installed: %s) "
		    "from repository '%s'\n", rid->pkgname, repover, instver,
		    rpi->rpi_uri);
	}

	return 0;
}

int
xbps_repository_update_pkg(const char *pkgname)
{
	prop_array_t unsorted;
	prop_dictionary_t pkgd;
	struct rpool_index_data *rid;
	int rv = 0;

	assert(pkgname != NULL);

	/*
	 * Prepare repository pool queue.
	 */
	if ((rv = xbps_repository_pool_init()) != 0)
		return rv;

	rid = calloc(1, sizeof(struct rpool_index_data));
	if (rid == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Check if package is not installed.
	 */
	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL) {
		rv = ENODEV;
		goto out;
	}
	prop_object_release(pkgd);

	/*
	 * Find out if a new package version exists in repositories.
	 */
	rid->pkgname = pkgname;
	rv = xbps_repository_pool_foreach(repo_find_updated_pkg_cb, rid);
	if (rv != 0)
		goto out;

	/*
	 * No new versions found in repository pool.
	 */
	if (rid->newpkgfound == false) {
		rv = EEXIST;
		goto out;
	}

	/*
	 * Package couldn't be found in repository pool.
	 */
	if (rid->pkg_repod == NULL) {
		rv = ENOENT;
		goto out;
	}
	/*
	 * Create the transaction dictionary.
	 */
	if ((rv = create_transaction_dictionary()) != 0)
		goto out;

	/*
	 * Set repository in pkg dictionary.
	 */
	if (!prop_dictionary_set_cstring(rid->pkg_repod,
	    "repository", rid->repo_uri)) {
		rv = errno;
		goto out;
	}

	/*
	 * Construct the dependency chain for this package.
	 */
	rv = xbps_repository_find_pkg_deps(trans_dict, rid->pkg_repod);
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
	if ((rv = set_pkg_state(rid->pkg_repod, pkgname)) != 0)
		goto out;

	/*
	 * Set trans-action obj in pkg dictionary to "update".
	 */
	if (!prop_dictionary_set_cstring_nocopy(rid->pkg_repod,
	    "trans-action", "update")) {
		rv = errno;
		goto out;
	}

	/*
	 * Added package dictionary from repository into the "unsorted"
	 * array in the transaction dictionary.
	 */
	if (!prop_array_add(unsorted, rid->pkg_repod)) {
		rv = errno;
		goto out;
	}

out:
	if (rid)
		free(rid);

	xbps_repository_pool_release();

	return rv;
}

static int
repo_find_new_pkg_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct rpool_index_data *rid = arg;
	const char *pkgver;

	/*
	 * Finds a package dictionary from a repository pkg-index dictionary.
	 */
	rid->pkg_repod = xbps_find_pkg_in_dict_by_pattern(
	    rpi->rpi_repod, "packages", rid->pkgname);
	if (rid->pkg_repod == NULL) {
		if (errno && errno != ENOENT)
			return errno;
	} else {
		prop_dictionary_get_cstring_nocopy(rid->pkg_repod,
		    "pkgver", &pkgver);
		xbps_dbg_printf("Found package '%s' from repository %s.\n",
		    pkgver, rpi->rpi_uri);

		rid->repo_uri = rpi->rpi_uri;
		*done = true;
	}

	return 0;
}

int
xbps_repository_install_pkg(const char *pkg)
{
	prop_dictionary_t origin_pkgrd = NULL;
	prop_array_t unsorted;
	struct rpool_index_data *rid;
	const char *pkgname;
	int rv = 0;

	assert(pkg != NULL);

	if ((rv = xbps_repository_pool_init()) != 0)
		return rv;

	rid = calloc(1, sizeof(struct rpool_index_data));
	if (rid == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Get the package dictionary from current repository.
	 * If it's not there, pass to the next repository.
	 */
	rid->pkgname = pkg;
	rv = xbps_repository_pool_foreach(repo_find_new_pkg_cb, rid);
	if (rv != 0)
		goto out;

	/*
	 * Package couldn't be found in repository pool... EAGAIN.
	 */
	if (rid->pkg_repod == NULL) {
		rv = EAGAIN;
		goto out;
	}

	/*
	 * Create the transaction dictionary.
	 */
	if ((rv = create_transaction_dictionary()) != 0) 
		goto out;

	/*
	 * Check that this pkg hasn't been added previously into
	 * the transaction.
	 */
	if (xbps_find_pkg_in_dict_by_pattern(trans_dict,
	    "unsorted_deps", pkg))
		goto out;

	/*
	 * Set repository location in pkg dictionary.
	 */
	if (!prop_dictionary_set_cstring(rid->pkg_repod,
	    "repository", rid->repo_uri)) {
		rv = EINVAL;
		goto out;
	}
	origin_pkgrd = prop_dictionary_copy(rid->pkg_repod);
	prop_dictionary_get_cstring_nocopy(rid->pkg_repod, "pkgname", &pkgname);

	/*
	 * Prepare required package dependencies and add them into the
	 * "unsorted" array in transaction dictionary.
	 */
	rv = xbps_repository_find_pkg_deps(trans_dict, rid->pkg_repod);
	if (rv != 0)
		goto out;

	/*
	 * Add required package dictionary into the unsorted array and
	 * set package state as not yet installed.
	 */
	unsorted = prop_dictionary_get(trans_dict, "unsorted_deps");
	if (unsorted == NULL) {
		rv = EINVAL;
		goto out;
	}
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
	 * Add the pkg dictionary from repository's index dictionary into
	 * the "unsorted" array in transaction dictionary.
	 */
	if (!prop_array_add(unsorted, origin_pkgrd)) {
		rv = errno;
		goto out;
	}

out:
	if (rid)
		free(rid);

	if (origin_pkgrd)
		prop_object_release(origin_pkgrd);

	xbps_repository_pool_release();

	return rv;
}
