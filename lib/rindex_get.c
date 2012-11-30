/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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
 * @brief Repository index get routines
 * @defgroup rindex_get Repository index get routines
 */
static prop_dictionary_t
match_pkg_by_pkgver(prop_dictionary_t repod, const char *p)
{
	prop_dictionary_t d = NULL;
	const char *pkgver;
	char *pkgname;

	/* exact match by pkgver */
	if ((pkgname = xbps_pkg_name(p)) == NULL)
		return NULL;

	d = prop_dictionary_get(repod, pkgname);
	if (d) {
		prop_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver);
		if (strcmp(pkgver, p))
			d = NULL;
	}

	free(pkgname);
	return d;
}

static prop_dictionary_t
match_pkg_by_pattern(prop_dictionary_t repod, const char *p)
{
	prop_dictionary_t d = NULL;
	const char *pkgver;
	char *pkgname;

	/* match by pkgpattern in pkgver */
	if ((pkgname = xbps_pkgpattern_name(p)) == NULL) {
		if ((pkgname = xbps_pkg_name(p)))
			return match_pkg_by_pkgver(repod, p);

		return NULL;
	}

	d = prop_dictionary_get(repod, pkgname);
	if (d) {
		prop_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver);
		assert(pkgver);
		if (!xbps_pkgpattern_match(pkgver, p))
			d = NULL;
	}

	free(pkgname);
	return d;
}

const char HIDDEN *
vpkg_user_conf(struct xbps_handle *xhp,
	       const char *vpkg,
	       bool bypattern)
{
	const char *vpkgver, *pkg = NULL;
	char *vpkgname = NULL, *tmp;
	size_t i, j, cnt;

	if (xhp->cfg == NULL)
		return NULL;

	if ((cnt = cfg_size(xhp->cfg, "virtual-package")) == 0) {
		/* no virtual packages configured */
		return NULL;
	}

	for (i = 0; i < cnt; i++) {
		cfg_t *sec = cfg_getnsec(xhp->cfg, "virtual-package", i);
		for (j = 0; j < cfg_size(sec, "targets"); j++) {
			tmp = NULL;
			vpkgver = cfg_getnstr(sec, "targets", j);
			if (strchr(vpkgver, '_') == NULL) {
				tmp = xbps_xasprintf("%s_1", vpkgver);
				vpkgname = xbps_pkg_name(tmp);
				free(tmp);
			} else {
				vpkgname = xbps_pkg_name(vpkgver);
			}
			if (vpkgname == NULL)
				break;
			if (bypattern) {
				if (!xbps_pkgpattern_match(vpkgver, vpkg)) {
					free(vpkgname);
					continue;
				}
			} else {
				if (strcmp(vpkg, vpkgname)) {
					free(vpkgname);
					continue;
				}
			}
			/* virtual package matched in conffile */
			pkg = cfg_title(sec);
			xbps_dbg_printf(xhp,
			    "matched vpkg in conf `%s' for %s\n",
			    pkg, vpkg);
			free(vpkgname);
			break;
		}
	}
	return pkg;
}

prop_dictionary_t
xbps_rindex_get_virtualpkg(struct xbps_rindex *rpi, const char *pkg)
{
	prop_object_t obj;
	prop_object_iterator_t iter;
	prop_dictionary_t pkgd = NULL;
	const char *vpkg;
	bool found = false, bypattern = false;

	if (xbps_pkgpattern_version(pkg))
		bypattern = true;

	/* Try matching vpkg from configuration files */
	vpkg = vpkg_user_conf(rpi->xhp, pkg, bypattern);
	if (vpkg != NULL) {
		if (xbps_pkgpattern_version(vpkg))
			pkgd = match_pkg_by_pattern(rpi->repod, vpkg);
		else if (xbps_pkg_version(vpkg))
			pkgd = match_pkg_by_pkgver(rpi->repod, vpkg);
		else
			pkgd = prop_dictionary_get(rpi->repod, vpkg);

		if (pkgd) {
			found = true;
			goto out;
		}
	}

	/* ... otherwise match the first one in dictionary */
	iter = prop_dictionary_iterator(rpi->repod);
	assert(iter);

	while ((obj = prop_object_iterator_next(iter))) {
		pkgd = prop_dictionary_get_keysym(rpi->repod, obj);
		if (xbps_match_virtual_pkg_in_dict(pkgd, pkg, bypattern)) {
			found = true;
			break;
		}
	}
	prop_object_iterator_release(iter);

out:
	if (found) {
		prop_dictionary_set_cstring_nocopy(pkgd,
				"repository", rpi->uri);
		return pkgd;
	}
	return NULL;
}

prop_dictionary_t
xbps_rindex_get_pkg(struct xbps_rindex *rpi, const char *pkg)
{
	prop_dictionary_t pkgd = NULL;

	if (xbps_pkgpattern_version(pkg))
		pkgd = match_pkg_by_pattern(rpi->repod, pkg);
	else if (xbps_pkg_version(pkg))
		pkgd = match_pkg_by_pkgver(rpi->repod, pkg);
	else
		pkgd = prop_dictionary_get(rpi->repod, pkg);

	if (pkgd) {
		prop_dictionary_set_cstring_nocopy(pkgd,
				"repository", rpi->uri);
		return pkgd;
	}

	return NULL;
}
