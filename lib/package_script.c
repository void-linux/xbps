/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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

#include <sys/stat.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xbps_api_impl.h"


int
xbps_pkg_exec_buffer(struct xbps_handle *xhp,
		     const void *blob,
		     const size_t blobsiz,
		     const char *pkgver,
		     const char *action,
		     bool update)
{
	int i;
	ssize_t ret;
	const char *tmpdir, *version;
	const char *shells[] = {
		"/bin/sh",
		"/bin/dash",
		"/bin/bash",
		NULL
	};
	char pkgname[XBPS_NAME_SIZE], *fpath;
	int fd, rv;

	assert(blob);
	assert(pkgver);
	assert(action);

	if (xhp->target_arch) {
		xbps_dbg_printf("%s: not executing %s "
		    "install/remove action.\n", pkgver, action);
		return 0;
	}

	if (strcmp(xhp->rootdir, "/") == 0) {
		tmpdir = getenv("TMPDIR");
		if (tmpdir == NULL)
			tmpdir = P_tmpdir;

		fpath = xbps_xasprintf("%s/.xbps-script-XXXXXX", tmpdir);
	} else {
		fpath = strdup(".xbps-script-XXXXXX");
	}

	/* change cwd to rootdir to exec the script */
	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		goto out;
	}

	/* Create temp file to run script */
	if ((fd = mkstemp(fpath)) == -1) {
		rv = errno;
		xbps_dbg_printf("%s: mkstemp %s\n",
		    __func__, strerror(errno));
		goto out;
	}
	/* write blob to our temp fd */
	ret = write(fd, blob, blobsiz);
	if (ret == -1) {
		rv = errno;
		xbps_dbg_printf("%s: write %s\n",
		    __func__, strerror(errno));
		close(fd);
		goto out;
	}
	fchmod(fd, 0750);
#ifdef HAVE_FDATASYNC
	fdatasync(fd);
#else
	fsync(fd);
#endif
	close(fd);

	/* exec script */
	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		abort();
	}
	version = xbps_pkg_version(pkgver);
	assert(version);

	// find a shell that can be used to execute the script.
	for (i = 0; shells[i] != NULL; i++) {
		if (access(shells[i], X_OK) == 0) {
			break;
		}
	}
	if (shells[i] != NULL) {
		rv = xbps_file_exec(xhp, shells[i], fpath, action, pkgname, version,
				update ? "yes" : "no",
				"no", xhp->native_arch, NULL);
	} else if (access("/bin/busybox", X_OK) == 0) {
		rv = xbps_file_exec(xhp, "/bin/busybox", "sh", fpath, action, pkgname, version,
				update ? "yes" : "no",
				"no", xhp->native_arch, NULL);
	} else if (access("/bin/busybox.static", X_OK) == 0) {
		rv = xbps_file_exec(xhp, "/bin/busybox.static", "sh", fpath, action, pkgname, version,
				update ? "yes" : "no",
				"no", xhp->native_arch, NULL);
	} else {
		rv = -1;
	}

out:
	remove(fpath);
	free(fpath);
	return rv;
}

int
xbps_pkg_exec_script(struct xbps_handle *xhp,
		     xbps_dictionary_t d,
		     const char *script,
		     const char *action,
		     bool update)
{
	xbps_data_t data;
	const void *buf;
	size_t buflen;
	const char *pkgver = NULL;
	int rv;

	assert(xhp);
	assert(d);
	assert(script);
	assert(action);

	data = xbps_dictionary_get(d, script);
	if (data == NULL)
		return 0;

	xbps_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver);

	buf = xbps_data_data_nocopy(data);
	buflen = xbps_data_size(data);
	rv = xbps_pkg_exec_buffer(xhp, buf, buflen, pkgver, action, update);

	return rv;
}
