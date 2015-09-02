/*-
 * Copyright (c) 2014-2015 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_transaction_store(struct xbps_handle *xhp, xbps_array_t pkgs,
		xbps_dictionary_t pkgd, const char *tract, bool autoinst)
{
	xbps_array_t replaces;
	const char *pkgver, *repo;
	char *pkgname, *self_replaced;

	xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repo);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if (xbps_find_pkg_in_array(pkgs, pkgver, NULL))
		return 0;
	/*
	 * Add required objects into package dep's dictionary.
	 */
	if (autoinst && !xbps_dictionary_set_bool(pkgd, "automatic-install", true))
		return EINVAL;

	/*
	 * Set a replaces to itself, so that virtual packages are always replaced.
	*/
	if ((replaces = xbps_dictionary_get(pkgd, "replaces")) == NULL)
		replaces = xbps_array_create();

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);
	self_replaced = xbps_xasprintf("%s>=0", pkgname);
	free(pkgname);
	xbps_array_add_cstring(replaces, self_replaced);
	free(self_replaced);

	if (!xbps_dictionary_set(pkgd, "replaces", replaces))
		return EINVAL;

	/*
	 * Add the dictionary into the unsorted queue.
	 */
	if (!xbps_array_add(pkgs, pkgd))
		return EINVAL;

	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_ADDPKG, 0, pkgver,
	    "Found %s (%s) in repository %s", pkgver, tract, repo);

	xbps_dbg_printf(xhp, "Added `%s' into the dependency list (%s)\n",
	    pkgver, repo);

	return 0;
}
