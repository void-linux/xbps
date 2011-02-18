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
#include <unistd.h>

#include <xbps_api.h>
#include "xbps_api_impl.h"

int HIDDEN
xbps_repository_pkg_replaces(prop_dictionary_t transd,
			     prop_dictionary_t pkg_repod)
{
	prop_array_t replaces, unsorted;
	prop_dictionary_t instd, reppkgd;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pattern;

	replaces = prop_dictionary_get(pkg_repod, "replaces");
	if (replaces == NULL || prop_array_count(replaces) == 0)
		return 0;

	iter = prop_array_iterator(replaces);
	if (iter == NULL)
		return ENOMEM;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		pattern = prop_string_cstring_nocopy(obj);
		assert(pattern != NULL);
		/*
		 * Find the installed package that matches the pattern
		 * to be replaced.
		 */
		instd = xbps_find_pkg_dict_installed(pattern, true);
		if (instd == NULL)
			continue;
		/*
		 * Package contains replaces="pkgpattern", but the
		 * package that should be replaced is also in the
		 * transaction and it's going to be updated.
		 */
		unsorted = prop_dictionary_get(transd, "unsorted_deps");
		reppkgd = xbps_find_pkg_in_array_by_pattern(unsorted, pattern);
		if (reppkgd) {
			prop_dictionary_set_bool(reppkgd,
			    "remove-and-update", true);
			prop_object_release(instd);
			continue;
		}
		/*
		 * Add package dictionary into the transaction and mark it
		 * as to be "removed".
		 */
		prop_dictionary_set_cstring_nocopy(instd,
		    "transaction", "remove");
		if (!xbps_add_obj_to_array(unsorted, instd)) {
			prop_object_release(instd);
			prop_object_iterator_release(iter);
			return EINVAL;
		}
	}
	prop_object_iterator_release(iter);

	return 0;
}
