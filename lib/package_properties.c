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
#include <errno.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

/**
 * @file lib/package_properties.c
 * @brief Package properties routines
 * @defgroup pkgprops Package property functions
 *
 * Set and unset global properties for packages in the regpkgdb
 * plist file and its "properties" array object.
 */
int
xbps_property_set(const char *key, const char *pkgname)
{
	const struct xbps_handle *xhp;
	prop_dictionary_t d, repo_pkgd = NULL, pkgd = NULL;
	prop_array_t props, provides = NULL, virtual = NULL;
	prop_string_t virtualpkg;
	char *plist;
	int rv = 0;
	bool regpkgd_alloc, pkgd_alloc, virtual_alloc, propbool;

	assert(key != NULL);
	assert(pkgname != NULL);

	regpkgd_alloc = pkgd_alloc = virtual_alloc = propbool = false;
	xhp = xbps_handle_get();

	if ((d = xbps_regpkgdb_dictionary_get()) == NULL) {
		/*
		 * If regpkgdb dictionary doesn't exist, create it
		 * and the properties array.
		 */
		d = prop_dictionary_create();
		if (d == NULL) {
			rv = ENOMEM;
			goto out;
		}
		regpkgd_alloc = true;
		props = prop_array_create();
		if (props == NULL) {
			rv = ENOMEM;
			goto out;
		}
		if (!prop_dictionary_set(d, "properties", props)) {
			rv = EINVAL;
			prop_object_release(props);
			goto out;
		}
		prop_object_release(props);
	}
	props = prop_dictionary_get(d, "properties");
	if (prop_object_type(props) != PROP_TYPE_ARRAY) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * If package dictionary doesn't exist, create it.
	 */
	pkgd = xbps_find_pkg_in_array_by_name(props, pkgname);
	if (pkgd == NULL) {
		pkgd = prop_dictionary_create();
		if (pkgd == NULL) {
			rv = ENOMEM;
			goto out;
		}
		pkgd_alloc = true;
		prop_dictionary_set_cstring_nocopy(pkgd, "pkgname", pkgname);
		if (!prop_array_add(props, pkgd)) {
			rv = EINVAL;
			goto out;
		}
	}

	if (strcmp(key, "virtual") == 0) {
		/*
		 * Sets the "virtual" property in package.
		 */
		virtual = prop_dictionary_get(pkgd, "provides");
		if (virtual == NULL) {
			virtual = prop_array_create();
			if (virtual == NULL) {
				rv = ENOMEM;
				goto out;
			}
			virtual_alloc = true;
			virtualpkg = prop_string_create_cstring(pkgname);
			if (virtualpkg == NULL) {
				rv = ENOMEM;
				goto out;
			}
			prop_string_append_cstring(virtualpkg, ">=0");
			prop_dictionary_set(pkgd, "pkgpattern", virtualpkg);
			prop_object_release(virtualpkg);
			virtualpkg = NULL;
		} else {
			/* property already set */
			xbps_dbg_printf("%s: property `%s' already set!\n",
			    pkgname, key);
			rv = EEXIST;
			goto out;
		}
		/*
		 * Get the package object from repository pool.
		 */
		repo_pkgd = xbps_repository_pool_find_pkg(pkgname, false, false);
		if (repo_pkgd == NULL) {
			xbps_dbg_printf("%s: cannot find pkg dictionary "
			    "in repository pool.\n", pkgname);
			rv = ENOENT;
			goto out;
		}
		provides = prop_dictionary_get(repo_pkgd, "provides");
		if (provides == NULL) {
			xbps_dbg_printf("%s: pkg dictionary no provides "
			    "array!\n", pkgname);
			prop_object_release(repo_pkgd);
			rv = EINVAL;
			goto out;
		}
		if (!prop_dictionary_set(pkgd, "provides", provides)) {
			prop_object_release(repo_pkgd);
			rv = EINVAL;
			goto out;
		}
		prop_object_release(repo_pkgd);

	} else if ((strcmp(key, "hold") == 0) ||
		   (strcmp(key, "update-first") == 0)) {
		/*
		 * Sets the property "key" in package.
		 */
		if (prop_dictionary_get_bool(pkgd, key, &propbool)) {
			rv = EEXIST;
			goto out;
		}
		prop_dictionary_set_bool(pkgd, key, true);
	} else {
		/* invalid property */
		rv = EINVAL;
		goto out;
	}
	/*
	 * Add array with new properties set into the regpkgdb
	 * dictionary.
	 */
	if (!prop_dictionary_set(d, "properties", props)) {
		rv = errno;
		goto out;
	}
	/*
	 * Write regpkgdb dictionary to plist file.
	 */
	plist = xbps_xasprintf("%s/%s/%s", xhp->rootdir,
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL) {
		rv = ENOMEM;
		goto out;
	}
	if (!prop_dictionary_externalize_to_zfile(d, plist)) {
		rv = errno;
		goto out;
	}
out:
	if (virtual_alloc)
		prop_object_release(virtual);
	if (pkgd_alloc)
		prop_object_release(pkgd);
	if (regpkgd_alloc)
		prop_object_release(d);

	xbps_regpkgdb_dictionary_release();
	return rv;
}

int
xbps_property_unset(const char *key, const char *pkgname)
{
	const struct xbps_handle *xhp;
	prop_dictionary_t d, pkgd;
	prop_array_t props;
	char *plist;
	int rv = 0;

	assert(key != NULL);
	assert(pkgname != NULL);
	xhp = xbps_handle_get();

	if ((d = xbps_regpkgdb_dictionary_get()) == NULL)
		return ENODEV;

	props = prop_dictionary_get(d, "properties");
	if (prop_object_type(props) != PROP_TYPE_ARRAY) {
		rv = ENODEV;
		goto out;
	}
	pkgd = xbps_find_pkg_in_array_by_name(props, pkgname);
	if (pkgd == NULL) {
		rv = ENODEV;
		goto out;
	}
	if ((strcmp(key, "virtual") == 0)  ||
	    (strcmp(key, "hold") == 0) ||
	    (strcmp(key, "update-first") == 0)) {
		/* remove the property object matching the key */
		prop_dictionary_remove(pkgd, key);
	} else {
		/* invalid property */
		rv = EINVAL;
		goto out;
	}
	/*
	 * If pkg dictionary does not contain any property, remove
	 * the object completely.
	 */
	if (!prop_dictionary_get(d, "virtual") &&
	    !prop_dictionary_get(d, "hold") &&
	    !prop_dictionary_get(d, "update-first"))
		xbps_remove_pkg_from_array_by_name(props, pkgname);

	if (!prop_dictionary_set(d, "properties", props)) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Write regpkgdb dictionary to plist file.
	 */
	plist = xbps_xasprintf("%s/%s/%s", xhp->rootdir,
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL) {
		rv = ENOMEM;
		goto out;
	}
	if (!prop_dictionary_externalize_to_zfile(d, plist)) {
		rv = errno;
		goto out;
	}
out:
	xbps_regpkgdb_dictionary_release();
	return rv;
}
