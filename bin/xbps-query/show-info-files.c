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
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

static void
print_value_obj(const char *keyname, prop_object_t obj,
		const char *indent, bool raw)
{
	prop_array_t allkeys;
	prop_object_t obj2, keysym;
	const char *ksymname, *value;
	size_t i;
	char size[8];

	if (indent == NULL)
		indent = "";

	switch (prop_object_type(obj)) {
	case PROP_TYPE_STRING:
		if (!raw)
			printf("%s%s: ", indent, keyname);
		printf("%s\n", prop_string_cstring_nocopy(obj));
		break;
	case PROP_TYPE_NUMBER:
		if (!raw)
			printf("%s%s: ", indent, keyname);
		if (xbps_humanize_number(size,
		    (int64_t)prop_number_unsigned_integer_value(obj)) == -1)
			printf("%ju\n",
			    prop_number_unsigned_integer_value(obj));
		else
			printf("%s\n", size);
		break;
	case PROP_TYPE_BOOL:
		if (!raw)
			printf("%s%s: ", indent, keyname);
		printf("%s\n", prop_bool_true(obj) ? "yes" : "no");
		break;
	case PROP_TYPE_ARRAY:
		if (!raw)
			printf("%s%s:\n", indent, keyname);
		for (i = 0; i < prop_array_count(obj); i++) {
			obj2 = prop_array_get(obj, i);
			if (prop_object_type(obj2) == PROP_TYPE_STRING) {
				value = prop_string_cstring_nocopy(obj2);
				printf("%s%s%s%s", indent, !raw ? "\t" : "",
				    value, !raw ? "\n" : "");
			} else {
				print_value_obj(keyname, obj2, "  ", raw);
			}
		}
		if (raw)
			printf("\n");
		break;
	case PROP_TYPE_DICTIONARY:
		allkeys = prop_dictionary_all_keys(obj);
		for (i = 0; i < prop_array_count(allkeys); i++) {
			keysym = prop_array_get(allkeys, i);
			ksymname = prop_dictionary_keysym_cstring_nocopy(keysym);
			obj2 = prop_dictionary_get_keysym(obj, keysym);
			print_value_obj(ksymname, obj2, "  ", raw);
		}
		prop_object_release(allkeys);
		if (raw)
			printf("\n");
		break;
	case PROP_TYPE_DATA:
		if (!raw) {
			xbps_humanize_number(size, (int64_t)prop_data_size(obj));
			printf("%s%s: %s\n", indent, keyname, size);
		} else {
			FILE *f;
			char buf[BUFSIZ-1];
			void *data;

			data = prop_data_data(obj);
			f = fmemopen(data, prop_data_size(obj), "r");
			assert(f);
			while (fgets(buf, BUFSIZ-1, f))
				printf("%s", buf);
			fclose(f);
			free(data);
		}
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
		print_value_obj(keys, obj, NULL, true);
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
		print_value_obj(p, obj, NULL, true);
	}
	free(key);
}

static void
print_srcrevs(const char *keyname, prop_string_t obj)
{
	const char *str = prop_string_cstring_nocopy(obj);
	size_t i;

	/* parse string appending a \t after EOL */
	printf("%s:\n  ", keyname);
	for (i = 0; i < strlen(str); i++) {
		if (str[i] == '\n')
			printf("\n  ");
		else
			putchar(str[i]);
	}
	putchar('\n');
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
		/* ignore objs shown by other targets */
		if ((strcmp(keyname, "run_depends") == 0) ||
		    (strcmp(keyname, "files") == 0) ||
		    (strcmp(keyname, "dirs") == 0) ||
		    (strcmp(keyname, "links") == 0))
			continue;

		/* special case for source-revisions obj */
		if (strcmp(keyname, "source-revisions") == 0) {
			print_srcrevs(keyname, obj);
			continue;
		}
		/* anything else */
		print_value_obj(keyname, obj, NULL, false);
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

	if (prop_object_type(filesd) != PROP_TYPE_DICTIONARY)
		return EINVAL;

	allkeys = prop_dictionary_all_keys(filesd);
	for (i = 0; i < prop_array_count(allkeys); i++) {
		ksym = prop_array_get(allkeys, i);
		keyname = prop_dictionary_keysym_cstring_nocopy(ksym);
		if ((strcmp(keyname, "files") &&
		    (strcmp(keyname, "conf_files") &&
		    (strcmp(keyname, "links")))))
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

	return 0;
}

int
show_pkg_info_from_metadir(struct xbps_handle *xhp,
			   const char *pkg,
			   const char *option)
{
	prop_dictionary_t d, pkgdb_d;
	const char *instdate;
	bool autoinst;
	pkg_state_t state;

	pkgdb_d = xbps_pkgdb_get_pkg(xhp, pkg);
	if (pkgdb_d == NULL)
		return ENOENT;

	d = xbps_pkgdb_get_pkg_metadata(xhp, pkg);
	if (d == NULL)
		return ENOENT;

	if (prop_dictionary_get_cstring_nocopy(pkgdb_d,
	    "install-date", &instdate))
		prop_dictionary_set_cstring_nocopy(d, "install-date",
		    instdate);

	if (prop_dictionary_get_bool(pkgdb_d, "automatic-install", &autoinst))
		prop_dictionary_set_bool(d, "automatic-install", autoinst);

	xbps_pkg_state_dictionary(pkgdb_d, &state);
	xbps_set_pkg_state_dictionary(d, state);

	if (option == NULL)
		show_pkg_info(d);
	else
		show_pkg_info_one(d, option);

	return 0;
}

int
show_pkg_files_from_metadir(struct xbps_handle *xhp, const char *pkg)
{
	prop_dictionary_t d;
	int rv = 0;

	d = xbps_pkgdb_get_pkg_metadata(xhp, pkg);
	if (d == NULL)
		return ENOENT;

	rv = show_pkg_files(d);

	return rv;
}

int
repo_show_pkg_info(struct xbps_handle *xhp,
		   const char *pattern,
		   const char *option)
{
	prop_dictionary_t pkgd;

	pkgd = xbps_rpool_get_pkg(xhp, pattern);
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

	pkgd = xbps_rpool_get_pkg_plist(xhp, pkg, "./files.plist");
	if (pkgd == NULL) {
                if (errno != ENOTSUP && errno != ENOENT) {
			fprintf(stderr, "Unexpected error: %s\n",
			    strerror(errno));
			return errno;
		}
	}

	return show_pkg_files(pkgd);
}
