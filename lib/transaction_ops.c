/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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
 * @file lib/transaction_ops.c
 * @brief Transaction handling routines
 * @defgroup transaction Transaction handling functions
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
enum {
	TRANS_INSTALL = 1,
	TRANS_UPDATE
};

static int
transaction_find_pkg(const char *pattern, int action)
{
	prop_dictionary_t pkg_pkgdb, pkg_repod = NULL;
	prop_dictionary_t transd;
	prop_array_t mdeps, unsorted;
	const char *pkgname, *pkgver, *repoloc, *repover, *instver, *reason;
	int rv = 0;
	bool bypattern, bestpkg;
	pkg_state_t state = 0;

	assert(pattern != NULL);

	if (action == TRANS_INSTALL) {
		/* install */
		bypattern = true;
		bestpkg = false;
		reason = "install";
	} else {
		/* update */
		pkg_pkgdb = xbps_find_pkg_dict_installed(pattern, false);
		if (pkg_pkgdb == NULL) {
			rv = ENODEV;
			goto out;
		}
		bypattern = false;
		bestpkg = true;
		reason = "update";
	}

	/*
	 * Find out if the pkg has been found in repository pool.
	 */
	pkg_repod = xbps_repository_pool_find_pkg(pattern,
	    bypattern, bestpkg);
	if (pkg_repod == NULL) {
		pkg_repod =
		    xbps_repository_pool_find_virtualpkg(pattern, bypattern);
		if (pkg_repod == NULL) {
			/* not found */
			rv = errno;
			errno = 0;
			goto out;
		}
	}
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "repository", &repoloc);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);

	if (bestpkg) {
		/*
		 * Compare installed version vs best pkg available in repos.
		 */
		prop_dictionary_get_cstring_nocopy(pkg_pkgdb,
		    "version", &instver);
		prop_dictionary_get_cstring_nocopy(pkg_repod,
		    "version", &repover);
		prop_object_release(pkg_pkgdb);
		if (xbps_cmpver(repover, instver) <= 0) {
			xbps_dbg_printf("[rpool] Skipping `%s' "
			    "(installed: %s) from repository `%s'\n",
			    pkgver, instver, repoloc);
			rv = EEXIST;
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

static int
update_pkgs_cb(prop_object_t obj, void *arg, bool *done)
{
	const char *pkgname;
	bool *newpkg_found = arg;
	int rv = 0;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	rv = xbps_transaction_update_pkg(pkgname);
	if (rv == 0)
		*newpkg_found = true;
	else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
		/*
		 * missing pkg or installed version is greater than or
		 * equal than pkg in repositories.
		 */
		rv = 0;
	}

	return rv;
}

int
xbps_transaction_update_packages(void)
{
	bool newpkg_found = false;
	int rv;

	rv = xbps_pkgdb_foreach_cb(update_pkgs_cb, &newpkg_found);
	if (!newpkg_found)
		rv = EEXIST;

	return rv;
}

int
xbps_transaction_update_pkg(const char *pkgname)
{
	return transaction_find_pkg(pkgname, TRANS_UPDATE);
}

int
xbps_transaction_install_pkg(const char *pkgpattern)
{
	return transaction_find_pkg(pkgpattern, TRANS_INSTALL);
}

int
xbps_transaction_remove_pkg(const char *pkgname, bool recursive)
{
	prop_dictionary_t transd, pkgd;
	prop_array_t mdeps, orphans, orphans_pkg, unsorted, reqby;
	prop_object_t obj;
	const char *pkgver;
	size_t count;
	int rv = 0;

	assert(pkgname != NULL);

	if ((pkgd = xbps_pkgdb_get_pkgd(pkgname, false)) == NULL) {
		/* pkg not installed */
		return ENOENT;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((transd = xbps_transaction_dictionary_get()) == NULL) {
		rv = ENXIO;
		goto out;
	}
	if ((mdeps = xbps_transaction_missingdeps_get()) == NULL) {
		rv = ENXIO;
		goto out;
	}
	unsorted = prop_dictionary_get(transd, "unsorted_deps");
	if (!recursive)
		goto rmpkg;
	/*
	 * If recursive is set, find out which packages would be orphans
	 * if the supplied package were already removed.
	 */
	orphans_pkg = prop_array_create();
	if (orphans_pkg == NULL) {
		rv = ENOMEM;
		goto out;
	}

	prop_array_set_cstring_nocopy(orphans_pkg, 0, pkgname);
	orphans = xbps_find_pkg_orphans(orphans_pkg);
	prop_object_release(orphans_pkg);
	if (prop_object_type(orphans) != PROP_TYPE_ARRAY) {
		rv = EINVAL;
		goto out;
	}

	count = prop_array_count(orphans);
	while (count--) {
		obj = prop_array_get(orphans, count);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_set_cstring_nocopy(obj, "transaction", "remove");
		prop_array_add(unsorted, obj);
		xbps_dbg_printf("%s: added into transaction (remove).\n", pkgver);
	}
	prop_object_release(orphans);
rmpkg:
	/*
	 * Add pkg dictionary into the unsorted_deps array.
	 */
	prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	prop_dictionary_set_cstring_nocopy(pkgd, "transaction", "remove");
	prop_array_add(unsorted, pkgd);
	xbps_dbg_printf("%s: added into transaction (remove).\n", pkgver);
	reqby = prop_dictionary_get(pkgd, "requiredby");
	/*
	 * If target pkg is required by any installed pkg, the client must be aware
	 * of this to take appropiate action.
	 */
	if ((prop_object_type(reqby) == PROP_TYPE_ARRAY) &&
	    (prop_array_count(reqby) > 0))
		rv = EEXIST;

out:
	prop_object_release(pkgd);

	return rv;
}

int
xbps_transaction_autoremove_pkgs(void)
{
	prop_dictionary_t transd;
	prop_array_t orphans, mdeps, unsorted;
	prop_object_t obj;
	const char *pkgver;
	size_t count;
	int rv = 0;

	orphans = xbps_find_pkg_orphans(NULL);
	if (prop_object_type(orphans) != PROP_TYPE_ARRAY)
		return EINVAL;

	count = prop_array_count(orphans);
	if (count == 0) {
		/* no orphans? we are done */
		rv = ENOENT;
		goto out;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((transd = xbps_transaction_dictionary_get()) == NULL) {
		rv = ENXIO;
		goto out;
	}
	if ((mdeps = xbps_transaction_missingdeps_get()) == NULL) {
		rv = ENXIO;
		goto out;
	}
	/*
	 * Add pkg orphan dictionary into the unsorted_deps array.
	 */
	unsorted = prop_dictionary_get(transd, "unsorted_deps");
	while (count--) {
		obj = prop_array_get(orphans, count);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_set_cstring_nocopy(obj,
		    "transaction", "remove");
		prop_array_add(unsorted, obj);
		xbps_dbg_printf("%s: added into transaction (remove).\n",
		    pkgver);
	}
out:
	if (prop_object_type(orphans) == PROP_TYPE_ARRAY)
		prop_object_release(orphans);
	return rv;
}
