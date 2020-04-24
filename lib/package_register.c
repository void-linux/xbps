/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_register_pkg(struct xbps_handle *xhp, xbps_dictionary_t pkgrd)
{
	xbps_array_t replaces;
	xbps_dictionary_t pkgd;
	time_t t;
	struct tm tm, *tmp;
	const char *pkgver, *pkgname;
	char sha256[XBPS_SHA256_SIZE], outstr[64], *buf;
	int rv = 0;
	bool autoinst = false;

	assert(xbps_object_type(pkgrd) == XBPS_TYPE_DICTIONARY);

	xbps_dictionary_make_immutable(pkgrd);
	if ((pkgd = xbps_dictionary_copy_mutable(pkgrd)) == NULL) {
		goto out;
	}

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);

	if (xhp->flags & XBPS_FLAG_INSTALL_AUTO)
		autoinst = true;
	/*
	 * Set automatic-install to true, iff it was explicitly set; otherwise
	 * preserve its value.
	 */
	if (autoinst && !xbps_dictionary_set_bool(pkgd, "automatic-install", true)) {
		xbps_dbg_printf(xhp, "%s: invalid autoinst for %s\n",  __func__, pkgver);
		rv = EINVAL;
		goto out;
	}
	if (xhp->flags & XBPS_FLAG_INSTALL_REPRO) {
		/*
		 * Reproducible mode. Some objects must not be recorded:
		 * 	- install-date
		 * 	- repository
		 */
		xbps_dictionary_remove(pkgd, "repository");
	} else {
		/*
		 * Set the "install-date" object to know the pkg installation date.
		 */
		t = time(NULL);
		if ((tmp = localtime_r(&t, &tm)) == NULL) {
			xbps_dbg_printf(xhp, "%s: localtime_r failed: %s\n",
					pkgver, strerror(errno));
			rv = EINVAL;
			goto out;
		}
		if (strftime(outstr, sizeof(outstr)-1, "%F %R %Z", tmp) == 0) {
			xbps_dbg_printf(xhp, "%s: strftime failed: %s\n",
					pkgver, strerror(errno));
			rv = EINVAL;
			goto out;
		}
		if (!xbps_dictionary_set_cstring(pkgd, "install-date", outstr)) {
			xbps_dbg_printf(xhp, "%s: install-date set failed!\n", pkgver);
			rv = EINVAL;
			goto out;
		}
	}
	/*
	 * Create a hash for the pkg's metafile if it exists.
	 */
	buf = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
	if (xbps_file_sha256(sha256, sizeof sha256, buf)) {
		xbps_dictionary_set_cstring(pkgd, "metafile-sha256", sha256);
	}
	free(buf);
	/*
	 * Remove self replacement when applicable.
	 */
	if ((replaces = xbps_dictionary_get(pkgd, "replaces"))) {
		buf = xbps_xasprintf("%s>=0", pkgname);
		xbps_remove_string_from_array(replaces, buf);
		free(buf);
		if (!xbps_array_count(replaces))
			xbps_dictionary_remove(pkgd, "replaces");
	}
	/*
	 * Remove unneeded objs from pkg dictionary.
	 */
	xbps_dictionary_remove(pkgd, "download");
	xbps_dictionary_remove(pkgd, "remove-and-update");
	xbps_dictionary_remove(pkgd, "transaction");
	xbps_dictionary_remove(pkgd, "skip-obsoletes");
	xbps_dictionary_remove(pkgd, "pkgname");
	xbps_dictionary_remove(pkgd, "version");

	if (!xbps_dictionary_set(xhp->pkgdb, pkgname, pkgd)) {
		xbps_dbg_printf(xhp,
				"%s: failed to set pkgd for %s\n", __func__, pkgver);
	}
out:
	xbps_object_release(pkgd);

	return rv;
}
