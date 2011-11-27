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
 * 	o Check for target file in symlinks, so that we can check that
 * 	  they have not been modified.
 *
 * returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */
int
check_pkg_symlinks(prop_dictionary_t pkgd_regpkgdb,
		   prop_dictionary_t pkg_propsd,
		   prop_dictionary_t pkg_filesd)
{
	const struct xbps_handle *xhp = xbps_handle_get();
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname, *file, *tgt = NULL;
	char *path, buf[PATH_MAX];
	bool broken = false, test_broken = false;

	(void)pkg_propsd;
	prop_dictionary_get_cstring_nocopy(pkgd_regpkgdb, "pkgname", &pkgname);

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
			path = xbps_xasprintf("%s/%s",
			    prop_string_cstring_nocopy(xhp->rootdir), file);
			if (path == NULL)
				return -1;

			memset(&buf, 0, sizeof(buf));
			if (realpath(path, buf) == NULL) {
				xbps_error_printf("%s: broken symlink `%s': "
				    "%s\n", pkgname, file, strerror(errno));
				test_broken = true;
				continue;
			}

			free(path);
			if (!prop_string_equals_cstring(xhp->rootdir, "/") &&
			    strstr(buf, prop_string_cstring_nocopy(xhp->rootdir)))
				path = buf + prop_string_size(xhp->rootdir);
			else
				path = buf;

			if (strcmp(path, tgt)) {
				xbps_error_printf("%s: modified symlink `%s', "
				    "target: `%s' (shall be: `%s')\n",
				    pkgname, file, tgt, path);
				test_broken = true;
			}
			path = NULL;
		}
		prop_object_iterator_release(iter);
	}
        if (test_broken) {
		xbps_error_printf("%s: symlinks check FAILED.\n", pkgname);
		broken = true;
	}
	return broken;
}
