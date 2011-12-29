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
check_pkg_autoinstall(const char *pkgname, void *arg)
{
	struct xbps_handle *xhp = xbps_handle_get();
	prop_dictionary_t pkgd = arg;
	prop_array_t array, reqby;
	int rv = 0;
	bool autoinst = false;

	/*
	 * Check if package has been installed manually but any other
	 * package is currently depending on it; in that case the package
	 * must be in automatic mode.
	 */
	if (prop_dictionary_get_bool(pkgd, "automatic-install", &autoinst)) {
		reqby = prop_dictionary_get(pkgd, "requiredby");
		if (((prop_object_type(reqby) == PROP_TYPE_ARRAY)) &&
		    ((prop_array_count(reqby) > 0) && !autoinst)) {

			/* pkg has reversedeps and was installed manually */
			prop_dictionary_set_bool(pkgd,
			    "automatic-install", true);

			array = prop_dictionary_get(xhp->regpkgdb, "packages");
			rv = xbps_array_replace_dict_by_name(array,
			    pkgd, pkgname);
			if (rv != 0) {
				xbps_error_printf("%s: [1] failed to set "
				    "automatic mode (%s)\n", pkgname,
				    strerror(rv));
				return -1;
			}
			if (!prop_dictionary_set(xhp->regpkgdb,
			    "packages", array)) {
				xbps_error_printf("%s: [2] failed to set "
				    "automatic mode (%s)\n", pkgname,
				    strerror(rv));
				return -1;
			}
			if ((rv = xbps_regpkgdb_update(xhp, true)) != 0) {
				xbps_error_printf("%s: failed to write "
				    "regpkgdb plist: %s\n", pkgname,
				    strerror(rv));
				return -1;
			}
			xbps_warn_printf("%s: was installed manually and has "
			    "reverse dependencies (FIXED)\n", pkgname);
		}
	}

	return 0;
}
