/*-
 * Copyright (c) 2009-2019 Juan Romero Pardines.
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
	TRANS_UPDATE,
	TRANS_REINSTALL
};

static int
trans_find_pkg(struct xbps_handle *xhp, const char *pkg, bool reinstall,
		bool hold)
{
	xbps_dictionary_t pkg_pkgdb = NULL, pkg_repod = NULL;
	xbps_array_t pkgs;
	const char *repoloc, *repopkgver, *instpkgver, *reason;
	char *pkgname;
	int action = 0, rv = 0;
	pkg_state_t state = 0;
	bool autoinst = false, repolock = false;

	assert(pkg != NULL);

	/*
	 * Find out if pkg is installed first.
	 */
	if ((pkgname = xbps_pkg_name(pkg))) {
		pkg_pkgdb = xbps_pkgdb_get_pkg(xhp, pkgname);
		free(pkgname);
	} else {
		pkg_pkgdb = xbps_pkgdb_get_pkg(xhp, pkg);
	}
	/*
	 * Find out if the pkg has been found in repository pool.
	 */
	if (pkg_pkgdb == NULL) {
		/* pkg not installed, perform installation */
		action = TRANS_INSTALL;
		reason = "install";
		if (((pkg_repod = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
		    ((pkg_repod = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL)) {
			/* not found */
			return ENOENT;
		}
	} else {
		/* pkg installed, update or reinstall */
		if (!reinstall) {
			action = TRANS_UPDATE;
			reason = "update";
		} else {
			action = TRANS_REINSTALL;
			reason = "install";
		}
		xbps_dictionary_get_bool(pkg_pkgdb, "repolock", &repolock);
		if (repolock) {
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

	if (action == TRANS_UPDATE) {
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
			xbps_dbg_printf(xhp, "[rpool] Skipping `%s' "
			    "(installed: %s) from repository `%s'\n",
			    repopkgver, instpkgver, repoloc);
			return EEXIST;
		}
	} else if (action == TRANS_REINSTALL) {
		/*
		 * For reinstallation check if installed version is less than
		 * or equal to the pkg in repos, if true, continue with reinstallation;
		 * otherwise perform an update.
		 */
		xbps_dictionary_get_cstring_nocopy(pkg_pkgdb, "pkgver", &instpkgver);
		if (xbps_cmpver(repopkgver, instpkgver) == 1) {
			action = TRANS_UPDATE;
			reason = "update";
		}
	}

	if (pkg_pkgdb) {
		/*
		 * If pkg is already installed, respect some properties.
		 */
		if (xbps_dictionary_get_bool(pkg_pkgdb, "automatic-install", &autoinst))
			xbps_dictionary_set_bool(pkg_repod, "automatic-install", autoinst);
		if (xbps_dictionary_get_bool(pkg_pkgdb, "repolock", &repolock))
			xbps_dictionary_set_bool(pkg_repod, "repolock", repolock);
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
	if (action == TRANS_UPDATE) {
		if (xbps_find_pkg_in_array(pkgs, repopkgver, NULL)) {
			xbps_dbg_printf(xhp, "[update] `%s' already queued in "
			    "transaction.\n", repopkgver);
			return EEXIST;
		}
	}

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
	if ((rv = xbps_set_pkg_state_dictionary(pkg_repod, state)) != 0) {
		free(pkgname);
		return rv;
	}

	if ((action == TRANS_INSTALL) && (state == XBPS_PKG_STATE_UNPACKED))
		reason = "configure";
	else if (state == XBPS_PKG_STATE_NOT_INSTALLED)
		reason = "install";
	else if ((action == TRANS_UPDATE) && hold)
		reason = "hold";

	/*
	 * Set transaction obj reason.
	 */
	if (!xbps_dictionary_set_cstring_nocopy(pkg_repod,
	    "transaction", reason)) {
		free(pkgname);
		return EINVAL;
	}
	if ((rv = xbps_transaction_store(xhp, pkgs, pkg_repod, reason, false)) != 0) {
		free(pkgname);
		return rv;
	}
	free(pkgname);
	return 0;
}

typedef struct {
	const char *pkgname;
	const char *pkgver;
	xbps_dictionary_t record;
} outmoded_search_t;

static int
find_outmoded_record(struct xbps_repo *repo, void *arg, bool *done)
{
	xbps_dictionary_t outmoded = NULL;
	xbps_dictionary_t record = NULL;
	outmoded_search_t *search = arg;
	const char *pkgpattern = NULL;

	if (!xbps_dictionary_get_dict(repo->idxmeta, "outmoded", &outmoded))
		return 0;
	if (!xbps_dictionary_get_dict(outmoded, search->pkgname, &record))
		return 0;
	if (!xbps_dictionary_get_cstring_nocopy(record, "pattern", &pkgpattern))
		return 0;
	if (xbps_pkgpattern_match(search->pkgver, pkgpattern) == 1)
	{
		search->record = record;
		*done = true;
	}
	return 0;
}

static int
trans_find_outmoded(struct xbps_handle *xhp, xbps_array_t pkgs) {
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	char *old_pkgname = NULL;
	int rv = 0;

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		outmoded_search_t search;
		xbps_dictionary_t old_pkg_in_trans = NULL;
		xbps_dictionary_t old_pkg_in_pkgdb = NULL;
		xbps_array_t to_install = NULL;
		const char *old_pattern = NULL;
		const char *old_pkgver = NULL;
		unsigned int i = 0;
		bool hold = false;
		bool instd_auto = true;

		old_pkg_in_pkgdb = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		if (!xbps_dictionary_get_cstring_nocopy(old_pkg_in_pkgdb, "pkgver", &old_pkgver)) {
			xbps_dbg_printf(xhp, "pkgdb item has no pkgver");
			continue;
		}
		xbps_dictionary_get_bool(old_pkg_in_pkgdb, "hold", &hold);
		if (hold) {
			xbps_dbg_printf(xhp, "[rpool] package `%s' "
			    "on hold, skipped checking if it is outmoded.\n", old_pkgver);
			continue;
		}
		old_pkgname = xbps_pkg_name(old_pkgver);
		assert(old_pkgname);
		memset(&search, 0, sizeof(search));
		search.pkgname = old_pkgname;
		search.pkgver = old_pkgver;
		search.record = NULL;
		rv = xbps_rpool_foreach(xhp, find_outmoded_record, &search);
		if (rv) {
			xbps_dbg_printf(xhp, "%s: trans_find_outmoded %s: %d\n", __func__, old_pkgver, rv);
			goto out;
		}
		if (search.record == NULL) {
			free(old_pkgname);
			old_pkgname = NULL;
			continue;
		}

		xbps_dictionary_get_cstring_nocopy(search.record, "pattern", &old_pattern);
		if (pkgs != NULL) {
			old_pkg_in_trans = xbps_find_pkg_in_array(pkgs, old_pattern, NULL);
		}

		if (old_pkg_in_trans != NULL)
		{
			const char *old_transaction = NULL;

			xbps_dictionary_get_cstring_nocopy(old_pkg_in_trans,
			    "transaction", &old_transaction);
			if (strcmp(old_transaction, "remove") != 0)
			{
				xbps_dictionary_set_cstring_nocopy(old_pkg_in_trans,
				    "transaction", "remove");
				xbps_dictionary_get_bool(old_pkg_in_trans,
				    "automatic-install", &instd_auto);
			}
		} else {
			xbps_dictionary_get_bool(old_pkg_in_pkgdb, "automatic-install", &instd_auto);
			if (xbps_transaction_remove_pkg(xhp, old_pkgname, false)) {
				rv = EINVAL;
				goto out;
			}
		}

		to_install = xbps_dictionary_get(search.record, "to_install");

		for (i = 0; i < xbps_array_count(to_install); ++i) {
			const char *new_pkgname = NULL;
			xbps_dictionary_t new_pkg_in_trans = NULL;

			xbps_array_get_cstring_nocopy(to_install, i, &new_pkgname);
			rv = xbps_transaction_install_pkg(xhp, new_pkgname, false);
			if (rv) {
				goto out;
			}

			if (pkgs == NULL) {
				pkgs = xbps_dictionary_get(xhp->transd, "packages");
			}
			new_pkg_in_trans = xbps_find_pkg_in_array(pkgs, new_pkgname, NULL);
			assert(new_pkg_in_trans);

			if (!instd_auto) {
				xbps_dictionary_set_bool(new_pkg_in_trans,
				    "automatic-install", false);
			}

			xbps_dbg_printf(xhp,
			    "Package `%s' in transaction will be installed in place of "
			    "outmoded `%s', matched with `%s'\n",
			    new_pkgname, old_pkgver, old_pattern);
		}

		xbps_dbg_printf(xhp,
		    "Package `%s' will be outmoded by %d package(s)\n",
		    old_pkgver, i);
		free(old_pkgname);
		old_pkgname = NULL;
	}

out:
	xbps_object_iterator_release(iter);
	free(old_pkgname);
	return rv;
}

/*
 * Returns 1 if there's an update, 0 if none or -1 on error.
 */
static int
xbps_autoupdate(struct xbps_handle *xhp)
{
	xbps_array_t rdeps;
	xbps_dictionary_t pkgd;
	const char *pkgver;
	char *pkgname;
	int rv;

	/*
	 * Check if there's a new update for XBPS before starting
	 * another transaction.
	 */
	if (((pkgd = xbps_pkgdb_get_pkg(xhp, "xbps")) == NULL) &&
	    ((pkgd = xbps_pkgdb_get_virtualpkg(xhp, "xbps")) == NULL))
		return 0;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);

	rv = trans_find_pkg(xhp, pkgname, false, false);
	free(pkgname);

	xbps_dbg_printf(xhp, "%s: trans_find_pkg xbps: %d\n", __func__, rv);

	if (rv == 0) {
		/* a new xbps version is available, check its revdeps */
		rdeps = xbps_pkgdb_get_pkg_revdeps(xhp, "xbps");
		for (unsigned int i = 0; i < xbps_array_count(rdeps); i++)  {
			const char *curpkgver = NULL;
			char *curpkgn;

			xbps_array_get_cstring_nocopy(rdeps, i, &curpkgver);
			xbps_dbg_printf(xhp, "%s: processing revdep %s\n", __func__, curpkgver);

			curpkgn = xbps_pkg_name(curpkgver);
			assert(curpkgn);
			rv = trans_find_pkg(xhp, curpkgn, false, false);
			free(curpkgn);
			xbps_dbg_printf(xhp, "%s: trans_find_pkg revdep %s: %d\n", __func__, curpkgver, rv);
			if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV)
				return -1;
		}
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
	xbps_array_t pkgs;
	xbps_dictionary_t pkgd;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *pkgver;
	char *pkgname;
	bool hold, newpkg_found = false;
	int rv = 0;

	rv = xbps_autoupdate(xhp);
	switch (rv) {
	case 1:
		/* xbps needs to be updated, don't add any other package */
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		break;
	}

	iter = xbps_dictionary_iterator(xhp->pkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		hold = false;
		pkgd = xbps_dictionary_get_keysym(xhp->pkgdb, obj);
		if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
			continue;
		xbps_dictionary_get_bool(pkgd, "hold", &hold);
		if (hold) {
			xbps_dbg_printf(xhp, "[rpool] package `%s' "
			    "on hold, ignoring updates.\n", pkgver);
		}
		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);
		rv = trans_find_pkg(xhp, pkgname, false, hold);
		xbps_dbg_printf(xhp, "%s: trans_find_pkg %s: %d\n", __func__, pkgver, rv);
		if (rv == 0) {
			newpkg_found = true;
		} else if (rv == ENOENT || rv == EEXIST || rv == ENODEV) {
			/*
			 * missing pkg or installed version is greater than or
			 * equal than pkg in repositories.
			 */
			rv = 0;
		}
		free(pkgname);
	}
	xbps_object_iterator_release(iter);

	pkgs = xbps_dictionary_get(xhp->transd, "packages");
	rv = trans_find_outmoded(xhp, pkgs);
	newpkg_found = true;
	xbps_dbg_printf(xhp, "%s: trans_find_outmoded %s: %d\n", __func__, pkgver, rv);

	return newpkg_found ? rv : EEXIST;
}

int
xbps_transaction_update_pkg(struct xbps_handle *xhp, const char *pkg)
{
	xbps_array_t rdeps;
	int rv;

	rv = xbps_autoupdate(xhp);
	xbps_dbg_printf(xhp, "%s: xbps_autoupdate %d\n", __func__, rv);
	switch (rv) {
	case 1:
		/* xbps needs to be updated, don't add any other package */
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		/* no update */
		break;
	}

	rdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkg);
	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++)  {
		const char *curpkgver = NULL;
		char *curpkgn;

		xbps_array_get_cstring_nocopy(rdeps, i, &curpkgver);
		curpkgn = xbps_pkg_name(curpkgver);
		assert(curpkgn);
		rv = trans_find_pkg(xhp, curpkgn, false, false);
		free(curpkgn);
		xbps_dbg_printf(xhp, "%s: trans_find_pkg %s: %d\n", __func__, curpkgver, rv);
		if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV)
			return rv;
	}
	rv = trans_find_pkg(xhp, pkg, false, false);
	xbps_dbg_printf(xhp, "%s: trans_find_pkg %s: %d\n", __func__, pkg, rv);
	return rv;
}

int
xbps_transaction_install_pkg(struct xbps_handle *xhp, const char *pkg,
			     bool reinstall)
{
	xbps_array_t rdeps;
	int rv;

	rv = xbps_autoupdate(xhp);
	switch (rv) {
	case 1:
		/* xbps needs to be updated, don't add any other package */
		return 0;
	case -1:
		/* error */
		return EINVAL;
	default:
		/* no update */
		break;
	}

	rdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkg);
	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++)  {
		const char *curpkgver = NULL;
		char *curpkgn;

		xbps_array_get_cstring_nocopy(rdeps, i, &curpkgver);
		curpkgn = xbps_pkg_name(curpkgver);
		assert(curpkgn);
		rv = trans_find_pkg(xhp, curpkgn, false, false);
		free(curpkgn);
		xbps_dbg_printf(xhp, "%s: trans_find_pkg %s: %d\n", __func__, curpkgver, rv);
		if (rv && rv != ENOENT && rv != EEXIST && rv != ENODEV)
			return rv;
	}
	rv = trans_find_pkg(xhp, pkg, reinstall, false);
	xbps_dbg_printf(xhp, "%s: trans_find_pkg %s: %d\n", __func__, pkg, rv);
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
	const char *pkgver;
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
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_set_cstring_nocopy(obj, "transaction", "remove");
		if ((rv = xbps_transaction_store(xhp, pkgs, obj, "remove", false)) != 0)
			return EINVAL;
		xbps_dbg_printf(xhp, "%s: added into transaction (remove).\n", pkgver);
	}
	return rv;

rmpkg:
	/*
	 * Add pkg dictionary into the transaction pkgs queue.
	 */
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	xbps_dictionary_set_cstring_nocopy(pkgd, "transaction", "remove");
	if ((rv = xbps_transaction_store(xhp, pkgs, pkgd, "remove", false)) != 0)
		return EINVAL;
	xbps_dbg_printf(xhp, "%s: added into transaction (remove).\n", pkgver);
	return rv;
}

int
xbps_transaction_autoremove_pkgs(struct xbps_handle *xhp)
{
	xbps_array_t orphans, pkgs;
	xbps_object_t obj;
	const char *pkgver;
	int rv = 0;

	orphans = xbps_find_pkg_orphans(xhp, NULL);
	if (xbps_array_count(orphans) == 0) {
		/* no orphans? we are done */
		rv = ENOENT;
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
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_set_cstring_nocopy(obj,
		    "transaction", "remove");
		xbps_array_add(pkgs, obj);
		xbps_dbg_printf(xhp, "%s: added (remove).\n", pkgver);
	}
out:
	if (orphans != NULL)
		xbps_object_release(orphans);

	return rv;
}
