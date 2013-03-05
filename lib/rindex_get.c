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

/**
 * @file lib/rindex_get.c
 * @brief Repository index functions
 * @defgroup rindex Repository index functions
 */
prop_dictionary_t
xbps_rindex_get_virtualpkg(struct xbps_rindex *rpi, const char *pkg)
{
	prop_dictionary_t pkgd;

	pkgd = xbps_find_virtualpkg_in_dict(rpi->xhp, rpi->repod, pkg);
	if (pkgd) {
		prop_dictionary_set_cstring_nocopy(pkgd,
				"repository", rpi->uri);
		return pkgd;
	}
	return NULL;
}

prop_dictionary_t
xbps_rindex_get_pkg(struct xbps_rindex *rpi, const char *pkg)
{
	prop_dictionary_t pkgd;

	pkgd = xbps_find_pkg_in_dict(rpi->repod, pkg);
	if (pkgd) {
		prop_dictionary_set_cstring_nocopy(pkgd,
				"repository", rpi->uri);
		return pkgd;
	}

	return NULL;
}
