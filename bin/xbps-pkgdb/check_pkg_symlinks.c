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
#include <libgen.h>
#include <sys/param.h>

#include <xbps.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for target file in symlinks, so that we can check that
 * 	  they have not been modified.
 *
 * returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */

int
check_pkg_symlinks(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_array_t array;
	xbps_object_t obj;
	xbps_dictionary_t filesd = arg;
	int rv = 0;

	array = xbps_dictionary_get(filesd, "links");
	if (array == NULL)
		return 0;

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		const char *file = NULL, *tgt = NULL;
		char path[PATH_MAX], *lnk = NULL;

		obj = xbps_array_get(array, i);
		if (!xbps_dictionary_get_cstring_nocopy(obj, "file", &file))
			continue;

		/* skip noextract files */
		if (xhp->noextract && xbps_patterns_match(xhp->noextract, file))
			continue;

		if (!xbps_dictionary_get_cstring_nocopy(obj, "target", &tgt)) {
			xbps_warn_printf("%s: `%s' symlink with "
			    "empty target object!\n", pkgname, file);
			continue;
		}
		if (tgt[0] == '\0') {
			xbps_warn_printf("%s: `%s' symlink with "
			    "empty target object!\n", pkgname, file);
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s", xhp->rootdir, file);
		if ((lnk = xbps_symlink_target(xhp, path, tgt)) == NULL) {
			xbps_error_printf("%s: broken symlink %s (target: %s)\n", pkgname, file, tgt);
			rv = -1;
			continue;
		}
		if (strcmp(lnk, tgt)) {
			xbps_warn_printf("%s: modified symlink %s "
			    "points to %s (shall be %s)\n",
			    pkgname, file, lnk, tgt);
			rv = -1;
		}
		free(lnk);
	}
	return rv;
}
