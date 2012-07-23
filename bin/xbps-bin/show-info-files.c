/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "../xbps-repo/defs.h"
#include "defs.h"

int
show_pkg_info_from_metadir(struct xbps_handle *xhp,
			   const char *pkgname,
			   const char *option)
{
	prop_dictionary_t d, pkgdb_d;
	const char *instdate, *pname;
	bool autoinst;

	d = xbps_dictionary_from_metadata_plist(xhp, pkgname, XBPS_PKGPROPS);
	if (d == NULL)
		return EINVAL;

	prop_dictionary_get_cstring_nocopy(d, "pkgname", &pname);
	pkgdb_d = xbps_pkgdb_get_pkgd(xhp, pname, false);
	if (pkgdb_d == NULL) {
		prop_object_release(d);
		return EINVAL;
	}
	if (prop_dictionary_get_cstring_nocopy(pkgdb_d,
	    "install-date", &instdate))
		prop_dictionary_set_cstring_nocopy(d, "install-date",
		    instdate);

	if (prop_dictionary_get_bool(pkgdb_d, "automatic-install", &autoinst))
		prop_dictionary_set_bool(d, "automatic-install", autoinst);

	if (option == NULL)
		show_pkg_info(d);
	else
		show_pkg_info_one(d, option);

	prop_object_release(d);
	return 0;
}

int
show_pkg_files_from_metadir(struct xbps_handle *xhp, const char *pkgname)
{
	prop_dictionary_t d;
	int rv = 0;

	d = xbps_dictionary_from_metadata_plist(xhp, pkgname, XBPS_PKGFILES);
	if (d == NULL)
		return EINVAL;

	rv = show_pkg_files(d);
	prop_object_release(d);

	return rv;
}
