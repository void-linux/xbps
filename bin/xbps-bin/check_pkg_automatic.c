/*-
 * Copyright (c) 2011 Juan Romero Pardines.
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
#include <unistd.h>
#include <sys/param.h>

#include <xbps_api.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check if package was installed manually, but currently
 * 	  other packages are depending on it. This package shall be
 * 	  changed to automatic mode, i.e installed as dependency of
 * 	  those packages.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */
int
check_pkg_autoinstall(prop_dictionary_t pkgd_regpkgdb,
		      prop_dictionary_t pkg_propsd,
		      prop_dictionary_t pkg_filesd)
{
	const struct xbps_handle *xhp = xbps_handle_get();
	prop_dictionary_t dict;
	prop_array_t array, reqby;
	const char *pkgname;
	char *path;
	int rv = 0;
	bool autoinst = false;

	(void)pkg_propsd;
	(void)pkg_filesd;

	prop_dictionary_get_cstring_nocopy(pkgd_regpkgdb, "pkgname", &pkgname);
	/*
	 * Check if package has been installed manually but any other
	 * package is currently depending on it; in that case the package
	 * must be in automatic mode.
	 */
	if (prop_dictionary_get_bool(pkgd_regpkgdb,
	    "automatic-install", &autoinst)) {
		reqby = prop_dictionary_get(pkgd_regpkgdb, "requiredby");
		if (((prop_object_type(reqby) == PROP_TYPE_ARRAY)) &&
		    ((prop_array_count(reqby) > 0) && !autoinst)) {
		        path = xbps_xasprintf("%s/%s/%s",
			    prop_string_cstring_nocopy(xhp->rootdir),
			    XBPS_META_PATH, XBPS_REGPKGDB);
			assert(path != NULL);

			/* pkg has reversedeps and was installed manually */
			prop_dictionary_set_bool(pkgd_regpkgdb,
			    "automatic-install", true);

			dict = prop_dictionary_internalize_from_zfile(path);
			if (dict == NULL) {
				xbps_error_printf("%s: [0] failed to set "
				    "automatic mode (%s)\n", pkgname,
				    strerror(errno));
				return -1;
			}
			array = prop_dictionary_get(dict, "packages");
			rv = xbps_array_replace_dict_by_name(array,
			    pkgd_regpkgdb, pkgname);
			if (rv != 0) {
				xbps_error_printf("%s: [1] failed to set "
				    "automatic mode (%s)\n", pkgname,
				    strerror(rv));
				return -1;
			}
			if (!prop_dictionary_set(dict, "packages", array)) {
				xbps_error_printf("%s: [2] failed to set "
				    "automatic mode (%s)\n", pkgname,
				    strerror(rv));
				return -1;
			}
			if (!prop_dictionary_externalize_to_zfile(dict, path)) {
				xbps_error_printf("%s: [3] failed to set "
				    "automatic mode (%s)\n", pkgname,
				    strerror(errno));
				return -1;
			}
			free(path);
			path = NULL;
			xbps_warn_printf("%s: was installed manually and has "
			    "reverse dependencies (FIXED)\n", pkgname);
		}
	}

	return 0;
}
