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
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>

#include <xbps_api.h>
#include "defs.h"

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

int
show_pkg_info_from_metadir(struct xbps_handle *xhp,
			   const char *pkgname,
			   const char *option)
{
	prop_dictionary_t d, pkgdb_d;
	const char *instdate, *pname;
	bool autoinst;

	d = xbps_dictionary_from_metadata_plist(xhp, pkgname, XBPS_PKGPROPS);
	if (d == NULL)
		return EINVAL;

	prop_dictionary_get_cstring_nocopy(d, "pkgname", &pname);
	pkgdb_d = xbps_pkgdb_get_pkgd(xhp, pname, false);
	if (pkgdb_d == NULL) {
		prop_object_release(d);
		return EINVAL;
	}
	if (prop_dictionary_get_cstring_nocopy(pkgdb_d,
	    "install-date", &instdate))
		prop_dictionary_set_cstring_nocopy(d, "install-date",
		    instdate);

	if (prop_dictionary_get_bool(pkgdb_d, "automatic-install", &autoinst))
		prop_dictionary_set_bool(d, "automatic-install", autoinst);

	if (option == NULL)
		show_pkg_info(d);
	else
		show_pkg_info_one(d, option);

	prop_object_release(d);
	return 0;
}

int
show_pkg_files_from_metadir(struct xbps_handle *xhp, const char *pkgname)
{
	prop_dictionary_t d;
	int rv = 0;

	d = xbps_dictionary_from_metadata_plist(xhp, pkgname, XBPS_PKGFILES);
	if (d == NULL)
		return EINVAL;

	rv = show_pkg_files(d);
	prop_object_release(d);

	return rv;
}

int
repo_show_pkg_info(struct xbps_handle *xhp,
		   const char *pattern,
		   const char *option)
{
	prop_dictionary_t pkgd;

	if (xbps_pkgpattern_version(pattern))
		pkgd = xbps_rpool_find_pkg(xhp, pattern, true, false);
	else
		pkgd = xbps_rpool_find_pkg(xhp, pattern, false, true);

	if (pkgd == NULL)
		return errno;

	if (option)
		show_pkg_info_one(pkgd, option);
	else
		show_pkg_info(pkgd);

	return 0;
}

int
repo_show_pkg_files(struct xbps_handle *xhp, const char *pkg)
{
	prop_dictionary_t pkgd;

	pkgd = xbps_rpool_dictionary_metadata_plist(xhp, pkg,
	    "./files.plist");
	if (pkgd == NULL) {
                if (errno != ENOTSUP && errno != ENOENT) {
			fprintf(stderr, "Unexpected error: %s\n",
			    strerror(errno));
			return errno;
		}
	}

	return show_pkg_files(pkgd);
}
