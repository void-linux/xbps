/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>
#include <limits.h>

#include <xbps.h>
#include "defs.h"

static int
binpkg_parse(char *buf, size_t bufsz, const char *path, const char **pkgver, const char **arch)
{
	char *p;
	size_t n = xbps_strlcpy(buf, path, bufsz);
	if (n >= bufsz)
		return -ENOBUFS;

	/* remove .xbps file extension */
	p = buf+n-sizeof(".xbps")+1;
	*p = '\0';

	/* find the previous dot that separates the architecture */
	for (p = p-1; p > buf && *p != '.'; p--);
	if (*p != '.')
		return -EINVAL;
	*p = '\0';

	/* make sure the pkgver part is valid */
	if (!xbps_pkg_version(buf))
		return -EINVAL;

	*arch = p+1;
	*pkgver = buf;
	return 0;
}

struct cleaner_data {
	bool dry;
	bool uninstalled;
};

static int
cleaner_cb(struct xbps_handle *xhp, xbps_object_t obj,
		const char *key UNUSED, void *arg,
		bool *done UNUSED)
{
	char buf[PATH_MAX];
	char buf2[PATH_MAX];
	xbps_dictionary_t pkgd;
	const char *binpkg, *rsha256;
	const char *binpkgver, *binpkgarch;
	struct cleaner_data *data = arg;
	int r;

	binpkg = xbps_string_cstring_nocopy(obj);
	r = binpkg_parse(buf, sizeof(buf), binpkg, &binpkgver, &binpkgarch);
	if (r < 0) {
		xbps_error_printf("Binary package filename: %s: %s\n", binpkg, strerror(-r));
		return 0;
	}
	if (strcmp(binpkgarch, xhp->target_arch ? xhp->target_arch : xhp->native_arch) != 0 &&
	    strcmp(binpkgarch, "noarch") != 0) {
		xbps_dbg_printf("%s: ignoring binpkg with unmatched arch\n", binpkg);
		return 0;
	}

	/*
	 * Remove binary pkg if it's not registered in any repository
	 * or if hash doesn't match.
	 */
	if (data->uninstalled) {
		pkgd = xbps_pkgdb_get_pkg(xhp, binpkgver);
	} else {
		pkgd = xbps_rpool_get_pkg(xhp, binpkgver);
	}
	if (pkgd) {
		xbps_dictionary_get_cstring_nocopy(pkgd,
		    "filename-sha256", &rsha256);
		r = xbps_file_sha256_check(binpkg, rsha256);
		if (r == 0) {
			/* hash matched */
			return 0;
		}
		if (r != ERANGE) {
			xbps_error_printf("Failed to checksum `%s': %s\n", binpkg, strerror(r));
			return 0;
		}
	}
	snprintf(buf, sizeof(buf), "%s.sig", binpkg);
	snprintf(buf2, sizeof(buf2), "%s.sig2", binpkg);
	if (!data->dry && unlink(binpkg) == -1) {
		xbps_error_printf("Failed to remove `%s': %s\n",
		    binpkg, strerror(errno));
	} else {
		printf("Removed %s from cachedir (obsolete)\n", binpkg);
	}
	if (!data->dry && unlink(buf) == -1 && errno != ENOENT) {
		xbps_error_printf("Failed to remove `%s': %s\n",
		    buf, strerror(errno));
	}
	if (!data->dry && unlink(buf2) == -1 && errno != ENOENT) {
		xbps_error_printf("Failed to remove `%s': %s\n",
		    buf2, strerror(errno));
	}

	return 0;
}

int
clean_cachedir(struct xbps_handle *xhp, bool uninstalled, bool drun)
{
	xbps_array_t array = NULL;
	DIR *dirp;
	struct dirent *dp;
	char *ext;
	int r;

	// XXX: there is no public api to load the pkgdb so force it before
	// its done potentially concurrently by threads through the
	// xbps_array_foreach_cb_multi call later.
	// XXX: same for the repository pool...
	if (uninstalled) {
		(void)xbps_pkgdb_get_pkg(xhp, "foo");
	} else {
		(void)xbps_rpool_get_pkg(xhp, "package-that-wont-exist-so-it-loads-all-repos");
	}

	if (chdir(xhp->cachedir) == -1) {
		if (errno == ENOENT)
			return 0;
		r = -errno;
		xbps_error_printf("failed to change to cache directory: %s: %s\n",
		    xhp->cachedir, strerror(-r));
		return r;
	}

	dirp = opendir(".");
	if (!dirp) {
		r = -errno;
		xbps_error_printf("failed to open cache directory: %s: %s\n",
		    xhp->cachedir, strerror(-r));
		return r;
	}

	array = xbps_array_create();
	if (!array)
		return xbps_error_oom();

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		/* only process xbps binary packages, ignore something else */
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".xbps")) {
			xbps_dbg_printf("ignoring unknown file: %s\n", dp->d_name);
			continue;
		}
		if (!xbps_array_add_cstring(array, dp->d_name)) {
			xbps_object_release(array);
			return xbps_error_oom();
		}
	}
	(void)closedir(dirp);

	if (xbps_array_count(array)) {
		struct cleaner_data data = {
			.dry = drun,
			.uninstalled = uninstalled,
		};
		r = xbps_array_foreach_cb_multi(xhp, array, NULL, cleaner_cb, (void*)&data);
	} else {
		r = 0;
	}

	xbps_object_release(array);
	return r;
}
