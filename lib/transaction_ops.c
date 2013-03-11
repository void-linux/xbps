/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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
#include <fnmatch.h>

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
trans_find_pkg(struct xbps_handle *xhp, const char *pkg, int action)
{
	prop_dictionary_t pkg_pkgdb = NULL, pkg_repod;
	prop_array_t unsorted;
	const char *repoloc, *repopkgver, *instpkgver, *reason;
	char *pkgname;
	int rv = 0;
	pkg_state_t state = 0;
	bool autoinst = false;

	assert(pkg != NULL);

	/*
	 * Find out if the pkg has been found in repository pool.
	 */
	if (action == TRANS_INSTALL) {
		reason = "install";
		if (((pkg_repod = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkg_repod = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL)) {
			/* not found */
			return ENOENT;
		}
	} else {
		if ((pkg_pkgdb = xbps_pkgdb_get_pkg(xhp, pkg)) == NULL)
			return ENODEV;

		reason = "update";
		if ((pkg_repod = xbps_rpool_get_pkg(xhp, pkg)) == NULL) {
			/* not found */
			return ENOENT;
		}
	}
	prop_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &repopkgver);
	prop_dictionary_get_cstring_nocopy(pkg_repod, "repository", &repoloc);

	if (action == TRANS_UPDATE) {
		/*
		 * Compare installed version vs best pkg available in repos.
		 */
		prop_dictionary_get_cstring_nocopy(pkg_pkgdb,
		    "pkgver", &instpkgver);
		if (xbps_cmpver(repopkgver, instpkgver) <= 0) {
			xbps_dbg_printf(xhp, "[rpool] Skipping `%s' "
			    "(installed: %s) from repository `%s'\n",
			    repopkgver, instpkgver, repoloc);
			return EEXIST;
		}
		/* respect current install mode from pkgdb */
		prop_dictionary_get_bool(pkg_pkgdb, "automatic-install",
		    &autoinst);
		prop_dictionary_set_bool(pkg_repod, "automatic-install",
		    autoinst);
	}
	/*
	 * Prepare transaction dictionary.
	 */
	if ((rv = xbps_transaction_init(xhp)) != 0)
		return rv;

	unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");
	/*
	 * Find out if package has matched conflicts.
	 */
	xbps_pkg_find_conflicts(xhp, unsorted, pkg_repod);

	/*
	 * Find out if package being updated matches the one already
	 * in transaction, in that case ignore it.
	 */
	if (action == TRANS_UPDATE) {
		if (xbps_find_pkg_in_array(unsorted, repopkgver)) {
			xbps_dbg_printf(xhp, "[update] `%s' already queued in "
			    "transaction.\n", repopkgver);
			return EEXIST;
		}
	}

	if ((rv = xbps_repository_find_deps(xhp, unsorted, pkg_repod)) != 0)
		return rv;

	pkgname = xbps_pkg_name(repopkgver);
	assert(pkgname);
	/*
	 * Set package state in dictionary with same state than the
	 * package currently uses, otherwise not-installed.
	 */
	if ((rv = xbps_pkg_state_installed(xhp, pkgname, &state)) != 0) {
		if (rv != ENOENT) {
			free(pkgname);
			return rv;
		}
		/* Package not installed, don't error out */
		state = XBPS_PKG_STATE_NOT_INSTALLED;
	}
	free(pkgname);

	if ((rv = xbps_set_pkg_state_dictionary(pkg_repod, state)) != 0)
		return rv;

	if ((action == TRANS_INSTALL) && (state == XBPS_PKG_STATE_UNPACKED))
		reason = "configure";
	else if (state == XBPS_PKG_STATE_NOT_INSTALLED)
		reason = "install";

	/*
	 * Set transaction obj in pkg dictionary to "install", "configure"
	 * or "update".
	 */
	if (!prop_dictionary_set_cstring_nocopy(pkg_repod,
	    "transaction", reason))
		return EINVAL;

	/*
	 * Add the pkg dictionary from repository's index dictionary into
	 * the "unsorted" queue.
	 */
	if (!prop_array_add(unsorted, pkg_repod))
		return EINVAL;

	xbps_dbg_printf(xhp, "%s: added into the transaction (%s).\n",
	    repopkgver, repoloc);

	return 0;
}

int
xbps_transaction_update_packages(struct xbps_handle *xhp)
{
	prop_dictionary_t pkgd;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgver, *holdpkg;
	char *pkgname;
	bool foundhold = false, newpkg_found = false;
	int rv = 0;
	size_t x;

	if ((rv = xbps_pkgdb_init(xhp)) != 0)
		return rv;

	iter = prop_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = prop_object_iterator_next(iter))) {
		pkgd = prop_dictionary_get_keysym(xhp->pkgdb, obj);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);

		for (x = 0; x < cfg_size(xhp->cfg, "PackagesOnHold"); x++) {
			holdpkg = cfg_getnstr(xhp->cfg, "PackagesOnHold", x);
			if ((strcmp(holdpkg, pkgname) == 0) ||
			    (fnmatch(holdpkg, pkgname, FNM_PERIOD) == 0)) {
				xbps_dbg_printf(xhp, "[rpool] package `%s' "
				    "on hold, ignoring updates.\n", pkgname);
				foundhold = true;
				break;
			}
		}
		if (foundhold) {
			foundhold = false;
			continue;
		}
		rv = trans_find_pkg(xhp, pkgname, TRANS_UPDATE);
		if (rv == 0)
			newpkg_found = true;
		else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
			/*
			 * missing pkg or installed version is greater than or
			 * equal than pkg in repositories.
			 */
			rv = 0;
		}
		free(pkgname);
	}
	prop_object_iterator_release(iter);

	return newpkg_found ? rv : EEXIST;
}

int
xbps_transaction_update_pkg(struct xbps_handle *xhp, const char *pkg)
{
	return trans_find_pkg(xhp, pkg, TRANS_UPDATE);
}

int
xbps_transaction_install_pkg(struct xbps_handle *xhp, const char *pkg,
			     bool reinstall)
{
	prop_dictionary_t pkgd = NULL;
	pkg_state_t state;

	if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkg)) ||
	    (pkgd = xbps_pkgdb_get_virtualpkg(xhp, pkg))) {
		if (xbps_pkg_state_dictionary(pkgd, &state) != 0)
			return EINVAL;
		if ((state == XBPS_PKG_STATE_INSTALLED) && !reinstall) {
			/* error out if pkg installed and no reinstall */
			return EEXIST;
		}
	}

	return trans_find_pkg(xhp, pkg, TRANS_INSTALL);
}

int
xbps_transaction_remove_pkg(struct xbps_handle *xhp,
			    const char *pkgname,
			    bool recursive)
{
	prop_dictionary_t pkgd;
	prop_array_t unsorted, orphans, orphans_pkg, reqby;
	prop_object_t obj;
	const char *pkgver;
	size_t count;
	int rv = 0;

	assert(pkgname != NULL);

	if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
		/* pkg not installed */
		return ENOENT;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((rv = xbps_transaction_init(xhp)) != 0)
		return rv;

	unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");

	if (!recursive)
		goto rmpkg;
	/*
	 * If recursive is set, find out which packages would be orphans
	 * if the supplied package were already removed.
	 */
	if ((orphans_pkg = prop_array_create()) == NULL)
		return ENOMEM;

	prop_array_set_cstring_nocopy(orphans_pkg, 0, pkgname);
	orphans = xbps_find_pkg_orphans(xhp, orphans_pkg);
	prop_object_release(orphans_pkg);
	if (prop_object_type(orphans) != PROP_TYPE_ARRAY)
		return EINVAL;

	count = prop_array_count(orphans);
	while (count--) {
		obj = prop_array_get(orphans, count);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_set_cstring_nocopy(obj, "transaction", "remove");
		prop_array_add(unsorted, obj);
		xbps_dbg_printf(xhp, "%s: added into transaction (remove).\n", pkgver);
	}
	reqby = xbps_pkgdb_get_pkg_revdeps(xhp, pkgver);
	/*
	 * If target pkg is required by any installed pkg, the client must be aware
	 * of this to take appropiate action.
	 */
	if ((prop_object_type(reqby) == PROP_TYPE_ARRAY) &&
	    (prop_array_count(reqby) > 0))
		rv = EEXIST;

	prop_object_release(orphans);
	return rv;

rmpkg:
	/*
	 * Add pkg dictionary into the transaction unsorted queue.
	 */
	prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	prop_dictionary_set_cstring_nocopy(pkgd, "transaction", "remove");
	prop_array_add(unsorted, pkgd);
	xbps_dbg_printf(xhp, "%s: added into transaction (remove).\n", pkgver);
	reqby = xbps_pkgdb_get_pkg_revdeps(xhp, pkgver);
	/*
	 * If target pkg is required by any installed pkg, the client must be aware
	 * of this to take appropiate action.
	 */
	if ((prop_object_type(reqby) == PROP_TYPE_ARRAY) &&
	    (prop_array_count(reqby) > 0))
		rv = EEXIST;

	return rv;
}

int
xbps_transaction_autoremove_pkgs(struct xbps_handle *xhp)
{
	prop_array_t orphans, unsorted;
	prop_object_t obj;
	const char *pkgver;
	size_t count;
	int rv = 0;

	orphans = xbps_find_pkg_orphans(xhp, NULL);
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
	if ((rv = xbps_transaction_init(xhp)) != 0)
		goto out;

	unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");
	/*
	 * Add pkg orphan dictionary into the transaction unsorted queue.
	 */
	while (count--) {
		obj = prop_array_get(orphans, count);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_set_cstring_nocopy(obj,
		    "transaction", "remove");
		prop_array_add(unsorted, obj);
		xbps_dbg_printf(xhp, "%s: added (remove).\n", pkgver);
	}
out:
	if (orphans != NULL)
		prop_object_release(orphans);

	return rv;
}
