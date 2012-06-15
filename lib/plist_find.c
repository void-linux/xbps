/*-
 * Copyright (c) 2008-2012 Juan Romero Pardines.
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
 * @file lib/plist_find.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
static prop_dictionary_t
find_pkg_in_array(struct xbps_handle *xhp,
		  prop_array_t array,
		  const char *str,
		  bool bypattern,
		  bool virtual,
		  const char *targetarch)
{
	prop_object_iterator_t iter;
	prop_object_t obj = NULL;
	const char *pkgver, *dpkgn, *arch;
	bool chkarch;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(str != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return NULL;

	while ((obj = prop_object_iterator_next(iter))) {
		chkarch = prop_dictionary_get_cstring_nocopy(obj,
		    "architecture", &arch);
		if (chkarch && !xbps_pkg_arch_match(xhp, arch, targetarch))
			continue;

		if (virtual) {
			/*
			 * Check if package pattern matches
			 * any virtual package version in dictionary.
			 */
			if (xbps_match_virtual_pkg_in_dict(obj, str, bypattern))
				break;
		} else if (bypattern) {
			/*
			 * Check if package pattern matches the
			 * pkgver string object in dictionary.
			 */
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &pkgver))
				continue;
			if (xbps_pkgpattern_match(pkgver, str))
				break;
		} else {
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &dpkgn))
				continue;
			if (strcmp(dpkgn, str) == 0)
				break;
		}
	}
	prop_object_iterator_release(iter);
	if (obj == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return obj;
}

prop_dictionary_t
xbps_find_pkg_in_array_by_name(struct xbps_handle *xhp,
			       prop_array_t array,
			       const char *name,
			       const char *targetarch)
{
	return find_pkg_in_array(xhp, array, name, false, false, targetarch);
}

prop_dictionary_t
xbps_find_pkg_in_array_by_pattern(struct xbps_handle *xhp,
				  prop_array_t array,
				  const char *pattern,
				  const char *targetarch)
{
	return find_pkg_in_array(xhp, array, pattern, true, false, targetarch);
}

prop_dictionary_t
xbps_find_pkg_in_array_by_pkgver(struct xbps_handle *xhp,
				 prop_array_t array,
				 const char *pkgver,
				 const char *targetarch)
{
	prop_object_iterator_t iter;
	prop_object_t obj = NULL;
	const char *rpkgver, *arch;
	bool chkarch, found = false;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(pkgver != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return NULL;

	while ((obj = prop_object_iterator_next(iter))) {
		chkarch = prop_dictionary_get_cstring_nocopy(obj,
		    "architecture", &arch);
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &rpkgver))
			continue;
		if (chkarch && !xbps_pkg_arch_match(xhp, arch, targetarch))
			continue;
		if (strcmp(pkgver, rpkgver) == 0) {
			found = true;
			break;
		}
	}
	prop_object_iterator_release(iter);
	if (found)
		return obj;

	return NULL;
}

prop_dictionary_t
xbps_find_virtualpkg_in_array_by_name(struct xbps_handle *xhp,
				      prop_array_t array,
				      const char *name)
{
	return find_pkg_in_array(xhp, array, name, false, true, NULL);
}

prop_dictionary_t
xbps_find_virtualpkg_in_array_by_pattern(struct xbps_handle *xhp,
					 prop_array_t array,
					 const char *pattern)
{
	return find_pkg_in_array(xhp, array, pattern, true, true, NULL);
}

static const char *
find_virtualpkg_user_in_conf(struct xbps_handle *xhp,
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
				assert(tmp != NULL);
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

static prop_dictionary_t
find_virtualpkg_user_in_array(struct xbps_handle *xhp,
			      prop_array_t array,
			      const char *str,
			      bool bypattern)
{
	const char *vpkgname;

	assert(prop_object_type(array) == PROP_TYPE_ARRAY);
	assert(str != NULL);

	vpkgname = find_virtualpkg_user_in_conf(xhp, str, bypattern);
	if (vpkgname == NULL)
		return NULL;

	return find_pkg_in_array(xhp, array, vpkgname, false, false, NULL);
}

prop_dictionary_t HIDDEN
xbps_find_virtualpkg_conf_in_array_by_name(struct xbps_handle *xhp,
					   prop_array_t array,
					   const char *name)
{
	return find_virtualpkg_user_in_array(xhp, array, name, false);
}

prop_dictionary_t HIDDEN
xbps_find_virtualpkg_conf_in_array_by_pattern(struct xbps_handle *xhp,
					      prop_array_t array,
					      const char *p)
{
	return find_virtualpkg_user_in_array(xhp, array, p, true);
}

static prop_dictionary_t
find_pkg_in_dict(struct xbps_handle *xhp,
		 prop_dictionary_t d,
		 const char *key,
		 const char *str,
		 bool bypattern,
		 bool virtual)
{
	prop_array_t array;

	assert(prop_object_type(d) == PROP_TYPE_DICTIONARY);
	assert(str != NULL);
	assert(key != NULL);

	array = prop_dictionary_get(d, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY)
		return NULL;

	return find_pkg_in_array(xhp, array, str, bypattern, virtual, NULL);
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_name(struct xbps_handle *xhp,
			      prop_dictionary_t d,
			      const char *key,
			      const char *pkgname)
{
	return find_pkg_in_dict(xhp, d, key, pkgname, false, false);
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_pattern(struct xbps_handle *xhp,
				 prop_dictionary_t d,
				 const char *key,
				 const char *pattern)
{
	return find_pkg_in_dict(xhp, d, key, pattern, true, false);
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_pkgver(struct xbps_handle *xhp,
				prop_dictionary_t d,
				const char *key,
				const char *pkgver)
{
	prop_array_t array;

	assert(d != NULL);
	assert(key != NULL);
	assert(pkgver != NULL);

	array = prop_dictionary_get(d, key);
	if (array == NULL)
		return NULL;

	return xbps_find_pkg_in_array_by_pkgver(xhp, array, pkgver, NULL);
}

prop_dictionary_t
xbps_find_virtualpkg_in_dict_by_name(struct xbps_handle *xhp,
				     prop_dictionary_t d,
				     const char *key,
				     const char *name)
{
	return find_pkg_in_dict(xhp, d, key, name, false, true);
}

prop_dictionary_t
xbps_find_virtualpkg_in_dict_by_pattern(struct xbps_handle *xhp,
					prop_dictionary_t d,
					const char *key,
					const char *pattern)
{
	return find_pkg_in_dict(xhp, d, key, pattern, true, true);
}

static prop_dictionary_t
find_pkgd_installed(struct xbps_handle *xhp,
		    const char *str,
		    bool bypattern,
		    bool virtual)
{
	prop_dictionary_t pkgd, rpkgd = NULL;
	pkg_state_t state = 0;
	int rv;

	assert(str != NULL);

	if ((rv = xbps_pkgdb_init(xhp)) != 0) {
		if (rv != ENOENT) {
			xbps_dbg_printf(xhp, "%s: couldn't initialize "
			    "pkgdb: %s\n", strerror(rv));
			return NULL;
		} else if (rv == ENOENT)
			return NULL;
	}

	/* try normal pkg */
	if (virtual == false) {
		pkgd = find_pkg_in_array(xhp, xhp->pkgdb, str,
		    bypattern, false, NULL);
	} else {
		/* virtual pkg set by user in conf */
		pkgd = find_virtualpkg_user_in_array(xhp, xhp->pkgdb,
		    str, bypattern);
		if (pkgd == NULL) {
			/* any virtual pkg in array matching pattern */
			pkgd = find_pkg_in_array(xhp, xhp->pkgdb,
			    str, bypattern, true, NULL);
		}
	}
	/* pkg not found */
	if (pkgd == NULL)
		return NULL;

	if (xbps_pkg_state_dictionary(pkgd, &state) != 0)
		return rpkgd;

	switch (state) {
	case XBPS_PKG_STATE_INSTALLED:
	case XBPS_PKG_STATE_UNPACKED:
		rpkgd = prop_dictionary_copy(pkgd);
		break;
	default:
		/* not fully installed */
		errno = ENOENT;
		break;
	}

	return rpkgd;
}

prop_dictionary_t
xbps_find_pkg_dict_installed(struct xbps_handle *xhp,
			     const char *str,
			     bool bypattern)
{
	return find_pkgd_installed(xhp, str, bypattern, false);
}

prop_dictionary_t
xbps_find_virtualpkg_dict_installed(struct xbps_handle *xhp,
				    const char *str,
				    bool bypattern)
{
	return find_pkgd_installed(xhp, str, bypattern, true);
}
