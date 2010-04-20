/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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
show_pkg_info_from_metadir(const char *pkgname)
{
	prop_dictionary_t pkgd;
	char *plist;

	plist = xbps_xasprintf("%s/%s/metadata/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, pkgname, XBPS_PKGPROPS);
	if (plist == NULL)
		return EINVAL;

	pkgd = prop_dictionary_internalize_from_zfile(plist);
	if (pkgd == NULL) {
		free(plist);
		return errno;
	}

	show_pkg_info(pkgd);
	prop_object_release(pkgd);
	free(plist);

	return 0;
}

int
show_pkg_files_from_metadir(const char *pkgname)
{
	prop_dictionary_t pkgd;
	char *plist;
	int rv = 0;

	plist = xbps_xasprintf("%s/%s/metadata/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, pkgname, XBPS_PKGFILES);
	if (plist == NULL)
		return EINVAL;

	pkgd = prop_dictionary_internalize_from_zfile(plist);
	if (pkgd == NULL) {
		free(plist);
		return errno;
	}
	free(plist);

	rv = show_pkg_files(pkgd);
	prop_object_release(pkgd);

	return rv;
}
