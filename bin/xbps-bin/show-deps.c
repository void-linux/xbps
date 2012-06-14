/*-
 * Copyright (c) 2009-2010 Juan Romero Pardines.
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
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-repo/defs.h"

int
show_pkg_deps(struct xbps_handle *xhp, const char *pkgname)
{
	prop_dictionary_t propsd;
	int rv = 0;

	assert(pkgname != NULL);

	/*
	 * Check for props.plist metadata file.
	 */
	propsd = xbps_dictionary_from_metadata_plist(xhp,
	    pkgname, XBPS_PKGPROPS);
	if (propsd == NULL) {
		fprintf(stderr,
		    "%s: unexistent %s metadata file.\n", pkgname,
		    XBPS_PKGPROPS);
		return errno;
	}
	rv = xbps_callback_array_iter_in_dict(xhp, propsd, "run_depends",
	     list_strings_sep_in_array, NULL);
	prop_object_release(propsd);

	return rv;
}

int
show_pkg_reverse_deps(struct xbps_handle *xhp, const char *pkgname)
{
	prop_dictionary_t pkgd;
	int rv = 0;

	pkgd = xbps_find_virtualpkg_dict_installed(xhp, pkgname, false);
	if (pkgd == NULL) {
		pkgd = xbps_find_pkg_dict_installed(xhp, pkgname, false);
		if (pkgd == NULL) {
			printf("Package %s is not installed.\n", pkgname);
			return 0;
		}
	}
	rv = xbps_callback_array_iter_in_dict(xhp, pkgd, "requiredby",
	    list_strings_sep_in_array, NULL);
	prop_object_release(pkgd);

	return rv;
}
