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
 * 	o Check if pkg dictionary from pkgdb contains "unneeded" objects,
 * 	  and remove them if that was true.
 */
int
check_pkg_unneeded(struct xbps_handle *xhp UNUSED, const char *pkgname, void *arg)
{
	xbps_array_t replaces;
	xbps_dictionary_t pkgd = arg;
	const char *repo = NULL;
	char *buf;

	xbps_dictionary_remove(pkgd, "download");
	xbps_dictionary_remove(pkgd, "remove-and-update");
	xbps_dictionary_remove(pkgd, "transaction");
	xbps_dictionary_remove(pkgd, "skip-obsoletes");
	xbps_dictionary_remove(pkgd, "packaged-with");
	if (xbps_dictionary_get_cstring_nocopy(pkgd, "repository-origin", &repo)) {
		xbps_dictionary_set_cstring(pkgd, "repository", repo);
		xbps_dictionary_remove(pkgd, "repository-origin");
	}
	/*
	 * Remove self replacement when applicable.
	 */
	if ((replaces = xbps_dictionary_get(pkgd, "replaces"))) {
		buf = xbps_xasprintf("%s>=0", pkgname);
		xbps_remove_string_from_array(replaces, buf);
		free(buf);
		if (!xbps_array_count(replaces))
			xbps_dictionary_remove(pkgd, "replaces");
	}

	return 0;
}
