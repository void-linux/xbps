/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
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
		rv = errno;
		xbps_dbg_printf(xhp, "%s: mkstemp %s\n",
		    __func__, strerror(errno));
		goto out;
	}
	/* write blob to our temp fd */
	ret = write(fd, blob, blobsiz);
	if (ret == -1) {
		rv = errno;
		xbps_dbg_printf(xhp, "%s: write %s\n",
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
	void *buf;
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

	buf = xbps_data_data(data);
	buflen = xbps_data_size(data);
	rv = xbps_pkg_exec_buffer(xhp, buf, buflen, pkgver, action, update);
	free(buf);

	return rv;
}
