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

#include "xbps_api_impl.h"

/**
 * @file lib/repository_findpkg.c
 * @brief Repository package handling routines
 * @defgroup repo_pkgs Repository package handling functions
 *
 * The following image shows off the full transaction dictionary returned
 * by xbps_transaction_prepare().
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
static int
repository_find_pkg(const char *pattern, const char *reason)
{
	prop_dictionary_t pkg_repod = NULL;
	prop_dictionary_t transd;
	prop_array_t mdeps, unsorted;
	const char *pkgname, *pkgver, *repoloc;
	int rv = 0;
	bool bypattern = false, bestpkg;
	pkg_state_t state = 0;

	assert(pattern != NULL);
	assert(reason != NULL);

	if (strcmp(reason, "install") == 0) {
		/* install */
		bypattern = true;
		bestpkg = false;
	} else {
		/* update */
		pkg_repod = xbps_find_pkg_dict_installed(pattern, false);
		if (pkg_repod == NULL) {
			rv = ENODEV;
			goto out;
		}
		prop_object_release(pkg_repod);
		pkg_repod = NULL;
		bestpkg = true;
	}

	xbps_dbg_printf("%s: pattern %s reason %s\n", __func__, pattern, reason);
	/*
	 * Find out if the pkg has been found in repository pool.
	 */
	pkg_repod = xbps_repository_pool_find_pkg(pattern,
	    bypattern, bestpkg);
	if (pkg_repod == NULL) {
		if (bestpkg == false) {
			pkg_repod = xbps_repository_pool_find_virtualpkg(
			    pattern, bypattern, bestpkg);
		}
		if (pkg_repod == NULL) {
			/* not found */
			rv = errno;
			errno = 0;
			goto out;
		}
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((transd = xbps_transaction_dictionary_get()) == NULL) {
		rv = EINVAL;
		goto out;
	}
	if ((mdeps = xbps_transaction_missingdeps_get()) == NULL) {
		rv = EINVAL;
		goto out;
	}

	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "repository", &repoloc);
	/*
	 * Prepare required package dependencies and add them into the
	 * "unsorted" array in transaction dictionary.
	 */
	if (xbps_pkg_has_rundeps(pkg_repod)) {
		rv = xbps_repository_find_pkg_deps(transd, mdeps, pkg_repod);
		if (rv != 0)
			goto out;
	}
	/*
	 * Set package state in dictionary with same state than the
	 * package currently uses, otherwise not-installed.
	 */
	if ((rv = xbps_pkg_state_installed(pkgname, &state)) != 0) {
		if (rv != ENOENT)
			goto out;
		/* Package not installed, don't error out */
		state = XBPS_PKG_STATE_NOT_INSTALLED;
	}
	if ((rv = xbps_set_pkg_state_dictionary(pkg_repod, state)) != 0)
		goto out;

	if (state == XBPS_PKG_STATE_UNPACKED)
		reason = "configure";
	else if (state == XBPS_PKG_STATE_NOT_INSTALLED ||
		 state == XBPS_PKG_STATE_HALF_UNPACKED)
		reason = "install";

	/*
	 * Set transaction obj in pkg dictionary to "install", "configure"
	 * or "update".
	 */
	if (!prop_dictionary_set_cstring_nocopy(pkg_repod,
	    "transaction", reason)) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Add required package dictionary into the unsorted array.
	 */
	unsorted = prop_dictionary_get(transd, "unsorted_deps");
	if (unsorted == NULL) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Add the pkg dictionary from repository's index dictionary into
	 * the "unsorted" array in transaction dictionary.
	 */
	if (!prop_array_add(unsorted, pkg_repod)) {
		rv = errno;
		goto out;
	}
	xbps_dbg_printf("%s: added into the transaction (%s).\n",
	    pkgver, repoloc);

out:
	if (prop_object_type(pkg_repod) == PROP_TYPE_DICTIONARY)
		prop_object_release(pkg_repod);

	return rv;
}

int
xbps_repository_update_packages(void)
{
	struct xbps_handle *xhp;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname;
	int rv = 0;
	bool newpkg_found = false;

	xhp = xbps_handle_get();
	if (xhp->regpkgdb_dictionary == NULL)
		return ENOENT;

	iter = xbps_array_iter_from_dict(xhp->regpkgdb_dictionary, "packages");
	if (iter == NULL)
		return ENOENT;
	/*
	 * Find out if there is a newer version for all currently
	 * installed packages.
	 */
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		if ((rv = xbps_repository_update_pkg(pkgname)) != 0) {
			if (rv == ENOENT || rv == EEXIST) {
				/*
				 * missing pkg or installed version is
				 * greater than or equal than pkg
				 * in repositories.
				 */
				rv = 0;
				continue;
			}

			xbps_dbg_printf("[update-all] '%s' returned: %s\n",
			    pkgname, strerror(rv));
			break;
		}
		newpkg_found = true;
	}
	prop_object_iterator_release(iter);

	if (!newpkg_found)
		rv = EEXIST;

	return rv;
}

int
xbps_repository_update_pkg(const char *pkgname)
{
	return repository_find_pkg(pkgname, "update");
}

int
xbps_repository_install_pkg(const char *pkgpattern)
{
	return repository_find_pkg(pkgpattern, "install");
}
