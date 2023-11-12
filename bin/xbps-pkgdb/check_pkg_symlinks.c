/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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
#include <libgen.h>
#include <sys/param.h>

#include <xbps.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for target file in symlinks, so that we can check that
 * 	  they have not been modified or broken.
 *
 * 	o Check for symlink ownership.
 *
 * returns 0 if test ran successfully and -1 on error.
 */

int
check_pkg_symlinks(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_array_t array;
	xbps_object_t obj;
	xbps_dictionary_t filesd = arg;
	bool test_broken = false;
	int rv = 0;
	struct idtree *idt = NULL;

	array = xbps_dictionary_get(filesd, "links");
	if (array == NULL)
		return 0;

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		const char *file = NULL, *tgt = NULL, *user = NULL, *group = NULL;
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
			test_broken = true;
			continue;
		}
		if (strcmp(lnk, tgt)) {
			xbps_warn_printf("%s: modified symlink %s "
			    "points to %s (shall be %s)\n",
			    pkgname, file, lnk, tgt);
			test_broken = true;
		}

		user = NULL;
		xbps_dictionary_get_cstring_nocopy(obj, "user", &user);
		if (user == NULL)
			user = "root";
		rv = file_user_check(idt, path, user);
		switch (rv) {
		case 0:
			break;
		case ERANGE:
			xbps_error_printf("%s: user mismatch for %s.\n", pkgname, file);
			test_broken = true;
			break;
		default:
			xbps_error_printf("%s: can't check `%s' (%s)\n", pkgname, file, strerror(-rv));
			break;
		}

		group = NULL;
		xbps_dictionary_get_cstring_nocopy(obj, "group", &group);
		if (group == NULL)
			group = "root";
		rv = file_group_check(idt, path, group);
		switch (rv) {
		case 0:
			break;
		case ERANGE:
			xbps_error_printf("%s: group mismatch for %s.\n", pkgname, file);
			test_broken = true;
			break;
		default:
			xbps_error_printf("%s: can't check `%s' (%s)\n", pkgname, file, strerror(-rv));
			break;
		}

		free(lnk);
	}

	idtree_free(idt);
	return test_broken ? -1 : 0;
}
