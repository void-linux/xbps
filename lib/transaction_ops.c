/*-
 * Copyright (c) 2009-2020 Juan Romero Pardines.
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
static int
trans_find_pkg(struct xbps_handle *xhp, const char *pkg, bool force)
{
	xbps_dictionary_t pkg_pkgdb = NULL, pkg_repod = NULL;
	xbps_object_t obj;
	xbps_array_t pkgs;
	pkg_state_t state = 0;
	xbps_trans_type_t ttype;
	const char *repoloc, *repopkgver, *instpkgver, *pkgname;
	char buf[XBPS_NAME_SIZE] = {0};
	bool autoinst = false;
	int rv = 0;

	assert(pkg != NULL);

	/*
	 * Find out if pkg is installed first.
	 */
	if (xbps_pkg_name(buf, sizeof(buf), pkg)) {
		pkg_pkgdb = xbps_pkgdb_get_pkg(xhp, buf);
	} else {
		pkg_pkgdb = xbps_pkgdb_get_pkg(xhp, pkg);
	}

	if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
		pkg_pkgdb = NULL;
		ttype = XBPS_TRANS_DOWNLOAD;
	}

	/*
	 * Find out if the pkg has been found in repository pool.
	 */
	if (pkg_pkgdb == NULL) {
		/* pkg not installed, perform installation */
		ttype = XBPS_TRANS_INSTALL;
		if (((pkg_repod = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkg_repod = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL)) {
			/* not found */
			return ENOENT;
		}
	} else {
		if (force) {
			ttype = XBPS_TRANS_REINSTALL;
		} else {
			ttype = XBPS_TRANS_UPDATE;
		}
		if (xbps_dictionary_get(pkg_pkgdb, "repolock")) {
			struct xbps_repo *repo;
			/* find update from repo */
			xbps_dictionary_get_cstring_nocopy(pkg_pkgdb, "repository", &repoloc);
			assert(repoloc);
			if ((repo = xbps_regget_repo(xhp, repoloc)) == NULL) {
				/* not found */
				return ENOENT;
			}
			pkg_repod = xbps_repo_get_pkg(repo, pkg);
		} else {
			/* find update from rpool */
			pkg_repod = xbps_rpool_get_pkg(xhp, pkg);
		}
		if (pkg_repod == NULL) {
			/* not found */
			return ENOENT;
		}
	}

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &repopkgver);

	if (ttype == XBPS_TRANS_UPDATE) {
		/*
		 * Compare installed version vs best pkg available in repos
		 * for pkg updates.
		 */
		xbps_dictionary_get_cstring_nocopy(pkg_pkgdb,
		    "pkgver", &instpkgver);
		if (xbps_cmpver(repopkgver, instpkgver) <= 0 &&
		    !xbps_pkg_reverts(pkg_repod, instpkgver)) {
			xbps_dictionary_get_cstring_nocopy(pkg_repod,
			    "repository", &repoloc);
			xbps_dbg_printf("[rpool] Skipping `%s' "
			    "(installed: %s) from repository `%s'\n",
			    repopkgver, instpkgver, repoloc);
			return EEXIST;
		}
	} else if (ttype == XBPS_TRANS_REINSTALL) {
		/*
		 * For reinstallation check if installed version is less than
		 * or equal to the pkg in repos, if true, continue with reinstallation;
		 * otherwise perform an update.
		 */
		xbps_dictionary_get_cstring_nocopy(pkg_pkgdb, "pkgver", &instpkgver);
		if (xbps_cmpver(repopkgver, instpkgver) == 1) {
			ttype = XBPS_TRANS_UPDATE;
		}
	}

	if (pkg_pkgdb) {
		/*
		 * If pkg is already installed, respect some properties.
		 */
		if ((obj = xbps_dictionary_get(pkg_pkgdb, "automatic-install")))
			xbps_dictionary_set(pkg_repod, "automatic-install", obj);
		if ((obj = xbps_dictionary_get(pkg_pkgdb, "hold")))
			xbps_dictionary_set(pkg_repod, "hold", obj);
		if ((obj = xbps_dictionary_get(pkg_pkgdb, "repolock")))
			xbps_dictionary_set(pkg_repod, "repolock", obj);
	}
	/*
	 * Prepare transaction dictionary.
	 */
	if ((rv = xbps_transaction_init(xhp)) != 0)
		return rv;

	pkgs = xbps_dictionary_get(xhp->transd, "packages");
	/*
	 * Find out if package being updated matches the one already
	 * in transaction, in that case ignore it.
	 */
	if (ttype == XBPS_TRANS_UPDATE) {
		if (xbps_find_pkg_in_array(pkgs, repopkgver, 0)) {
			xbps_dbg_printf("[update] `%s' already queued in "
			    "transaction.\n", repopkgver);
			return EEXIST;
		}
	}

	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname)) {
		return EINVAL;
	}
	/*
	 * Set package state in dictionary with same state than the
	 * package currently uses, otherwise not-installed.
	 */
	if ((rv = xbps_pkg_state_installed(xhp, pkgname, &state)) != 0) {
		if (rv != ENOENT) {
			return rv;
		}
		/* Package not installed, don't error out */
		state = XBPS_PKG_STATE_NOT_INSTALLED;
	}
	if ((rv = xbps_set_pkg_state_dictionary(pkg_repod, state)) != 0) {
		return rv;
	}

	if (state == XBPS_PKG_STATE_NOT_INSTALLED)
		ttype = XBPS_TRANS_INSTALL;

	if (!force && xbps_dictionary_get(pkg_repod, "hold"))
		ttype = XBPS_TRANS_HOLD;

	/*
	 * Store pkgd from repo into the transaction.
	 */
	if (!xbps_transaction_pkg_type_set(pkg_repod, ttype)) {
		return EINVAL;
	}

	/*
	 * Set automatic-install to true if it was requested and this is a new install.
	 */
	if (ttype == XBPS_TRANS_INSTALL)
		autoinst = xhp->flags & XBPS_FLAG_INSTALL_AUTO;

	if (!xbps_transaction_store(xhp, pkgs, pkg_repod, autoinst)) {
		return EINVAL;
	}

	return 0;
}

/*
 * Returns 1 if there's an update, 0 if none or -1 on error.
 */
static int
xbps_autoupdate(struct xbps_handle *xhp)
{
	xbps_array_t rdeps;
	xbps_dictionary_t pkgd;
	const char *pkgver = NULL, *pkgname = NULL;
	int rv;

	/*
	 * Check if there's a new update for XBPS before starting
	 * another transaction.
	 */
	if (((pkgd = xbps_pkgdb_get_pkg(xhp, "xbps")) == NULL) &&
	    ((pkgd = xbps_pkgdb_get_virtualpkg(xhp, "xbps")) == NULL))
		return 0;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
		return EINVAL;
	}
	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname)) {
		return EINVAL;
	}

	rv = trans_find_pkg(xhp, pkgname, false);

	xbps_dbg_printf("%s: trans_find_pkg xbps: %d\n", __func__, rv);

	if (rv == 0) {
		if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
			return 0;
		}
		/* a new xbps version is available, check its revdeps */
		rdeps = xbps_pkgdb_get_pkg_revdeps(xhp, "xbps");
		for (unsigned int i = 0; i < xbps_array_count(rdeps); i++)  {
			const char *curpkgver = NULL;
			char curpkgn[XBPS_NAME_SIZE] = {0};

			xbps_array_get_cstring_nocopy(rdeps, i, &curpkgver);
			xbps_dbg_printf("%s: processing revdep %s\n", __func__, curpkgver);

			if (!xbps_pkg_name(curpkgn, sizeof(curpkgn), curpkgver)) {
				abort();
			}
			rv = trans_find_pkg(xhp, curpkgn, false);
			xbps_dbg_printf("%s: trans_find_pkg revdep %s: %d\n", __func__, curpkgver, rv);
			if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV)
				return -1;
		}
		/*
		 * Set XBPS_FLAG_FORCE_REMOVE_REVDEPS to ignore broken
		 * reverse dependencies in xbps_transaction_prepare().
		 *
		 * This won't skip revdeps of the xbps pkg, rather other
		 * packages in rootdir that could be broken indirectly.
		 *
		 * A sysup transaction after updating xbps should fix them
		 * again.
		 */
		xhp->flags |= XBPS_FLAG_FORCE_REMOVE_REVDEPS;
		return 1;
	} else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
		/* no update */
		return 0;
	} else {
		/* error */
		return -1;
	}

	return 0;
}

int
xbps_transaction_update_packages(struct xbps_handle *xhp)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_dictionary_t pkgd;
	bool newpkg_found = false;
	int rv = 0;

	rv = xbps_autoupdate(xhp);
	switch (rv) {
	case 1:
		/* xbps needs to be updated, don't allow any other update */
		return EBUSY;
	case -1:
		/* error */
		return EINVAL;
	default:
		break;
	}

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		const char *pkgver = NULL;
		char pkgname[XBPS_NAME_SIZE] = {0};

		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver)) {
			continue;
		}
		if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			break;
		}
		rv = trans_find_pkg(xhp, pkgname, false);
		xbps_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv == 0) {
			newpkg_found = true;
		} else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
			/*
			 * missing pkg or installed version is greater than or
			 * equal than pkg in repositories.
			 */
			rv = 0;
		}
	}
	xbps_object_iterator_release(iter);

	return newpkg_found ? rv : EEXIST;
}

int
xbps_transaction_update_pkg(struct xbps_handle *xhp, const char *pkg, bool force)
{
	xbps_array_t rdeps;
	int rv;

	rv = xbps_autoupdate(xhp);
	xbps_dbg_printf("%s: xbps_autoupdate %d\n", __func__, rv);
	switch (rv) {
	case 1:
		/* xbps needs to be updated, only allow xbps to be updated */
		if (strcmp(pkg, "xbps"))
			return EBUSY;
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		/* no update */
		break;
	}

	/* update its reverse dependencies */
	rdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkg);
	if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
		rdeps = NULL;
	}
	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++)  {
		const char *pkgver = NULL;
		char pkgname[XBPS_NAME_SIZE] = {0};

		if (!xbps_array_get_cstring_nocopy(rdeps, i, &pkgver)) {
			rv = EINVAL;
			break;
		}
		if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			break;
		}
		rv = trans_find_pkg(xhp, pkgname, false);
		xbps_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV) {
			return rv;
		}
	}
	/* add pkg repod */
	rv = trans_find_pkg(xhp, pkg, force);
	xbps_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkg, rv);
	return rv;
}

int
xbps_transaction_install_pkg(struct xbps_handle *xhp, const char *pkg, bool force)
{
	xbps_array_t rdeps;
	int rv;

	rv = xbps_autoupdate(xhp);
	switch (rv) {
	case 1:
		/* xbps needs to be updated, only allow xbps to be updated */
		if (strcmp(pkg, "xbps"))
			return EBUSY;
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		/* no update */
		break;
	}

	/* update its reverse dependencies */
	rdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkg);
	if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
		rdeps = NULL;
	}
	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++)  {
		const char *pkgver = NULL;
		char pkgname[XBPS_NAME_SIZE] = {0};

		if (!xbps_array_get_cstring_nocopy(rdeps, i, &pkgver)) {
			rv = EINVAL;
			break;
		}
		if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
			rv = EINVAL;
			break;
		}
		rv = trans_find_pkg(xhp, pkgname, false);
		xbps_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV) {
			return rv;
		}
	}
	rv = trans_find_pkg(xhp, pkg, force);
	xbps_dbg_printf("%s: trans_find_pkg %s: %d\n", __func__, pkg, rv);
	return rv;
}

int
xbps_transaction_remove_pkg(struct xbps_handle *xhp,
			    const char *pkgname,
			    bool recursive)
{
	xbps_dictionary_t pkgd;
	xbps_array_t pkgs, orphans, orphans_pkg;
	xbps_object_t obj;
	int rv = 0;

	assert(xhp);
	assert(pkgname);

	if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
		/* pkg not installed */
		return ENOENT;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((rv = xbps_transaction_init(xhp)) != 0)
		return rv;

	pkgs = xbps_dictionary_get(xhp->transd, "packages");

	if (!recursive)
		goto rmpkg;
	/*
	 * If recursive is set, find out which packages would be orphans
	 * if the supplied package were already removed.
	 */
	if ((orphans_pkg = xbps_array_create()) == NULL)
		return ENOMEM;

	xbps_array_set_cstring_nocopy(orphans_pkg, 0, pkgname);
	orphans = xbps_find_pkg_orphans(xhp, orphans_pkg);
	xbps_object_release(orphans_pkg);
	if (xbps_object_type(orphans) != XBPS_TYPE_ARRAY)
		return EINVAL;

	for (unsigned int i = 0; i < xbps_array_count(orphans); i++) {
		obj = xbps_array_get(orphans, i);
		xbps_transaction_pkg_type_set(obj, XBPS_TRANS_REMOVE);
		if (!xbps_transaction_store(xhp, pkgs, obj, false)) {
			return EINVAL;
		}
	}
	xbps_object_release(orphans);
	return rv;

rmpkg:
	/*
	 * Add pkg dictionary into the transaction pkgs queue.
	 */
	xbps_transaction_pkg_type_set(pkgd, XBPS_TRANS_REMOVE);
	if (!xbps_transaction_store(xhp, pkgs, pkgd, false)) {
		return EINVAL;
	}
	return rv;
}

int
xbps_transaction_autoremove_pkgs(struct xbps_handle *xhp)
{
	xbps_array_t orphans, pkgs;
	xbps_object_t obj;
	int rv = 0;

	orphans = xbps_find_pkg_orphans(xhp, NULL);
	if (xbps_array_count(orphans) == 0) {
		/* no orphans? we are done */
		goto out;
	}
	/*
	 * Prepare transaction dictionary and missing deps array.
	 */
	if ((rv = xbps_transaction_init(xhp)) != 0)
		goto out;

	pkgs = xbps_dictionary_get(xhp->transd, "packages");
	/*
	 * Add pkg orphan dictionary into the transaction pkgs queue.
	 */
	for (unsigned int i = 0; i < xbps_array_count(orphans); i++) {
		obj = xbps_array_get(orphans, i);
		xbps_transaction_pkg_type_set(obj, XBPS_TRANS_REMOVE);
		if (!xbps_transaction_store(xhp, pkgs, obj, false)) {
			rv = EINVAL;
			goto out;
		}
	}
out:
	if (orphans)
		xbps_object_release(orphans);

	return rv;
}

xbps_trans_type_t
xbps_transaction_pkg_type(xbps_dictionary_t pkg_repod)
{
	uint8_t r;

	if (xbps_object_type(pkg_repod) != XBPS_TYPE_DICTIONARY)
		return 0;

	if (!xbps_dictionary_get_uint8(pkg_repod, "transaction", &r))
		return 0;

	return r;
}

bool
xbps_transaction_pkg_type_set(xbps_dictionary_t pkg_repod, xbps_trans_type_t ttype)
{
	uint8_t r;

	if (xbps_object_type(pkg_repod) != XBPS_TYPE_DICTIONARY)
		return false;

	switch (ttype) {
	case XBPS_TRANS_INSTALL:
	case XBPS_TRANS_UPDATE:
	case XBPS_TRANS_CONFIGURE:
	case XBPS_TRANS_REMOVE:
	case XBPS_TRANS_REINSTALL:
	case XBPS_TRANS_HOLD:
	case XBPS_TRANS_DOWNLOAD:
		break;
	default:
		return false;
	}
	r = ttype;
	if (!xbps_dictionary_set_uint8(pkg_repod, "transaction", r))
		return false;

	return true;
}
