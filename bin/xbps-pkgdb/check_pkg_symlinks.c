/*-
 * Copyright (c) 2011-2014 Juan Romero Pardines.
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
 * 	  they have not been modified.
 *
 * returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */
static char *
symlink_target(const char *pkgname, const char *path)
{
	struct stat sb;
	char *lnk;
	ssize_t r;

	if (lstat(path, &sb) == -1) {
		xbps_error_printf("%s: lstat failed for %s\n", pkgname, path);
		return NULL;
	}

	lnk = malloc(sb.st_size + 1);
	assert(lnk);

	r = readlink(path, lnk, sb.st_size + 1);
	if (r < 0 || r > sb.st_size) {
		xbps_error_printf("%s: readlink failed for %s\n", pkgname, path);
		free(lnk);
		return NULL;
	}
	lnk[sb.st_size] = '\0';

	return lnk;
}

int
check_pkg_symlinks(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_array_t array;
	xbps_object_t obj;
	xbps_dictionary_t filesd = arg;
	const char *file, *tgt = NULL;
	char path[PATH_MAX], tgt_path[PATH_MAX], *p, *buf, *buf2, *lnk, *dname;
	int rv;
	bool broken = false;

	array = xbps_dictionary_get(filesd, "links");
	if (array == NULL)
		return false;

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		if (!xbps_dictionary_get_cstring_nocopy(obj, "file", &file))
			continue;

		if (!xbps_dictionary_get_cstring_nocopy(obj, "target", &tgt)) {
			xbps_warn_printf("%s: `%s' symlink with "
			    "empty target object!\n", pkgname, file);
			continue;
		}
		if (strcmp(tgt, "") == 0) {
			xbps_warn_printf("%s: `%s' symlink with "
			    "empty target object!\n", pkgname, file);
			continue;
		}

		if (strcmp(xhp->rootdir, "/")) {
			snprintf(path, sizeof(path), "%s%s", xhp->rootdir, file);
			snprintf(tgt_path, sizeof(tgt_path), "%s%s", xhp->rootdir, tgt);

			strncpy(tgt_path, tgt, sizeof(tgt_path)-1);
		} else  {
			strncpy(path, file, sizeof(path)-1);
			strncpy(tgt_path, tgt, sizeof(tgt_path)-1);
		}

		if ((lnk = symlink_target(pkgname, path)) == NULL)
			continue;

		p = strdup(path);
		assert(p);
		dname = dirname(p);
		buf = xbps_xasprintf("%s/%s", dname, lnk);
		free(p);

		buf2 = realpath(path, NULL);
		if (buf2 == NULL) {
			xbps_warn_printf("%s: broken symlink %s (target: %s)\n",
			     pkgname, file, tgt);
			free(buf);
			free(lnk);
			continue;
		}

		rv = 1;
		if (lnk[0] != '/') {
			/* relative symlink */
			if ((strcmp(lnk, tgt) == 0) ||
			    (strcmp(buf, tgt_path) == 0) ||
			    (strcmp(buf2, tgt_path) == 0))
				rv = 0;
		} else {
			/* absolute symlink */
			if (strcmp(lnk, tgt) == 0)
				rv = 0;
		}

		if (rv) {
			xbps_warn_printf("%s: modified symlink %s "
			    "points to %s (shall be %s)\n",
			    pkgname, file, lnk, tgt);
			broken = true;
		}
		free(buf);
		free(buf2);
		free(lnk);
	}
	return broken;
}
