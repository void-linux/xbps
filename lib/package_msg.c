/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_cb_message(struct xbps_handle *xhp, xbps_dictionary_t pkgd, const char *key)
{
	xbps_data_t msg;
	FILE *f = NULL;
	const void *data = NULL;
	const char *pkgver = NULL;
	size_t len;
	char *buf = NULL;
	int rv = 0;

	assert(xhp);
	assert(pkgd);
	assert(key);

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	/* show install-msg if exists */
	msg = xbps_dictionary_get(pkgd, key);
	if (xbps_object_type(msg) != XBPS_TYPE_DATA)
		goto out;

	data = xbps_data_data_nocopy(msg);
	len = xbps_data_size(msg);
	if ((f = fmemopen(__UNCONST(data), len, "r")) == NULL) {
		rv = errno;
		xbps_dbg_printf(xhp, "[%s] %s: fmemopen %s\n", __func__, pkgver, strerror(rv));
		goto out;
	};
	buf = malloc(len+1);
	assert(buf);
	if (fread(buf, len, 1, f) != len) {
		if (ferror(f)) {
			rv = errno;
			xbps_dbg_printf(xhp, "[%s] %s: fread %s\n", __func__, pkgver, strerror(rv));
			goto out;
		}
	}
	/* terminate buffer and notify client to show the post-install message */
	buf[len] = '\0';

	if (strcmp(key, "install-msg") == 0)
		xbps_set_cb_state(xhp, XBPS_STATE_SHOW_INSTALL_MSG, 0, pkgver, "%s", buf);
	else
		xbps_set_cb_state(xhp, XBPS_STATE_SHOW_REMOVE_MSG, 0, pkgver, "%s", buf);

out:
	if (f != NULL)
		fclose(f);
	if (buf != NULL)
		free(buf);

	return rv;
}
