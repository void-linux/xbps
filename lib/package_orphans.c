/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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
 * @file lib/package_orphans.c
 * @brief Package orphans handling routines
 * @defgroup pkg_orphans Package orphans handling functions
 *
 * Functions to find installed package orphans.
 *
 * Package orphans were installed automatically by another package,
 * but currently no other packages are depending on.
 *
 * The following image shown below shows the registered packages database
 * dictionary (the array returned by xbps_find_orphan_packages() will
 * contain a package dictionary per orphan found):
 *
 * @image html images/xbps_regpkgdb_dictionary.png
 *
 * Legend:
 *  - <b>Salmon filled box</b>: \a XBPS_REGPKGDB_PLIST file internalized.
 *  - <b>White filled box</b>: mandatory objects.
 *  - <b>Grey filled box</b>: optional objects.
 *  - <b>Green filled box</b>: possible value set in the object, only one
 *    of them is set.
 * 
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e array, bool, integer, string,
 * dictionary.
 */

static int
find_orphan_pkg(prop_object_t obj, void *arg, bool *loop_done)
{
	prop_array_t reqby, orphans = arg;
	prop_object_t obj2, obj3;
	prop_object_iterator_t iter, iter2;
	const char *orphan_pkgname;
	char *pkgname;
	unsigned int ndep = 0, cnt = 0;
	bool automatic = false;
	pkg_state_t state = 0;
	int rv = 0;

	(void)loop_done;

	prop_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (!automatic)
		return 0;

	if ((rv = xbps_get_pkg_state_dictionary(obj, &state)) != 0)
		return rv;

	if (state != XBPS_PKG_STATE_INSTALLED)
		return 0;

	reqby = prop_dictionary_get(obj, "requiredby");
	if (prop_object_type(reqby) != PROP_TYPE_ARRAY)
		return EINVAL;

	if ((cnt = prop_array_count(reqby)) == 0) {
		prop_array_add(orphans, obj);
		return 0;
	}

	iter = prop_array_iterator(reqby);
	if (iter == NULL)
		return ENOMEM;

	while ((obj2 = prop_object_iterator_next(iter)) != NULL) {
		pkgname = xbps_get_pkg_name(prop_string_cstring_nocopy(obj2));
		if (pkgname == NULL) {
			prop_object_iterator_release(iter);
			return EINVAL;
		}

		iter2 = prop_array_iterator(orphans);
		if (iter == NULL) {
			free(pkgname);
			prop_object_iterator_release(iter);
			return ENOMEM;
		}
		while ((obj3 = prop_object_iterator_next(iter2)) != NULL) {
			prop_dictionary_get_cstring_nocopy(obj3,
			    "pkgname", &orphan_pkgname);
			if (strcmp(orphan_pkgname, pkgname) == 0) {
				ndep++;
				break;
			}
		}
		prop_object_iterator_release(iter2);
		free(pkgname);
	}
	prop_object_iterator_release(iter);

	if (ndep != cnt)
		return 0;
	if (!prop_array_add(orphans, obj))
		return EINVAL;

	return 0;
}

prop_array_t
xbps_find_orphan_packages(void)
{
	prop_array_t array;
	prop_dictionary_t dict;
	int rv = 0;

	if ((dict = xbps_regpkgdb_dictionary_get()) == NULL)
		return NULL;
	/*
	 * Prepare an array with all packages previously found.
	 */
	if ((array = prop_array_create()) == NULL)
		return NULL;

	/*
	 * Find out all orphans by looking at the
	 * regpkgdb dictionary and iterate in reverse order
	 * in which packages were installed.
	 */
	rv = xbps_callback_array_iter_reverse_in_dict(dict, "packages",
	    find_orphan_pkg, array);
	if (rv != 0) {
		errno = rv;
		prop_object_release(array);
		return NULL;
	}
	xbps_regpkgdb_dictionary_release();

	return array;
}
