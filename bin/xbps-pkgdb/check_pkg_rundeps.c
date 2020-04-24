/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for missing run time dependencies.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */

int
check_pkg_rundeps(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_dictionary_t pkg_propsd = arg;
	xbps_array_t array;
	const char *reqpkg = NULL;
	int rv = 0;

	if (!xbps_pkg_has_rundeps(pkg_propsd))
		return 0;

	array = xbps_dictionary_get(pkg_propsd, "run_depends");
	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		xbps_array_get_cstring_nocopy(array, i, &reqpkg);
		if (xbps_pkg_is_ignored(xhp, reqpkg))
			continue;
		if (xbps_pkg_is_installed(xhp, reqpkg) <= 0) {
			xbps_error_printf("%s: dependency not satisfied: %s\n",
			    pkgname, reqpkg);
			rv = -1;
		}
	}
	return rv;
}
