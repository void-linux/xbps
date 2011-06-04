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
#include <fnmatch.h>
#include <assert.h>

#include <xbps_api.h>
#include "strlcpy.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

struct object_info {
	const char *key;
	const char *descr;
};

static const struct object_info obj_info[] = {
	{ "repository", "Repository: " },
	{ "filename", "Binary package: " },
	{ "filename-size", "Binary package size" },
	{ "filename-sha256", "Binary package SHA256: " },
	{ "archive-compression-type", "Binary package compression type: " },
	{ "pkgname", "Package name: " },
	{ "version", "Version: " },
	{ "installed_size", "Installed size" },
	{ "maintainer", "Maintainer: " },
	{ "architecture", "Architecture: " },
	{ "homepage", "Upstream URL: " },
	{ "license", "License(s): " },
	{ "build_date", "Package build date: " },
	{ "preserve", "Preserve files" },
	{ "replaces", "Replaces these packages" },
	{ "provides", "Provides these virtual packages" },
	{ "conflicts", "Conflicts with" },
	{ "conf_files", "Configuration files" },
	{ "short_desc", "Description: " },
	{ "long_desc", "" },
	{ NULL, NULL }
};

void
show_pkg_info(prop_dictionary_t dict)
{
	const struct object_info *oip;
	prop_object_t obj;
	char size[8];

	assert(dict != NULL);
	assert(prop_dictionary_count(dict) != 0);

	for (oip = obj_info; oip->key != NULL; oip++) {
		obj = prop_dictionary_get(dict, oip->key);
		switch (prop_object_type(obj)) {
		case PROP_TYPE_STRING:
			printf("%s%s\n", oip->descr,
			    prop_string_cstring_nocopy(obj));
			break;
		case PROP_TYPE_NUMBER:
			printf("%s: ", oip->descr);
			if (xbps_humanize_number(size,
		    	    (int64_t)prop_number_unsigned_integer_value(obj)) == -1)
				printf("%ju\n",
				    prop_number_unsigned_integer_value(obj));
			else
				printf("%s\n", size);
			break;
		case PROP_TYPE_BOOL:
			printf("%s: %s\n", oip->descr,
			    prop_bool_true(obj) ? "yes" : "no");
			break;
		case PROP_TYPE_ARRAY:
			printf("%s:\n", oip->descr);
			(void)xbps_callback_array_iter_in_dict(dict, oip->key,
		    	    list_strings_sep_in_array, __UNCONST(" "));
			break;
		default:
			break;
		}
	}
}

int
show_pkg_files(prop_dictionary_t filesd)
{
	prop_array_t array;
	prop_object_iterator_t iter = NULL;
	prop_object_t obj;
	const char *file, *array_str, *target;
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

		iter = xbps_array_iter_from_dict(filesd, array_str);
		if (iter == NULL)
			return EINVAL;

		while ((obj = prop_object_iterator_next(iter))) {
			prop_dictionary_get_cstring_nocopy(obj, "file", &file);
			printf("%s", file);
			if (prop_dictionary_get_cstring_nocopy(obj,
			    "target", &target))
				printf(" -> %s", target);
			printf("\n");
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
