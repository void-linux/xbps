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

#include <xbps.h>
#include "defs.h"

int
show_pkg_deps(struct xbps_handle *xhp, const char *pkgname, bool repomode, bool full)
{
	xbps_array_t rdeps;
	xbps_dictionary_t pkgd;

	if (repomode) {
		if (((pkgd = xbps_rpool_get_pkg(xhp, pkgname)) == NULL) &&
		    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pkgname)) == NULL))
			return errno;
	} else {
		if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL)
			return errno;
	}
	if (full) {
		if (repomode)
			rdeps = xbps_rpool_get_pkg_fulldeptree(xhp, pkgname);
		else
			rdeps = xbps_pkgdb_get_pkg_fulldeptree(xhp, pkgname);

		if (rdeps == NULL)
			return errno;
	} else {
		rdeps = xbps_dictionary_get(pkgd, "run_depends");
	}
	for (unsigned int i = 0; i < xbps_array_count(rdeps); i++) {
		const char *pkgdep = NULL;
		xbps_array_get_cstring_nocopy(rdeps, i, &pkgdep);
		puts(pkgdep);
	}
	return 0;
}

int
show_pkg_revdeps(struct xbps_handle *xhp, const char *pkg, bool repomode)
{
	xbps_array_t revdeps;
	const char *pkgdep = NULL;

	if (repomode)
		revdeps = xbps_rpool_get_pkg_revdeps(xhp, pkg);
	else
		revdeps = xbps_pkgdb_get_pkg_revdeps(xhp, pkg);

	if (revdeps == NULL)
		return errno;

	for (unsigned int i = 0; i < xbps_array_count(revdeps); i++) {
		xbps_array_get_cstring_nocopy(revdeps, i, &pkgdep);
		puts(pkgdep);
	}
	xbps_object_release(revdeps);
	return 0;
}
