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
#include <string.h>
#include <errno.h>

#include <xbps_api.h>
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
find_pkg_dict_from_plist(const char *plist,
			 const char *key,
			 const char *str,
			 bool bypattern)
{
	prop_dictionary_t dict, obj, res;

	assert(plist != NULL);
	assert(str != NULL);

	dict = prop_dictionary_internalize_from_zfile(plist);
	if (dict == NULL) {
		xbps_dbg_printf("cannot internalize %s for pkg %s: %s",
		    plist, str, strerror(errno));
		return NULL;
	}
	if (bypattern)
		obj = xbps_find_pkg_in_dict_by_pattern(dict, key, str);
	else
		obj = xbps_find_pkg_in_dict_by_name(dict, key, str);

	if (obj == NULL) {
		prop_object_release(dict);
		return NULL;
	}
	res = prop_dictionary_copy(obj);
	prop_object_release(dict);

	return res;
}

prop_dictionary_t
xbps_find_pkg_dict_from_plist_by_name(const char *plist,
				      const char *key,
				      const char *pkgname)
{
	return find_pkg_dict_from_plist(plist, key, pkgname, false);
}

prop_dictionary_t
xbps_find_pkg_dict_from_plist_by_pattern(const char *plist,
					 const char *key,
					 const char *pattern)
{
	return find_pkg_dict_from_plist(plist, key, pattern, true);
}

bool
xbps_find_virtual_pkg_in_dict(prop_dictionary_t d,
			      const char *str,
			      bool bypattern)
{
	prop_array_t provides;
	bool found = false;

	if ((provides = prop_dictionary_get(d, "provides"))) {
		if (bypattern)
			found = xbps_find_pkgpattern_in_array(provides, str);
		else
			found = xbps_find_pkgname_in_array(provides, str);
	}
	return found;
}

static prop_dictionary_t
find_pkg_in_array(prop_array_t array, const char *str, bool bypattern)
{
	prop_object_iterator_t iter;
	prop_object_t obj = NULL;
	const char *pkgver, *dpkgn;

	assert(array != NULL);
	assert(str != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return NULL;

	while ((obj = prop_object_iterator_next(iter))) {
		if (bypattern) {
			/*
			 * Check if package pattern matches the
			 * pkgver string object in dictionary.
			 */
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &pkgver))
				continue;
			if (xbps_pkgpattern_match(pkgver, __UNCONST(str)))
				break;
		} else {
			if (!prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &dpkgn))
				continue;
			if (strcmp(dpkgn, str) == 0)
				break;
		}
		/*
		 * Finally check if package pattern matches
		 * any virtual package version in dictionary.
		 */
		if (xbps_find_virtual_pkg_in_dict(obj, str, bypattern))
			break;
	}
	prop_object_iterator_release(iter);
	if (obj == NULL) {
		errno = ENOENT;
		return NULL;
	}
	return obj;
}

prop_dictionary_t
xbps_find_pkg_in_array_by_name(prop_array_t array, const char *name)
{
	return find_pkg_in_array(array, name, false);
}

prop_dictionary_t
xbps_find_pkg_in_array_by_pattern(prop_array_t array, const char *pattern)
{
	return find_pkg_in_array(array, pattern, true);
}

static const char *
find_virtualpkg_user_in_regpkgdb(const char *virtualpkg, bool bypattern)
{
	prop_array_t virtual;
	prop_dictionary_t d;
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *pkg = NULL;
	bool found = false;

	if ((d = xbps_regpkgdb_dictionary_get()) == NULL)
		return NULL;

	if ((iter = xbps_get_array_iter_from_dict(d, "properties")) == NULL) {
		xbps_regpkgdb_dictionary_release();
		return NULL;
	}
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		virtual = prop_dictionary_get(obj, "provides");
		if (virtual == NULL)
			continue;
		if (bypattern)
			found = xbps_find_pkgpattern_in_array(virtual, virtualpkg);
		else
			found = xbps_find_pkgname_in_array(virtual, virtualpkg);

		if (!found)
			continue;
		if (bypattern)
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgpattern", &pkg);
		else
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &pkg);

		break;
	}
	prop_object_iterator_release(iter);
	xbps_regpkgdb_dictionary_release();

	return pkg;
}

static prop_dictionary_t
find_virtualpkg_user_in_array(prop_array_t array,
			      const char *str,
			      bool bypattern)
{
	prop_object_t obj = NULL;
	prop_object_iterator_t iter;
	const char *pkgver, *dpkgn, *virtualpkg;

	assert(array != NULL);
	assert(str != NULL);

	virtualpkg = find_virtualpkg_user_in_regpkgdb(str, bypattern);
	if (virtualpkg == NULL)
		return NULL;

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return NULL;

	while ((obj = prop_object_iterator_next(iter))) {
		if (bypattern) {
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &pkgver);
			if (xbps_pkgpattern_match(pkgver,
			    __UNCONST(virtualpkg)))
				break;
		} else {
			prop_dictionary_get_cstring_nocopy(obj,
			    "pkgname", &dpkgn);
			if (strcmp(dpkgn, virtualpkg) == 0)
				break;
		}
	}
	prop_object_iterator_release(iter);
	return obj;
}

static prop_dictionary_t
find_pkg_in_dict(prop_dictionary_t d,
		 const char *key,
		 const char *str,
		 bool bypattern,
		 bool virtual)
{
	prop_array_t array;

	assert(d != NULL);
	assert(str != NULL);
	assert(key != NULL);

	array = prop_dictionary_get(d, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY)
		return NULL;

	if (virtual)
		return find_virtualpkg_user_in_array(array, str, bypattern);

	return find_pkg_in_array(array, str, bypattern);
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_name(prop_dictionary_t d,
			      const char *key,
			      const char *pkgname)
{
	return find_pkg_in_dict(d, key, pkgname, false, false);
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_pattern(prop_dictionary_t d,
				 const char *key,
				 const char *pattern)
{
	return find_pkg_in_dict(d, key, pattern, true, false);
}

prop_dictionary_t HIDDEN
xbps_find_virtualpkg_user_in_dict_by_name(prop_dictionary_t d,
					  const char *key,
					  const char *name)
{
	return find_pkg_in_dict(d, key, name, false, true);
}

prop_dictionary_t HIDDEN
xbps_find_virtualpkg_user_in_dict_by_pattern(prop_dictionary_t d,
					     const char *key,
					     const char *pattern)
{
	return find_pkg_in_dict(d, key, pattern, true, true);
}

prop_dictionary_t
xbps_find_pkg_dict_installed(const char *str, bool bypattern)
{
	prop_dictionary_t d, pkgd, rpkgd = NULL;
	pkg_state_t state = 0;

	assert(str != NULL);

	if ((d = xbps_regpkgdb_dictionary_get()) == NULL)
		return NULL;

	pkgd = find_pkg_in_dict(d, "packages", str, bypattern, false);
	if (pkgd == NULL)
		goto out;

	if (xbps_get_pkg_state_dictionary(pkgd, &state) != 0)
		goto out;

	switch (state) {
	case XBPS_PKG_STATE_INSTALLED:
	case XBPS_PKG_STATE_UNPACKED:
		rpkgd = prop_dictionary_copy(pkgd);
		break;
	case XBPS_PKG_STATE_CONFIG_FILES:
		errno = ENOENT;
		xbps_dbg_printf("'%s' installed but its state is "
		    "config-files\n",str);
		break;
	default:
		break;
	}
out:
	xbps_regpkgdb_dictionary_release();
	return rpkgd;
}

static bool
find_string_in_array(prop_array_t array, const char *str, int mode)
{
	prop_object_iterator_t iter;
	prop_object_t obj;
	const char *pkgdep;
	char *curpkgname;
	bool found = false;

	assert(array != NULL);
	assert(str != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return false;

	while ((obj = prop_object_iterator_next(iter))) {
		assert(prop_object_type(obj) == PROP_TYPE_STRING);
		if (mode == 0) {
			/* match by string */
			if (prop_string_equals_cstring(obj, str)) {
				found = true;
				break;
			}
		} else if (mode == 1) {
			/* match by pkgname */
			pkgdep = prop_string_cstring_nocopy(obj);
			curpkgname = xbps_get_pkg_name(pkgdep);
			if (curpkgname == NULL)
				break;
			if (strcmp(curpkgname, str) == 0) {
				free(curpkgname);
				found = true;
				break;
			}
			free(curpkgname);
		} else if (mode == 2) {
			/* match by pkgpattern */
			pkgdep = prop_string_cstring_nocopy(obj);
			if (xbps_pkgpattern_match(pkgdep, __UNCONST(str))) {
				found = true;
				break;
			}
		}
	}
	prop_object_iterator_release(iter);

	return found;
}

bool
xbps_find_string_in_array(prop_array_t array, const char *str)
{
	return find_string_in_array(array, str, 0);
}

bool
xbps_find_pkgname_in_array(prop_array_t array, const char *pkgname)
{
	return find_string_in_array(array, pkgname, 1);
}

bool
xbps_find_pkgpattern_in_array(prop_array_t array, const char *pattern)
{
	return find_string_in_array(array, pattern, 2);
}
