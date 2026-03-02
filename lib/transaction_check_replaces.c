/*-
 * Copyright (c) 2011-2020 Juan Romero Pardines.
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

#include <libgen.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include "xbps.h"
#include "xbps/xbps_dictionary.h"
#include "xbps_api_impl.h"

static int
replace_in_transaction(
    xbps_dictionary_t pkgd, const char *pattern, const char *pkgver)
{
	const char *curpkgver = NULL;
	int r;

	// already being removed
	if (xbps_transaction_pkg_type(pkgd) == XBPS_TRANS_REMOVE)
		return 0;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &curpkgver))
		xbps_unreachable();

	// XXX: ???
	if (!xbps_match_virtual_pkg_in_dict(pkgd, pattern) &&
	    !xbps_pkgpattern_match(curpkgver, pattern))
		return 0;

	if (!xbps_dictionary_set_bool(pkgd, "replaced", true))
		return xbps_error_oom();
	r = transaction_package_set_action(pkgd, XBPS_TRANS_REMOVE);
	if (r < 0)
		return r;

	xbps_verbose_printf(
	    "Package `%s' will be replaced by `%s'\n", curpkgver, pkgver);
	xbps_dbg_printf(
	    "[replaces] package `%s' in transaction will be replaced by `%s', "
	    "matched with `%s'\n",
	    curpkgver, pkgver, pattern);
	return 0;
}

static int
replace_in_pkgdb(struct xbps_handle *xhp, xbps_dictionary_t pkgd,
    const char *pattern, const char *pkgver)
{
	const char *curpkgver = NULL;
	int r;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &curpkgver))
		xbps_unreachable();

	r = transaction_store(xhp, pkgd, XBPS_TRANS_REMOVE, false, true);
	if (r < 0)
		return r;

	xbps_verbose_printf(
	    "Installed package `%s' will be replaced by `%s'\n", curpkgver, pkgver);
	xbps_dbg_printf(
	    "[replaces] installed package `%s' will be replaced by `%s', "
	    "matched with `%s'\n",
	    curpkgver, pkgver, pattern);
	return 0;
}

/*
 * Processes the array of pkg dictionaries in "pkgs" to
 * find matching package replacements via "replaces" pkg obj.
 *
 * This array contains the unordered list of packages in
 * the transaction dictionary.
 */
int HIDDEN
transaction_check_replaces(struct xbps_handle *xhp, xbps_array_t pkgs)
{
	for (unsigned int i = 0; i < xbps_array_count(pkgs); i++) {
		xbps_array_t replaces;
		xbps_object_t pkgd;
		const char *pkgver = NULL;
		int r;

		pkgd = xbps_array_get(pkgs, i);
		if (!pkgd)
			xbps_unreachable();

		replaces = xbps_dictionary_get(pkgd, "replaces");
		if (!replaces || xbps_array_count(replaces) == 0)
			continue;

		if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver))
			xbps_unreachable();

		for (unsigned int j = 0; j < xbps_array_count(replaces); j++) {
			bool autoinst = false;
			xbps_dictionary_t instd, transd;
			const char *pattern = NULL;
			const char *reppkgname = NULL;
			bool hold = false;

			if(!xbps_array_get_cstring_nocopy(replaces, j, &pattern))
				xbps_unreachable();

			// check if the package is installed or going to be installed...
			if (!(instd = xbps_pkgdb_get_pkg(xhp, pattern)) &&
			    !(instd = xbps_pkgdb_get_virtualpkg(xhp, pattern)) &&
			    !(instd = xbps_find_pkg_in_array(pkgs, pattern, XBPS_TRANS_INSTALL)))
				continue;

			// don't replace itself
			if (instd == pkgd)
				continue;

			// don't replace packages on hold
			if (xbps_dictionary_get_bool(pkgd, "hold", &hold) && hold)
				continue;

			// check if the package is already in the transaction
			if (!xbps_dictionary_get_cstring_nocopy(instd, "pkgname", &reppkgname))
				xbps_unreachable();
			transd = xbps_find_pkg_in_array(pkgs, reppkgname, 0);

			if (transd) {
				if (transd == pkgd)
					continue;
				r = replace_in_transaction(
				    transd, pattern, pkgver);
				if (r < 0)
					return r;
				xbps_dictionary_get_bool(
				    transd, "automatic-install", &autoinst);
			} else {
				r = replace_in_pkgdb(
				    xhp, instd, pattern, pkgver);
				if (r < 0)
					return r;
				xbps_dictionary_get_bool(
				    instd, "automatic-install", &autoinst);
			}

			// inherit manually installed
			if (!autoinst)
				xbps_dictionary_remove(pkgd, "automatic-install");
		}
	}

	return 0;
}
