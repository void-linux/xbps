/*-
 * Copyright (c) 2012-2020 Juan Romero Pardines.
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

#include "xbps/xbps_array.h"
#include "xbps_api_impl.h"

static int
pkg_conflicts_trans(struct xbps_handle *xhp, xbps_array_t array,
		xbps_dictionary_t pkg_repod)
{
	xbps_array_t pkg_cflicts, trans_cflicts;
	xbps_dictionary_t pkgd, tpkgd;
	xbps_trans_type_t ttype;
	const char *repopkgver, *repopkgname;
	char *buf;

	assert(xhp);
	assert(array);
	assert(pkg_repod);

	pkg_cflicts = xbps_dictionary_get(pkg_repod, "conflicts");
	if (xbps_array_count(pkg_cflicts) == 0)
		return 0;

	ttype = xbps_transaction_pkg_type(pkg_repod);
	if (ttype == XBPS_TRANS_HOLD || ttype == XBPS_TRANS_REMOVE)
		return 0;

	trans_cflicts = xbps_dictionary_get(xhp->transd, "conflicts");
	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &repopkgver))
		abort();
	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &repopkgname))
		abort();

	for (unsigned int i = 0; i < xbps_array_count(pkg_cflicts); i++) {
		const char *pkgver = NULL, *pkgname = NULL, *cfpkg = NULL;

		if (!xbps_array_get_cstring_nocopy(pkg_cflicts, i, &cfpkg))
			abort();

		/*
		 * Check if current pkg conflicts with an installed package.
		 */
		if ((pkgd = xbps_pkgdb_get_pkg(xhp, cfpkg)) ||
		    (pkgd = xbps_pkgdb_get_virtualpkg(xhp, cfpkg))) {
			/* If the conflicting pkg is on hold, ignore it */
			if (xbps_dictionary_get(pkgd, "hold"))
				continue;

			/* Ignore itself */
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname))
				abort();
			if (strcmp(pkgname, repopkgname) == 0) {
				continue;
			}
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
				abort();
			/*
			 * If there's a pkg for the conflict in transaction,
			 * ignore it.
			 */
			if ((tpkgd = xbps_find_pkg_in_array(array, pkgname, 0))) {
				ttype = xbps_transaction_pkg_type(tpkgd);
				if (ttype == XBPS_TRANS_INSTALL ||
				    ttype == XBPS_TRANS_UPDATE ||
				    ttype == XBPS_TRANS_REMOVE ||
				    ttype == XBPS_TRANS_HOLD) {
					continue;
				}
			}
			xbps_dbg_printf("found conflicting installed "
			    "pkg %s with pkg in transaction %s "
			    "(matched by %s [trans])\n", pkgver, repopkgver, cfpkg);
			buf = xbps_xasprintf("CONFLICT: %s with "
			    "installed pkg %s (matched by %s)",
			    repopkgver, pkgver, cfpkg);
			if (!xbps_array_add_cstring(trans_cflicts, buf))
				return xbps_error_oom();
			continue;
		}
		/*
		 * Check if current pkg conflicts with any pkg in transaction.
		 */
		if ((pkgd = xbps_find_pkg_in_array(array, cfpkg, 0)) ||
		    (pkgd = xbps_find_virtualpkg_in_array(xhp, array, cfpkg, 0))) {
			/* ignore pkgs to be removed or on hold */
			ttype = xbps_transaction_pkg_type(pkgd);
			if (ttype == XBPS_TRANS_REMOVE || ttype == XBPS_TRANS_HOLD)
				continue;
			/* ignore itself */
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname))
				abort();
			if (strcmp(pkgname, repopkgname) == 0)
				continue;
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
				break;
			xbps_dbg_printf("found conflicting pkgs in "
			    "transaction %s <-> %s (matched by %s [trans])\n",
			    pkgver, repopkgver, cfpkg);
			buf = xbps_xasprintf("CONFLICT: %s with "
			   "%s in transaction (matched by %s)",
			   repopkgver, pkgver, cfpkg);
			if (!buf)
				return xbps_error_oom();
			if (!xbps_array_add_cstring_nocopy(trans_cflicts, buf))
				return xbps_error_oom();
			continue;
		}
	}
	return 0;
}

static int
pkgdb_conflicts_cb(struct xbps_handle *xhp, xbps_object_t obj,
		const char *key UNUSED, void *arg, bool *done UNUSED)
{
	xbps_array_t pkg_cflicts, trans_cflicts, pkgs = arg;
	xbps_dictionary_t pkgd;
	xbps_trans_type_t ttype;
	const char *repopkgver, *repopkgname;
	char *buf;

	pkg_cflicts = xbps_dictionary_get(obj, "conflicts");
	if (xbps_array_count(pkg_cflicts) == 0)
		return 0;

	if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &repopkgver))
		abort();
	if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgname", &repopkgname))
		abort();

	// XXX: this should really be a hashtable/dictionary lookup
	/* if a pkg is in the transaction, ignore the one from pkgdb */
	if (xbps_find_pkg_in_array(pkgs, repopkgname, 0))
		return 0;

	trans_cflicts = xbps_dictionary_get(xhp->transd, "conflicts");

	for (unsigned int i = 0; i < xbps_array_count(pkg_cflicts); i++) {
		const char *pkgver = NULL, *pkgname = NULL, *cfpkg = NULL;

		if (!xbps_array_get_cstring_nocopy(pkg_cflicts, i, &cfpkg))
			abort();

		if ((pkgd = xbps_find_pkg_in_array(pkgs, cfpkg, 0)) ||
		    (pkgd = xbps_find_virtualpkg_in_array(xhp, pkgs, cfpkg, 0))) {
			/* ignore pkgs to be removed or on hold */
			ttype = xbps_transaction_pkg_type(pkgd);
			if (ttype == XBPS_TRANS_REMOVE || ttype == XBPS_TRANS_HOLD) {
				continue;
			}
			/* ignore itself */
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname))
				abort();
			if (strcmp(pkgname, repopkgname) == 0) {
				continue;
			}
			if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
				abort();
			xbps_dbg_printf("found conflicting pkgs in "
			    "transaction %s <-> %s (matched by %s [pkgdb])\n",
			    pkgver, repopkgver, cfpkg);
			buf = xbps_xasprintf("CONFLICT: %s with "
			   "%s in transaction (matched by %s)",
			   repopkgver, pkgver, cfpkg);
			if (!buf)
				return xbps_error_oom();
			if (!xbps_array_add_cstring_nocopy(trans_cflicts, buf))
				return xbps_error_oom();
			continue;
		}
	}
	return 0;
}

int HIDDEN
xbps_transaction_check_conflicts(struct xbps_handle *xhp, xbps_array_t pkgs)
{
	xbps_array_t array;
	int r;

	/* find conflicts in transaction */
	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++) {
		r = pkg_conflicts_trans(xhp, pkgs, xbps_array_get(pkgs, i));
		if (r < 0)
			return r;
	}

	/* find conflicts in pkgdb */
	r = xbps_pkgdb_foreach_cb_multi(xhp, pkgdb_conflicts_cb, pkgs);
	if (r < 0)
		return r;
	else if (r > 0)
		return -r;

	array = xbps_dictionary_get(xhp->transd, "conflicts");
	if (xbps_array_count(array) == 0)
		xbps_dictionary_remove(xhp->transd, "conflicts");

	return 0;
}
