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
#include "uthash.h"


struct pkgmeta {
	char name[64];
	prop_dictionary_t d;
	UT_hash_handle hh;
};

struct pkgmeta *pkgmetas = NULL;

void HIDDEN
xbps_metadir_release(void)
{
	struct pkgmeta *pm = NULL, *pmp = NULL;

	HASH_ITER(hh, pm, pkgmetas, pmp) {
		HASH_DEL(pkgmetas, pm);
		prop_object_release(pm->d);
		free(pm);
	}
}

static prop_dictionary_t
metadir_get(const char *name)
{
	struct pkgmeta *pm;

	HASH_FIND_STR(pkgmetas, __UNCONST(name), pm);
	if (pm && pm->d)
		return pm->d;

	return NULL;
}

prop_dictionary_t
xbps_metadir_get_pkgd(struct xbps_handle *xhp, const char *name)
{
	struct pkgmeta *pm;
	prop_dictionary_t pkgd, d;
	const char *savedpkgname;
	char *plistf;

	assert(xhp);
	assert(name);

	if ((pkgd = metadir_get(name)) != NULL)
		return pkgd;

	savedpkgname = name;
	plistf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, name);

	if (access(plistf, R_OK) == -1) {
		pkgd = xbps_find_virtualpkg_dict_installed(xhp, name, false);
		if (pkgd == NULL)
			pkgd = xbps_find_pkg_dict_installed(xhp, name, false);

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

	/* Add pkg plist to hash map */
	pm = calloc(1, sizeof(*pm));
	assert(pm);
	strlcpy(pm->name, name, sizeof(pm->name));
	pm->d = d;

	HASH_ADD_KEYPTR(hh,
			pkgmetas,
			__UNCONST(name),
			strlen(__UNCONST(name)),
			pm);

	return d;
}
