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
#include <errno.h>
#include <fnmatch.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>

#include <xbps_api.h>
#include "defs.h"
#include "../xbps-repo/defs.h"

static void
print_value_obj(const char *keyname, prop_object_t obj, bool raw)
{
	const char *value;
	size_t i;
	char size[8];

	switch (prop_object_type(obj)) {
	case PROP_TYPE_STRING:
		if (!raw)
			printf("%s: ", keyname);
		printf("%s\n", prop_string_cstring_nocopy(obj));
		break;
	case PROP_TYPE_NUMBER:
		if (!raw)
			printf("%s: ", keyname);
		if (xbps_humanize_number(size,
		    (int64_t)prop_number_unsigned_integer_value(obj)) == -1)
			printf("%ju\n",
			    prop_number_unsigned_integer_value(obj));
		else
			printf("%s\n", size);
		break;
	case PROP_TYPE_BOOL:
		if (!raw)
			printf("%s: ", keyname);
		printf("%s\n", prop_bool_true(obj) ? "yes" : "no");
		break;
	case PROP_TYPE_ARRAY:
		if (!raw)
			printf("%s:\n", keyname);
		for (i = 0; i < prop_array_count(obj); i++) {
			prop_array_get_cstring_nocopy(obj, i, &value);
			printf("%s%s%s", !raw ? "\t" : "", value,
			    !raw ? "\n" : " ");
		}
		if (raw)
			printf("\n");
		break;
	default:
		xbps_warn_printf("unknown obj type (key %s)\n",
		    keyname);
		break;
	}
}

void
show_pkg_info_one(prop_dictionary_t d, const char *keys)
{
	prop_object_t obj;
	char *key, *p, *saveptr;

	if (strchr(keys, ',') == NULL) {
		obj = prop_dictionary_get(d, keys);
		if (obj == NULL)
			return;
		print_value_obj(keys, obj, true);
		return;
	}
	key = strdup(keys);
	if (key == NULL)
		abort();
	for ((p = strtok_r(key, ",", &saveptr)); p;
	    (p = strtok_r(NULL, ",", &saveptr))) {
		obj = prop_dictionary_get(d, p);
		if (obj == NULL)
			continue;
		print_value_obj(p, obj, true);
	}
	free(key);
}

void
show_pkg_info(prop_dictionary_t dict)
{
	prop_array_t all_keys;
	prop_object_t obj, keysym;
	const char *keyname;
	size_t i;

	all_keys = prop_dictionary_all_keys(dict);
	for (i = 0; i < prop_array_count(all_keys); i++) {
		keysym = prop_array_get(all_keys, i);
		keyname = prop_dictionary_keysym_cstring_nocopy(keysym);
		obj = prop_dictionary_get_keysym(dict, keysym);
		/* ignore run_depends, it's shown via 'show-deps' */
		if (strcmp(keyname, "run_depends") == 0)
			continue;

		print_value_obj(keyname, obj, false);
	}
	prop_object_release(all_keys);
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
		if (array == NULL || prop_array_count(array) == 0)
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
	prop_object_release(allkeys);

	return 0;
}

static int
_find_longest_pkgver_cb(struct xbps_handle *xhp,
			prop_object_t obj,
			void *arg,
			bool *loop_done)
{
	size_t *len = arg;
	const char *pkgver;

	(void)xhp;
	(void)loop_done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	if (*len == 0 || strlen(pkgver) > *len)
		*len = strlen(pkgver);

	return 0;
}

size_t
find_longest_pkgver(struct xbps_handle *xhp, prop_object_t o)
{
	size_t len = 0;

	if (prop_object_type(o) == PROP_TYPE_ARRAY)
		(void)xbps_callback_array_iter(xhp, o,
		    _find_longest_pkgver_cb, &len);
	else
		(void)xbps_pkgdb_foreach_cb(xhp,
		    _find_longest_pkgver_cb, &len);

	return len;
}

int
list_strings_sep_in_array(struct xbps_handle *xhp,
			  prop_object_t obj,
			  void *arg,
			  bool *loop_done)
{
	const char *sep = arg;

	(void)xhp;
	(void)loop_done;

	printf("%s%s\n", sep ? sep : "", prop_string_cstring_nocopy(obj));

	return 0;
}

size_t
get_maxcols(void)
{
	struct winsize ws;

	if (ioctl(fileno(stdin), TIOCGWINSZ, &ws) == 0)
		return ws.ws_col;

	return 80;
}

void
print_package_line(const char *str, size_t maxcols, bool reset)
{
	static size_t cols;
	static bool first;

	if (reset) {
		cols = 0;
		first = false;
		return;
	}
	cols += strlen(str) + 4;
	if (cols <= maxcols) {
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
