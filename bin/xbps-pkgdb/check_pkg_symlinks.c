/*-
 * Copyright (c) 2011-2012 Juan Romero Pardines.
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

#include <xbps_api.h>
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
check_pkg_symlinks(struct xbps_handle *xhp,
		   const char *pkgname,
		   void *arg,
		   bool *pkgdb_update)
{
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter;
	prop_dictionary_t pkg_filesd = arg;
	const char *file, *tgt = NULL;
	char *path, *buf, *buf2, *buf3, *dname, *path_target;
	bool broken = false, test_broken = false;

	(void)pkgdb_update;

	array = prop_dictionary_get(pkg_filesd, "links");
	if ((prop_object_type(array) == PROP_TYPE_ARRAY) &&
	     prop_array_count(array) > 0) {
		iter = xbps_array_iter_from_dict(pkg_filesd, "links");
		if (iter == NULL)
			return -1;

		while ((obj = prop_object_iterator_next(iter))) {
			if (!prop_dictionary_get_cstring_nocopy(obj, "target", &tgt))
				continue;
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			if (strcmp(tgt, "") == 0) {
				xbps_warn_printf("%s: `%s' symlink with "
				    "empty target object!\n", pkgname, file);
				continue;
			}
			path = xbps_xasprintf("%s/%s", xhp->rootdir, file);
			if ((buf = realpath(path, NULL)) == NULL) {
				xbps_error_printf("%s: broken symlink `%s': "
				    "%s\n", pkgname, file, strerror(errno));
				test_broken = true;
				continue;
			}
			if (strncmp(tgt, "../", 3) == 0) {
				/* relative symlink target */
				dname = dirname(path);
				buf2 = xbps_xasprintf("%s/%s", dname, tgt);
				buf3 = realpath(buf2, NULL);
				assert(buf3);
				free(buf2);
				path_target = buf3;
			} else {
				path_target = buf;
			}
			if (strcmp(buf, path_target)) {
				xbps_error_printf("%s: modified symlink `%s' "
				    "points to: `%s' (shall be: `%s')\n",
				    pkgname, file, buf, path_target);
				test_broken = true;
			}
			free(buf);
			free(path);
			if (buf3)
				free(buf3);

			path = buf = buf2 = buf3 = NULL;

		}
		prop_object_iterator_release(iter);
	}
        if (test_broken) {
		xbps_error_printf("%s: symlinks check FAILED.\n", pkgname);
		broken = true;
	}
	return broken;
}
