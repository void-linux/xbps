/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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

void HIDDEN
xbps_metadir_release(struct xbps_handle *xhp)
{
	if (prop_object_type(xhp->pkg_metad) == PROP_TYPE_DICTIONARY)
	       prop_object_release(xhp->pkg_metad);
}

prop_dictionary_t
xbps_metadir_get_pkgd(struct xbps_handle *xhp, const char *name)
{
	prop_dictionary_t pkgd, d;
	const char *savedpkgname;
	char *plistf;

	assert(xhp);
	assert(name);

	if ((pkgd = prop_dictionary_get(xhp->pkg_metad, name)) != NULL)
		return pkgd;

	savedpkgname = name;
	plistf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, name);

	if (access(plistf, R_OK) == -1) {
		pkgd = xbps_pkgdb_get_virtualpkgd(xhp, name, false);
		if (pkgd == NULL)
			pkgd = xbps_pkgdb_get_pkgd(xhp, name, false);

		if (pkgd != NULL) {
			free(plistf);
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "pkgname", &savedpkgname);
			plistf = xbps_xasprintf("%s/.%s.plist",
			    xhp->metadir, savedpkgname);
		}
	}

	d = prop_dictionary_internalize_from_zfile(plistf);
	free(plistf);
	if (d == NULL) {
		xbps_dbg_printf(xhp, "cannot read %s metadata: %s\n",
		    savedpkgname, strerror(errno));
		return NULL;
	}

	if (xhp->pkg_metad == NULL)
		xhp->pkg_metad = prop_dictionary_create();

	prop_dictionary_set(xhp->pkg_metad, name, d);
	prop_object_release(d);

	return d;
}
