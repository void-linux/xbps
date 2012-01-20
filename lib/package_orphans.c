/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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
 * dictionary (the array returned by xbps_find_pkg_orphans() will
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

struct orphan_data {
	prop_array_t array;
	prop_array_t orphans_user;
};

static int
find_orphan_pkg(prop_object_t obj, void *arg, bool *loop_done)
{
	struct orphan_data *od = arg;
	prop_array_t reqby;
	prop_object_t obj2;
	prop_object_iterator_t iter;
	const char *pkgdep, *curpkgname;
	char *pkgdepname;
	unsigned int ndep = 0, cnt = 0;
	bool automatic = false;
	size_t i;
	int rv = 0;
	pkg_state_t state;

	(void)loop_done;
	/*
	 * Skip packages that were not installed automatically.
	 */
	prop_dictionary_get_bool(obj, "automatic-install", &automatic);
	if (!automatic)
		return 0;

	if ((rv = xbps_pkg_state_dictionary(obj, &state)) != 0)
		return rv;
	/*
	 * Skip packages that aren't fully installed or half removed.
	 */
	if (state != XBPS_PKG_STATE_INSTALLED &&
	    state != XBPS_PKG_STATE_HALF_REMOVED)
		return 0;

	reqby = prop_dictionary_get(obj, "requiredby");
	if (reqby == NULL || ((cnt = prop_array_count(reqby)) == 0)) {
		/*
		 * Add packages with empty or missing "requiredby" arrays.
		 */
		prop_array_add(od->array, obj);
		return 0;
	}
	/*
	 * Add packages that only have 1 entry matching any pkgname
	 * object in the user supplied array of strings.
	 */
	if (od->orphans_user != NULL && cnt == 1) {
		for (i = 0; i < prop_array_count(od->orphans_user); i++) {
			prop_array_get_cstring_nocopy(od->orphans_user,
			    i, &curpkgname);
			if (xbps_match_pkgname_in_array(reqby, curpkgname)) {
				prop_array_add(od->array, obj);
				return 0;
			}
		}
	}
	iter = prop_array_iterator(reqby);
	if (iter == NULL)
		return ENOMEM;
	/*
	 * Iterate over the requiredby array and add current pkg
	 * when all pkg dependencies are already in requiredby
	 * or any pkgname object in the user supplied array of
	 * strings match.
	 */
	while ((obj2 = prop_object_iterator_next(iter)) != NULL) {
		pkgdep = prop_string_cstring_nocopy(obj2);
		if (pkgdep == NULL) {
			prop_object_iterator_release(iter);
			return EINVAL;
		}
		if (xbps_find_pkg_in_array_by_pattern(od->array, pkgdep))
			ndep++;
		if (od->orphans_user == NULL)
			continue;

		pkgdepname = xbps_pkg_name(pkgdep);
		if (pkgdepname == NULL) {
			prop_object_iterator_release(iter);
			return ENOMEM;
		}
		for (i = 0; i < prop_array_count(od->orphans_user); i++) {
			prop_array_get_cstring_nocopy(od->orphans_user,
			    i, &curpkgname);
			if (strcmp(curpkgname, pkgdepname) == 0) {
				ndep++;
				break;
			}
		}
		free(pkgdepname);
	}
	prop_object_iterator_release(iter);

	if (ndep != cnt)
		return 0;
	if (!prop_array_add(od->array, obj))
		return EINVAL;

	return 0;
}

prop_array_t
xbps_find_pkg_orphans(prop_array_t orphans_user)
{
	prop_array_t array = NULL;
	struct orphan_data od;
	int rv = 0;

	/*
	 * Prepare an array with all packages previously found.
	 */
	if ((od.array = prop_array_create()) == NULL)
		return NULL;
	/*
	 * Find out all orphans by looking at pkgdb and iterating in reverse
	 * order in which packages were installed.
	 */
	od.orphans_user = orphans_user;
	rv = xbps_pkgdb_foreach_reverse_pkg_cb(find_orphan_pkg, &od);
	if (rv != 0) {
		errno = rv;
		prop_object_release(od.array);
		return NULL;
	}
	array = prop_array_copy(od.array);
	prop_object_release(od.array);
	return array;
}
