/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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
#include <time.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_register_pkg(struct xbps_handle *xhp, xbps_dictionary_t pkgrd)
{
	xbps_array_t replaces;
	xbps_dictionary_t pkgd;
	char outstr[64];
	time_t t;
	struct tm *tmp;
	const char *pkgver;
	char *pkgname = NULL, *buf, *sha256;
	int rv = 0;
	bool autoinst = false;

	assert(xbps_object_type(pkgrd) == XBPS_TYPE_DICTIONARY);

	xbps_dictionary_make_immutable(pkgrd);
	pkgd = xbps_dictionary_copy_mutable(pkgrd);

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);

	if (xhp->flags & XBPS_FLAG_INSTALL_AUTO)
		autoinst = true;
	/*
	 * Set automatic-install to true, iff it was explicitly set; otherwise
	 * preserve its value.
	 */
	if (autoinst && !xbps_dictionary_set_bool(pkgd, "automatic-install", true)) {
		xbps_dbg_printf(xhp, "%s: invalid autoinst for %s\n",  __func__, pkgver);
		rv = EINVAL;
		goto out;
	}
	/*
	 * Set the "install-date" object to know the pkg installation date.
	 */
	t = time(NULL);
	if ((tmp = localtime(&t)) == NULL) {
		xbps_dbg_printf(xhp, "%s: localtime failed: %s\n",
		    pkgver, strerror(errno));
		rv = EINVAL;
		goto out;
	}
	if (strftime(outstr, sizeof(outstr)-1, "%F %R %Z", tmp) == 0) {
		xbps_dbg_printf(xhp, "%s: strftime failed: %s\n",
		    pkgver, strerror(errno));
		rv = EINVAL;
		goto out;
	}
	if (!xbps_dictionary_set_cstring(pkgd, "install-date", outstr)) {
		xbps_dbg_printf(xhp, "%s: install-date set failed!\n", pkgver);
		rv = EINVAL;
		goto out;
	}
	/*
	 * Create a hash for the pkg's metafile.
	 */
	buf = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
	sha256 = xbps_file_hash(buf);
	assert(sha256);
	xbps_dictionary_set_cstring(pkgd, "metafile-sha256", sha256);
	free(sha256);
	free(buf);
	/*
	 * Remove unneeded objs from pkg dictionary.
	 */
	xbps_dictionary_remove(pkgd, "download");
	xbps_dictionary_remove(pkgd, "remove-and-update");
	xbps_dictionary_remove(pkgd, "transaction");
	xbps_dictionary_remove(pkgd, "skip-obsoletes");
	xbps_dictionary_remove(pkgd, "pkgname");
	xbps_dictionary_remove(pkgd, "version");
	/*
	 * Remove self replacement when applicable.
	 */
	if ((replaces = xbps_dictionary_get(pkgd, "replaces"))) {
		buf = xbps_xasprintf("%s>=0", pkgname);
		xbps_remove_string_from_array(replaces, buf);
		free(buf);
	}
	if (!xbps_dictionary_set(xhp->pkgdb, pkgname, pkgd)) {
		xbps_dbg_printf(xhp,
		    "%s: failed to set pkgd for %s\n", __func__, pkgver);
	}
out:
	xbps_object_release(pkgd);
	if (pkgname)
		free(pkgname);

	return rv;
}
