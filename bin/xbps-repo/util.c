/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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
#include <fnmatch.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

void
show_pkg_info_only_repo(prop_dictionary_t dict)
{
	prop_object_t obj;
	char size[64];
	int rv = 0;

	obj = prop_dictionary_get(dict, "filename");
	if (prop_object_type(obj) == PROP_TYPE_STRING) {
		printf("Filename: %s", prop_string_cstring_nocopy(obj));
		obj = prop_dictionary_get(dict, "filename-size");
		if (prop_object_type(obj) == PROP_TYPE_NUMBER) {
			rv = xbps_humanize_number(size, 5,
			    (int64_t)prop_number_unsigned_integer_value(obj),
			    "", HN_AUTOSCALE, HN_B|HN_DECIMAL|HN_NOSPACE);
			if (rv == -1)
				printf(" (size: %ju)\n",
				    prop_number_unsigned_integer_value(obj));
			else
				printf(" (size: %s)\n", size);
		} else
			printf("\n");
	}

	obj = prop_dictionary_get(dict, "filename-sha256");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("SHA256: %s\n", prop_string_cstring_nocopy(obj));
}

void
show_pkg_info(prop_dictionary_t dict)
{
	prop_object_t obj;
	char size[64], *sep;
	int rv = 0;

	assert(dict != NULL);
	assert(prop_dictionary_count(dict) != 0);

	obj = prop_dictionary_get(dict, "archive-compression-type");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("Compression type: %s\n",
		    prop_string_cstring_nocopy(obj));

	obj = prop_dictionary_get(dict, "pkgname");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("Package: %s\n", prop_string_cstring_nocopy(obj));

	obj = prop_dictionary_get(dict, "installed_size");
	if (prop_object_type(obj) == PROP_TYPE_NUMBER) {
		printf("Installed size: ");
		rv = xbps_humanize_number(size, 5,
		    (int64_t)prop_number_unsigned_integer_value(obj),
		    "", HN_AUTOSCALE, HN_B|HN_DECIMAL|HN_NOSPACE);
		if (rv == -1)
			printf("%ju\n",
			    prop_number_unsigned_integer_value(obj));
		else
			printf("%s\n", size);
	}

	obj = prop_dictionary_get(dict, "maintainer");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("Maintainer: %s\n", prop_string_cstring_nocopy(obj));

	obj = prop_dictionary_get(dict, "architecture");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("Architecture: %s\n", prop_string_cstring_nocopy(obj));

	obj = prop_dictionary_get(dict, "version");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("Version: %s\n", prop_string_cstring_nocopy(obj));

	obj = prop_dictionary_get(dict, "preserve");
	if (prop_object_type(obj) == PROP_TYPE_BOOL)
		printf("Preserve files: %s\n",
		    prop_bool_true(obj) ? "yes" : "no");

	obj = prop_dictionary_get(dict, "replaces");
	if (prop_object_type(obj) == PROP_TYPE_ARRAY) {
		printf("Replaces: ");
		(void)xbps_callback_array_iter_in_dict(dict, "replaces",
		    list_strings_sep_in_array, NULL);
	}

	obj = prop_dictionary_get(dict, "conflicts");
	if (prop_object_type(obj) == PROP_TYPE_ARRAY) {
		printf("Conflicts: ");
		(void)xbps_callback_array_iter_in_dict(dict, "conflicts",
		    list_strings_sep_in_array, NULL);
	}

	obj = prop_dictionary_get(dict, "conf_files");
	if (prop_object_type(obj) == PROP_TYPE_ARRAY) {
		printf("Configuration files:\n");
		sep = "  ";
		(void)xbps_callback_array_iter_in_dict(dict, "conf_files",
		    list_strings_sep_in_array, sep);
		printf("\n");
	}

	obj = prop_dictionary_get(dict, "short_desc");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf("Description: %s", prop_string_cstring_nocopy(obj));

	obj = prop_dictionary_get(dict, "long_desc");
	if (prop_object_type(obj) == PROP_TYPE_STRING)
		printf(" %s\n", prop_string_cstring_nocopy(obj));
}

int
show_pkg_files(prop_dictionary_t filesd)
{
	prop_array_t array;
	prop_object_iterator_t iter = NULL;
	prop_object_t obj;
	const char *file, *array_str;
	int i = 0;

	/* This will print links, conf_files and files respectively. */
	for (i = 0; i < 3; i++) {
		if (i == 0)
			array_str = "links";
		else if (i == 1)
			array_str = "conf_files";
		else
			array_str = "files";

		array = prop_dictionary_get(filesd, array_str);
		if (array == NULL || prop_array_count(array) == 0)
			continue;

		iter = xbps_get_array_iter_from_dict(filesd, array_str);
		if (iter == NULL)
			return EINVAL;

		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			printf("%s\n", file);
		}
		prop_object_iterator_release(iter);
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
	    (strcmp(pkgname, rsd->pattern) == 0) ||
	    (strstr(pkgver, rsd->pattern)) || (strstr(desc, rsd->pattern))) {
		tmp = malloc(rsd->pkgver_len + 1);
		if (tmp == NULL)
			return errno;

		memcpy(tmp, pkgver, rsd->pkgver_len);
		for (i = strlen(tmp); i < rsd->pkgver_len; i++)
			tmp[i] = ' ';

		tmp[rsd->pkgver_len + 1] = '\0';
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
	print_package_line(prop_string_cstring_nocopy(obj));

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
print_package_line(const char *str)
{
	static size_t cols;
	static bool first;

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
