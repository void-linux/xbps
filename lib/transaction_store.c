/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

bool HIDDEN
xbps_transaction_store(struct xbps_handle *xhp, xbps_array_t pkgs,
		xbps_dictionary_t pkgrd, bool autoinst)
{
	xbps_dictionary_t d, pkgd;
	xbps_array_t replaces;
	const char *pkgver, *pkgname, *curpkgver, *repo;
	char *self_replaced;
	int rv;

	assert(xhp);
	assert(pkgs);
	assert(pkgrd);

	if (!xbps_dictionary_get_cstring_nocopy(pkgrd, "pkgver", &pkgver)) {
		return false;
	}
	if (!xbps_dictionary_get_cstring_nocopy(pkgrd, "pkgname", &pkgname)) {
		return false;
	}
	d = xbps_find_pkg_in_array(pkgs, pkgname, 0);
	if (xbps_object_type(d) == XBPS_TYPE_DICTIONARY) {
		/* compare version stored in transaction vs current */
		if (!xbps_dictionary_get_cstring_nocopy(d, "pkgver", &curpkgver)) {
			return false;
		}
		rv = xbps_cmpver(pkgver, curpkgver);
		if (rv == 0 || rv == -1) {
			/* same version or stored version greater than current */
			return true;
		} else {
			/*
			 * Current version is greater than stored,
			 * replace stored with current.
			 */
			if (!xbps_remove_pkg_from_array_by_pkgver(pkgs, curpkgver)) {
				return false;
			}
			xbps_dbg_printf(xhp, "[trans] replaced %s with %s\n", curpkgver, pkgver);
		}
	}

	if ((pkgd = xbps_dictionary_copy_mutable(pkgrd)) == NULL)
		return false;

	/*
	 * Add required objects into package dep's dictionary.
	 */
	if (autoinst && !xbps_dictionary_set_bool(pkgd, "automatic-install", true))
		goto err;

	/*
	 * Set a replaces to itself, so that virtual packages are always replaced.
	*/
	if ((replaces = xbps_dictionary_get(pkgd, "replaces")) == NULL)
		replaces = xbps_array_create();

	self_replaced = xbps_xasprintf("%s>=0", pkgname);
	xbps_array_add_cstring(replaces, self_replaced);
	free(self_replaced);

	if (!xbps_dictionary_set(pkgd, "replaces", replaces))
		goto err;

	/*
	 * Add the dictionary into the unsorted queue.
	 */
	if (!xbps_array_add(pkgs, pkgd))
		goto err;

	xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repo);

	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_ADDPKG, 0, pkgver,
	    "Found %s in repository %s", pkgver, repo);

	xbps_dbg_printf(xhp, "[trans] `%s' stored (%s)\n", pkgver, repo);
	xbps_object_release(pkgd);

	return true;
err:
	xbps_object_release(pkgd);
	return false;
}
