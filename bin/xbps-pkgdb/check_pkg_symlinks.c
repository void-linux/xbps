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
symlink_target(struct xbps_handle *xhp, const char *path)
{
	struct stat sb;
	char *lnk, *res;
	ssize_t r;

	if (lstat(path, &sb) == -1)
		return NULL;

	lnk = malloc(sb.st_size + 1);
	assert(lnk);

	r = readlink(path, lnk, sb.st_size + 1);
	if (r < 0 || r > sb.st_size) {
		free(lnk);
		return NULL;
	}
	lnk[sb.st_size] = '\0';
	if (lnk[0] != '/') {
		char tpath[PATH_MAX], *p, *dname;

		/* relative */
		p = strdup(path);
		assert(p);
		dname = dirname(p);
		assert(dname);
		snprintf(tpath, sizeof(tpath), "%s/%s", dname, lnk);
		free(p);
		if ((res = realpath(tpath, NULL)) == NULL) {
			free(lnk);
			return NULL;
		}
		if (strcmp(xhp->rootdir, "/") == 0)
			p = strdup(res);
		else
			p = strdup(res + strlen(xhp->rootdir));
		free(res);
		res = p;
		free(lnk);
	} else {
		/* absolute */
		res = lnk;
	}
	return res;
}

int
check_pkg_symlinks(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_array_t array;
	xbps_object_t obj;
	xbps_dictionary_t filesd = arg;
	bool broken = false;

	array = xbps_dictionary_get(filesd, "links");
	if (array == NULL)
		return false;

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		const char *file = NULL, *tgt = NULL;
		char path[PATH_MAX], *lnk = NULL, *tlnk = NULL;

		obj = xbps_array_get(array, i);
		if (!xbps_dictionary_get_cstring_nocopy(obj, "file", &file))
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
		if ((lnk = symlink_target(xhp, path)) == NULL) {
			xbps_error_printf("%s: broken symlink %s (target: %s)\n", pkgname, file, tgt);
			broken = true;
			continue;
		}
		if (tgt[0] != '/') {
			char *p, *p1, *dname;

			p = strdup(file);
			assert(p);
			dname = dirname(p);
			assert(dname);
			snprintf(path, sizeof(path), "%s/%s", dname, tgt);
			p1 = realpath(path, NULL);
			free(p);
			if (p1 == NULL) {
				xbps_error_printf("%s: failed to realpath %s: %s\n",
				    pkgname, file, strerror(errno));
				free(lnk);
				continue;
			}
			if (strcmp(xhp->rootdir, "/") == 0)
				tlnk = strdup(p1);
			else
				tlnk = strdup(p1 + strlen(xhp->rootdir));
			free(p1);
		} else {
			char *p;

			snprintf(path, sizeof(path), "%s/%s", xhp->rootdir, tgt);
			if ((p = realpath(path, NULL))) {
				if (strcmp(xhp->rootdir, "/") == 0)
					tlnk = strdup(p);
				else
					tlnk = strdup(p + strlen(xhp->rootdir));
				free(p);
			} else {
				tlnk = strdup(tgt);
			}
		}
		/* absolute */
		if (strcmp(lnk, tlnk)) {
			xbps_warn_printf("%s: modified symlink %s "
			    "points to %s (shall be %s)\n",
			    pkgname, file, lnk, tlnk);
			broken = true;
		}
		free(lnk);
		free(tlnk);
	}
	return broken;
}
