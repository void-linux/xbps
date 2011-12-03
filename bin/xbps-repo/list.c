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
#include <strings.h>
#include <unistd.h>
#include <inttypes.h>

#include "defs.h"
#include "../xbps-bin/defs.h"

int
repo_pkg_list_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct list_pkgver_cb lpc;
	uint16_t idx;
	char *cp;

	(void)done;
	if (arg != NULL) {
		idx = (uint16_t)strtoul(arg, &cp, 0);
		if (rpi->rpi_index != idx)
			return 0;
	}
	lpc.check_state = false;
	lpc.state = 0;
	lpc.pkgver_len = find_longest_pkgver(rpi->rpi_repod);

	printf("From %s repository ...\n", rpi->rpi_uri);
	(void)xbps_callback_array_iter_in_dict(rpi->rpi_repod,
	    "packages", list_pkgs_in_dict, &lpc);

	return 0;
}

int
repo_list_uri_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	const char *pkgidx;
	uint64_t npkgs;

	(void)arg;
	(void)done;

	prop_dictionary_get_cstring_nocopy(rpi->rpi_repod,
	    "pkgindex-version", &pkgidx);
	prop_dictionary_get_uint64(rpi->rpi_repod, "total-pkgs", &npkgs);
	printf("[%u] %s (index %s, " "%" PRIu64 " packages)\n",
	    rpi->rpi_index, rpi->rpi_uri, pkgidx, npkgs);

	return 0;
}

int
repo_search_pkgs_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repo_search_data rsd;
	(void)done;

	rsd.pattern = arg;
	rsd.pkgver_len = find_longest_pkgver(rpi->rpi_repod);

	printf("From %s repository ...\n", rpi->rpi_uri);
	(void)xbps_callback_array_iter_in_dict(rpi->rpi_repod,
	    "packages", show_pkg_namedesc, &rsd);

	return 0;
}
