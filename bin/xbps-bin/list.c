/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "defs.h"

int
list_pkgs_in_dict(struct xbps_handle *xhp,
		  prop_object_t obj,
		  void *arg,
		  bool *loop_done)
{
	struct list_pkgver_cb *lpc = arg;
	const char *pkgver, *short_desc, *arch;
	char *tmp = NULL, *out = NULL;
	pkg_state_t curstate;
	size_t i, len = 0;
	bool chkarch;

	(void)xhp;
	(void)loop_done;

	chkarch = prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	if (chkarch && !xbps_pkg_arch_match(xhp, arch, NULL))
		return 0;

	if (lpc->check_state) {
		if (xbps_pkg_state_dictionary(obj, &curstate))
			return EINVAL;
		if (lpc->state == 0) {
			/* Only list packages that are fully installed */
			if (curstate != XBPS_PKG_STATE_INSTALLED)
				return 0;
		} else {
			/* Only list packages with specified state */
			if (curstate != lpc->state)
				return 0;
		}
	}

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "short_desc", &short_desc);
	if (!pkgver && !short_desc)
		return EINVAL;

	tmp = calloc(1, lpc->pkgver_len + 1);
	assert(tmp);
	memcpy(tmp, pkgver, lpc->pkgver_len);
	for (i = strlen(tmp); i < lpc->pkgver_len; i++)
		tmp[i] = ' ';

	tmp[i] = '\0';
	len = strlen(tmp) + strlen(short_desc) + 1;
	if (len > lpc->maxcols) {
		out = malloc(lpc->maxcols);
		assert(out);
		snprintf(out, lpc->maxcols-2, "%s %s", tmp, short_desc);
		strncat(out, "...", lpc->maxcols);
		printf("%s\n", out);
		free(out);
	} else {
		printf("%s %s\n", tmp, short_desc);
	}
	free(tmp);

	return 0;
}

int
list_manual_pkgs(struct xbps_handle *xhp,
		 prop_object_t obj,
		 void *arg,
		 bool *loop_done)
{
	const char *pkgver;
	bool automatic = false;

	(void)xhp;
	(void)arg;
	(void)loop_done;

	prop_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (automatic == false) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s\n", pkgver);
	}

	return 0;
}
