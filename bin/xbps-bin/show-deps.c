/*-
 * Copyright (c) 2009 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-repo/defs.h"

int
xbps_show_pkg_deps(const char *pkgname)
{
	prop_dictionary_t pkgd, propsd;
	char *path;
	int rv = 0;

	assert(pkgname != NULL);

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL) {
		printf("Package %s is not installed.\n", pkgname);
		return 0;
	}

	/*
	 * Check for props.plist metadata file.
	 */
	path = xbps_xasprintf("%s/%s/metadata/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, pkgname, XBPS_PKGPROPS);
	if (path == NULL)
		return errno;

	propsd = prop_dictionary_internalize_from_file(path);
	free(path);
	if (propsd == NULL) {
		fprintf(stderr,
		    "%s: unexistent %s metadata file.\n", pkgname,
		    XBPS_PKGPROPS);
		return errno;
	}

	rv = xbps_callback_array_iter_in_dict(propsd, "run_depends",
	     list_strings_sep_in_array, NULL);
	prop_object_release(propsd);
	prop_object_release(pkgd);

	return rv;
}

int
xbps_show_pkg_reverse_deps(const char *pkgname)
{
	prop_dictionary_t pkgd;
	int rv = 0;

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL) {
		printf("Package %s is not installed.\n", pkgname);
		return 0;
	}

	rv = xbps_callback_array_iter_in_dict(pkgd, "requiredby",
	    list_strings_sep_in_array, NULL);
	prop_object_release(pkgd);

	return rv;
}
