/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

int
xbps_pkg_exec_buffer(struct xbps_handle *xhp,
		     const void *blob,
		     const size_t blobsiz,
		     const char *pkgver,
		     const char *action,
		     bool update)
{
	ssize_t ret;
	const char *tmpdir, *version;
	char *pkgname, *fpath;
	int fd, rv;

	assert(blob);
	assert(pkgver);
	assert(action);

	if (xhp->target_arch) {
		xbps_dbg_printf(xhp, "%s: not executing %s "
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
		xbps_dbg_printf(xhp, "%s: mkstemp %s\n",
		    __func__, strerror(errno));
		free(fpath);
		return errno;
	}
	/* write blob to our temp fd */
	ret = write(fd, blob, blobsiz);
	if (ret == -1) {
		xbps_dbg_printf(xhp, "%s: write %s\n",
		    __func__, strerror(errno));
		close(fd);
		rv = errno;
		goto out;
	}
	fchmod(fd, 0750);
	fdatasync(fd);
	close(fd);

	/* exec script */
	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);
	version = xbps_pkg_version(pkgver);
	assert(version);

	rv = xbps_file_exec(xhp, fpath, action, pkgname, version,
			    update ? "yes" : "no",
			    xhp->conffile, NULL);
	free(pkgname);

out:
	remove(fpath);
	free(fpath);
	return rv;
}

int
xbps_pkg_exec_script(struct xbps_handle *xhp,
		     prop_dictionary_t d,
		     const char *script,
		     const char *action,
		     bool update)
{
	prop_data_t data;
	void *buf;
	size_t buflen;
	const char *pkgver;
	int rv;

	assert(xhp);
	assert(d);
	assert(script);
	assert(action);

	data = prop_dictionary_get(d, script);
	if (data == NULL)
		return 0;

	prop_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver);

	buf = prop_data_data(data);
	buflen = prop_data_size(data);
	rv = xbps_pkg_exec_buffer(xhp, buf, buflen, pkgver, action, update);
	free(buf);

	return rv;
}
