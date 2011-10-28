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

#ifdef HAVE_STRCASESTR
# define _GNU_SOURCE    /* for strcasestr(3) */
#endif
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <errno.h>
#include <fnmatch.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

#include <xbps_api.h>
#include "compat.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

void
show_pkg_info(prop_dictionary_t dict)
{
	prop_array_t all_keys;
	prop_object_t obj, keysym;
	const char *keyname;
	char size[8];
	size_t i;

	assert(prop_object_type(dict) == PROP_TYPE_DICTIONARY);
	assert(prop_dictionary_count(dict) != 0);

	all_keys = prop_dictionary_all_keys(dict);
	for (i = 0; i < prop_array_count(all_keys); i++) {
		keysym = prop_array_get(all_keys, i);
		keyname = prop_dictionary_keysym_cstring_nocopy(keysym);
		obj = prop_dictionary_get_keysym(dict, keysym);

		switch (prop_object_type(obj)) {
		case PROP_TYPE_STRING:
			printf("%s: %s\n", keyname,
			    prop_string_cstring_nocopy(obj));
			break;
		case PROP_TYPE_NUMBER:
			printf("%s: ", keyname);
			if (xbps_humanize_number(size,
		    	    (int64_t)prop_number_unsigned_integer_value(obj)) == -1)
				printf("%ju\n",
				    prop_number_unsigned_integer_value(obj));
			else
				printf("%s\n", size);
			break;
		case PROP_TYPE_BOOL:
			printf("%s: %s\n", keyname,
			    prop_bool_true(obj) ? "yes" : "no");
			break;
		case PROP_TYPE_ARRAY:
			/* ignore run_depends, it's shown via 'show-deps' */
			if (strcmp(keyname, "run_depends") == 0)
				break;
			printf("%s:\n", keyname);
			(void)xbps_callback_array_iter_in_dict(dict, keyname,
			    list_strings_sep_in_array, __UNCONST("\t"));
			break;
		default:
			xbps_warn_printf("unknown obj type (key %s)\n",
			    keyname);
			break;
		}
	}
}

int
show_pkg_files(prop_dictionary_t filesd)
{
	prop_array_t array, allkeys;
	prop_object_t obj;
	prop_dictionary_keysym_t ksym;
	const char *keyname, *file;
	size_t i, x;

	allkeys = prop_dictionary_all_keys(filesd);
	for (i = 0; i < prop_array_count(allkeys); i++) {
		ksym = prop_array_get(allkeys, i);
		keyname = prop_dictionary_keysym_cstring_nocopy(ksym);
		if (strcmp(keyname, "dirs") == 0)
			continue;

		array = prop_dictionary_get(filesd, keyname);
		if (prop_object_type(array) != PROP_TYPE_ARRAY ||
		    prop_array_count(array) == 0)
			continue;

		for (x = 0; x < prop_array_count(array); x++) {
			obj = prop_array_get(array, x);
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			printf("%s", file);
			if (prop_dictionary_get_cstring_nocopy(obj,
			    "target", &file))
				printf(" -> %s", file);

			printf("\n");
		}
	}

	return 0;
}

static int
_find_longest_pkgver_cb(prop_object_t obj, void *arg, bool *loop_done)
{
	size_t *len = arg;
	const char *pkgver;

	(void)loop_done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	if (*len == 0 || strlen(pkgver) > *len)
		*len = strlen(pkgver);

	return 0;
}

size_t
find_longest_pkgver(prop_dictionary_t d)
{
	size_t len = 0;

	(void)xbps_callback_array_iter_in_dict(d, "packages",
	    _find_longest_pkgver_cb, &len);

	return len;
}
int
show_pkg_namedesc(prop_object_t obj, void *arg, bool *loop_done)
{
	struct repo_search_data *rsd = arg;
	const char *pkgver, *pkgname, *desc;
	char *tmp = NULL;
	size_t i;

	(void)loop_done;

	assert(prop_object_type(obj) == PROP_TYPE_DICTIONARY);
	assert(rsd->pattern != NULL);

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "short_desc", &desc);

	if ((xbps_pkgpattern_match(pkgver, rsd->pattern) == 1) ||
	    (xbps_pkgpattern_match(desc, rsd->pattern) == 1)  ||
	    (strcasecmp(pkgname, rsd->pattern) == 0) ||
	    (strcasestr(pkgver, rsd->pattern)) ||
	    (strcasestr(desc, rsd->pattern))) {
		tmp = calloc(1, rsd->pkgver_len + 1);
		if (tmp == NULL)
			return errno;

		strlcpy(tmp, pkgver, rsd->pkgver_len + 1);
		for (i = strlen(tmp); i < rsd->pkgver_len; i++)
			tmp[i] = ' ';

		printf(" %s %s\n", tmp, desc);
		free(tmp);
	}

	return 0;
}

int
list_strings_in_array(prop_object_t obj, void *arg, bool *loop_done)
{
	(void)arg;
	(void)loop_done;

	assert(prop_object_type(obj) == PROP_TYPE_STRING);
	print_package_line(prop_string_cstring_nocopy(obj), false);

	return 0;
}

int
list_strings_sep_in_array(prop_object_t obj, void *arg, bool *loop_done)
{
	const char *sep = arg;

	(void)loop_done;

	assert(prop_object_type(obj) == PROP_TYPE_STRING);
	printf("%s%s\n", sep ? sep : "", prop_string_cstring_nocopy(obj));

	return 0;
}

void
print_package_line(const char *str, bool reset)
{
	static size_t cols;
	static bool first;

	if (reset) {
		cols = 0;
		first = false;
		return;
	}
	cols += strlen(str) + 4;
	if (cols <= 80) {
		if (first == false) {
			printf("  ");
			first = true;
		}
	} else {
		printf("\n  ");
		cols = strlen(str) + 4;
	}
	printf("%s ", str);
}
