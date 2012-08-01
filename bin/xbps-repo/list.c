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
#include <strings.h>
#include <unistd.h>
#include <inttypes.h>

#include "defs.h"
#include "../xbps-bin/defs.h"

static int
repo_longest_pkgver(struct xbps_handle *xhp,
		    struct xbps_rpool_index *rpi,
		    void *arg,
		    bool *done)
{
	size_t *len = arg, olen = 0;

	(void)done;

	if (*len == 0) {
		*len = find_longest_pkgver(xhp, rpi->repo);
		return 0;
	}
	olen = find_longest_pkgver(xhp, rpi->repo);
	if (olen > *len)
		*len = olen;

	return 0;
}

size_t
repo_find_longest_pkgver(struct xbps_handle *xhp)
{
	size_t len = 0;

	xbps_rpool_foreach(xhp, repo_longest_pkgver, &len);

	return len;
}

int
repo_pkg_list_cb(struct xbps_handle *xhp,
		 struct xbps_rpool_index *rpi,
		 void *arg,
		 bool *done)
{
	struct list_pkgver_cb lpc;
	struct repo_search_data *rsd = arg;

	(void)done;
	if (rsd->arg && strcmp(rpi->uri, rsd->arg))
		return 0;

	lpc.check_state = false;
	lpc.state = 0;
	lpc.pkgver_len = rsd->pkgver_len;
	lpc.maxcols = rsd->maxcols;

	(void)xbps_callback_array_iter(xhp, rpi->repo, list_pkgs_in_dict, &lpc);

	return 0;
}

int
repo_list_uri_cb(struct xbps_handle *xhp,
		 struct xbps_rpool_index *rpi,
		 void *arg,
		 bool *done)
{
	(void)xhp;
	(void)arg;
	(void)done;

	printf("%s (%zu packages)\n", rpi->uri,
	    (size_t)prop_array_count(rpi->repo));

	return 0;
}

int
repo_search_pkgs_cb(struct xbps_handle *xhp,
		    struct xbps_rpool_index *rpi,
		    void *arg,
		    bool *done)
{
	struct repo_search_data *rsd = arg;
	(void)done;

	(void)xbps_callback_array_iter(xhp, rpi->repo, show_pkg_namedesc, rsd);

	return 0;
}
