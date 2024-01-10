/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbps.h>
#include "defs.h"

#define _BOLD	"\033[1m"
#define _RESET	"\033[m"

static void
print_value_obj(const char *keyname, xbps_object_t obj,
		const char *indent, const char *bold,
		const char *reset, bool raw)
{
	xbps_array_t allkeys;
	xbps_object_t obj2, keysym;
	const char *ksymname, *value;
	char size[8];

	if (indent == NULL)
		indent = "";

	switch (xbps_object_type(obj)) {
	case XBPS_TYPE_STRING:
		if (!raw)
			printf("%s%s%s%s: ", indent, bold, keyname, reset);
		printf("%s\n", xbps_string_cstring_nocopy(obj));
		break;
	case XBPS_TYPE_NUMBER:
		if (!raw)
			printf("%s%s%s%s: ", indent, bold, keyname, reset);
		if (xbps_humanize_number(size,
		    (int64_t)xbps_number_unsigned_integer_value(obj)) == -1)
			printf("%ju\n",
			    xbps_number_unsigned_integer_value(obj));
		else
			printf("%s\n", size);
		break;
	case XBPS_TYPE_BOOL:
		if (!raw)
			printf("%s%s%s%s: ", indent, bold, keyname, reset);
		printf("%s\n", xbps_bool_true(obj) ? "yes" : "no");
		break;
	case XBPS_TYPE_ARRAY:
		if (!raw)
			printf("%s%s%s%s:\n", indent, bold, keyname, reset);
		for (unsigned int i = 0; i < xbps_array_count(obj); i++) {
			obj2 = xbps_array_get(obj, i);
			if (xbps_object_type(obj2) == XBPS_TYPE_STRING) {
				value = xbps_string_cstring_nocopy(obj2);
				printf("%s%s%s\n", indent, !raw ? "\t" : "",
				    value);
			} else {
				print_value_obj(keyname, obj2, "  ", bold, reset, raw);
			}
		}
		break;
	case XBPS_TYPE_DICTIONARY:
		if (!raw)
			printf("%s%s%s%s:\n", indent, bold, keyname, reset);
		allkeys = xbps_dictionary_all_keys(obj);
		for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
			keysym = xbps_array_get(allkeys, i);
			ksymname = xbps_dictionary_keysym_cstring_nocopy(keysym);
			obj2 = xbps_dictionary_get_keysym(obj, keysym);
			print_value_obj(ksymname, obj2, "  ", bold, reset, raw);
		}
		xbps_object_release(allkeys);
		if (raw)
			printf("\n");
		break;
	case XBPS_TYPE_DATA:
		if (!raw) {
			xbps_humanize_number(size, (int64_t)xbps_data_size(obj));
			printf("%s%s%s%s: %s\n", indent, bold, keyname, reset, size);
		} else {
			fwrite(xbps_data_data_nocopy(obj), 1, xbps_data_size(obj), stdout);
		}
		break;
	default:
		xbps_warn_printf("unknown obj type (key %s)\n",
		    keyname);
		break;
	}
}

void
show_pkg_info_one(xbps_dictionary_t d, const char *keys)
{
	xbps_object_t obj;
	const char *bold, *reset;
	char *key, *p, *saveptr;
	int v_tty = isatty(STDOUT_FILENO);
	bool raw;

	if (v_tty && !getenv("NO_COLOR")) {
		bold = _BOLD;
		reset = _RESET;
	} else {
		bold = "";
		reset = "";
	}

	if (strchr(keys, ',') == NULL) {
		obj = xbps_dictionary_get(d, keys);
		if (obj == NULL)
			return;
		raw = true;
		if (xbps_object_type(obj) == XBPS_TYPE_DICTIONARY)
			raw = false;
		print_value_obj(keys, obj, NULL, bold, reset, raw);
		return;
	}
	key = strdup(keys);
	if (key == NULL)
		abort();
	for ((p = strtok_r(key, ",", &saveptr)); p;
	    (p = strtok_r(NULL, ",", &saveptr))) {
		obj = xbps_dictionary_get(d, p);
		if (obj == NULL)
			continue;
		raw = true;
		if (xbps_object_type(obj) == XBPS_TYPE_DICTIONARY)
			raw = false;
		print_value_obj(p, obj, NULL, bold, reset, raw);
	}
	free(key);
}

void
show_pkg_info(xbps_dictionary_t dict)
{
	xbps_array_t all_keys;
	xbps_object_t obj, keysym;
	const char *keyname, *bold, *reset;
	int v_tty = isatty(STDOUT_FILENO);

	if (v_tty && !getenv("NO_COLOR")) {
		bold = _BOLD;
		reset = _RESET;
	} else {
		bold = "";
		reset = "";
	}

	all_keys = xbps_dictionary_all_keys(dict);
	for (unsigned int i = 0; i < xbps_array_count(all_keys); i++) {
		keysym = xbps_array_get(all_keys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		obj = xbps_dictionary_get_keysym(dict, keysym);
		/* anything else */
		print_value_obj(keyname, obj, NULL, bold, reset, false);
	}
	xbps_object_release(all_keys);
}

int
show_pkg_files(xbps_dictionary_t filesd)
{
	xbps_array_t array, allkeys;
	xbps_object_t obj;
	xbps_dictionary_keysym_t ksym;
	const char *keyname = NULL, *file = NULL;

	if (xbps_object_type(filesd) != XBPS_TYPE_DICTIONARY)
		return EINVAL;

	allkeys = xbps_dictionary_all_keys(filesd);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		ksym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(ksym);
		if ((strcmp(keyname, "files") &&
		    (strcmp(keyname, "conf_files") &&
		    (strcmp(keyname, "links")))))
			continue;

		array = xbps_dictionary_get(filesd, keyname);
		if (array == NULL || xbps_array_count(array) == 0)
			continue;

		for (unsigned int x = 0; x < xbps_array_count(array); x++) {
			obj = xbps_array_get(array, x);
			if (xbps_object_type(obj) != XBPS_TYPE_DICTIONARY)
				continue;
			xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
			printf("%s", file);
			if (xbps_dictionary_get_cstring_nocopy(obj,
			    "target", &file))
				printf(" -> %s", file);

			printf("\n");
		}
	}
	xbps_object_release(allkeys);

	return 0;
}

int
show_pkg_info_from_metadir(struct xbps_handle *xhp,
			   const char *pkg,
			   const char *option)
{
	xbps_dictionary_t d;

	d = xbps_pkgdb_get_pkg(xhp, pkg);
	if (d == NULL)
		return ENOENT;

	if (option == NULL)
		show_pkg_info(d);
	else
		show_pkg_info_one(d, option);

	return 0;
}

int
show_pkg_files_from_metadir(struct xbps_handle *xhp, const char *pkg)
{
	xbps_dictionary_t d;
	int rv = 0;

	d = xbps_pkgdb_get_pkg_files(xhp, pkg);
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
	xbps_dictionary_t pkgd;

	if (((pkgd = xbps_rpool_get_pkg(xhp, pattern)) == NULL) &&
	    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pattern)) == NULL))
		return errno;

	if (option)
		show_pkg_info_one(pkgd, option);
	else
		show_pkg_info(pkgd);

	return 0;
}

int
cat_file(struct xbps_handle *xhp, const char *pkg, const char *file)
{
	char bfile[PATH_MAX];
	xbps_dictionary_t pkgd;
	int rv;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return errno;

	rv = xbps_pkg_path_or_url(xhp, bfile, sizeof(bfile), pkgd);
	if (rv < 0) {
		xbps_error_printf("could not get package path: %s\n", strerror(-rv));
		return -rv;
	}

	return xbps_archive_fetch_file_into_fd(bfile, file, STDOUT_FILENO);
}

int
repo_cat_file(struct xbps_handle *xhp, const char *pkg, const char *file)
{
	char bfile[PATH_MAX];
	xbps_dictionary_t pkgd;
	int rv;

	pkgd = xbps_rpool_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return errno;

	rv = xbps_pkg_path_or_url(xhp, bfile, sizeof(bfile), pkgd);
	if (rv < 0) {
		xbps_error_printf("could not get package path: %s\n", strerror(-rv));
		return -rv;
	}

	return xbps_archive_fetch_file_into_fd(bfile, file, STDOUT_FILENO);
}

int
repo_show_pkg_files(struct xbps_handle *xhp, const char *pkg)
{
	char bfile[PATH_MAX];
	xbps_dictionary_t pkgd, filesd;
	int rv;

	pkgd = xbps_rpool_get_pkg(xhp, pkg);
	if (pkgd == NULL)
		return errno;

	rv = xbps_pkg_path_or_url(xhp, bfile, sizeof(bfile), pkgd);
	if (rv < 0) {
		xbps_error_printf("could not get package path: %s\n", strerror(-rv));
		return -rv;
	}

	filesd = xbps_archive_fetch_plist(bfile, "/files.plist");
	if (filesd == NULL) {
                if (errno != ENOTSUP && errno != ENOENT) {
			xbps_error_printf("Unexpected error: %s\n", strerror(errno));
		}
		return errno;
	}

	rv = show_pkg_files(filesd);
	xbps_object_release(filesd);
	return rv;
}
