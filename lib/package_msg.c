/*-
 * Copyright (c) 2014 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_cb_message(struct xbps_handle *xhp, xbps_dictionary_t pkgd, const char *key)
{
	xbps_data_t msg;
	const void *data;
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

	/* turn data from msg into a string */
	data = xbps_data_data_nocopy(msg);
	len = xbps_data_size(msg);
	buf = malloc(len+1);
	assert(buf);
	memcpy(buf, data, len);
	/* terminate string */
	buf[len] = '\0';

	/* notify client to show the post-install message */
	if (strcmp(key, "install-msg") == 0)
		xbps_set_cb_state(xhp, XBPS_STATE_SHOW_INSTALL_MSG, 0, pkgver, "%s", buf);
	else
		xbps_set_cb_state(xhp, XBPS_STATE_SHOW_REMOVE_MSG, 0, pkgver, "%s", buf);

out:
	free(buf);
	return rv;
}
