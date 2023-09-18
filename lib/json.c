/* SPDX-FileCopyrightText: Copyright 2023 Duncan Overbruck <mail@duncano.de> */
/* SPDX-License-Identifier: BSD-2-Clause */

#include <sys/types.h>

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "xbps/xbps_array.h"
#include "xbps/xbps_bool.h"
#include "xbps/xbps_dictionary.h"
#include "xbps/xbps_number.h"
#include "xbps/xbps_object.h"
#include "xbps/xbps_string.h"

#include "xbps/json.h"

static int __attribute__ ((format (printf, 2, 3)))
xbps_json_printf(struct xbps_json_printer *p, const char *fmt, ...)
{
	va_list ap;
	int r = 0;
	va_start(ap, fmt);
	if (vfprintf(p->file, fmt, ap) < 0)
		r = errno ? -errno : -EIO;
	va_end(ap);
	return r;
}

int
xbps_json_print_escape(struct xbps_json_printer *p, const char *s)
{
	int r = 0;
	for (; r >= 0 && *s; s++) {
		switch (*s) {
		case '"':  r = xbps_json_printf(p, "\\\""); break;
		case '\\': r = xbps_json_printf(p, "\\\\"); break;
		case '\b': r = xbps_json_printf(p, "\\b"); break;
		case '\f': r = xbps_json_printf(p, "\\f"); break;
		case '\n': r = xbps_json_printf(p, "\\n"); break;
		case '\r': r = xbps_json_printf(p, "\\r"); break;
		case '\t': r = xbps_json_printf(p, "\\t"); break;
		default:
			if ((unsigned)*s < 0x20) {
				r = xbps_json_printf(p, "\\u%04x", *s);
			} else {
				r = xbps_json_printf(p, "%c", *s);
			}
		}
	}
	return r;
}

int
xbps_json_print_quote(struct xbps_json_printer *p, const char *s)
{
	int r;
	if ((r = xbps_json_printf(p, "\"")) < 0)
		return r;
	if ((r = xbps_json_print_escape(p, s)) < 0)
		return r;
	return xbps_json_printf(p, "\"");
}

int
xbps_json_print_bool(struct xbps_json_printer *p, bool b)
{
	return xbps_json_printf(p, b ? "true" : "false");
}

int
xbps_json_print_xbps_string(struct xbps_json_printer *p, xbps_string_t str)
{
	return xbps_json_print_quote(p, xbps_string_cstring_nocopy(str));
}

int
xbps_json_print_xbps_number(struct xbps_json_printer *p, xbps_number_t num)
{
	if (xbps_number_unsigned(num)) {
		return xbps_json_printf(p, "%" PRIu64, xbps_number_unsigned_integer_value(num));
	} else {
		return xbps_json_printf(p, "%" PRId64, xbps_number_integer_value(num));
	}
	return 0;
}

int
xbps_json_print_xbps_boolean(struct xbps_json_printer *p, xbps_bool_t b)
{
	return xbps_json_print_bool(p, xbps_bool_true(b));
}

int
xbps_json_print_xbps_array(struct xbps_json_printer *p, xbps_array_t array)
{
	const char *item_sep = p->compact ? "," : ", ";
	int indent = 0;
	unsigned i = 0;
	int r;
	p->depth++;
	if (!p->compact && p->indent > 0) {
		indent = p->indent*p->depth;
		item_sep = ",\n";
	}
	if ((r = xbps_json_printf(p, "[")) < 0)
		return r;
	for (; i < xbps_array_count(array); i++) {
		if (i == 0) {
			if (indent > 0 && (r = xbps_json_printf(p, "\n%*s", indent, "")) < 0)
				return r;
		} else if ((r = xbps_json_printf(p, "%s%*s", item_sep, indent, "")) < 0) {
			return r;
		}
		if ((r = xbps_json_print_xbps_object(p, xbps_array_get(array, i))) < 0)
			return r;
	}

	p->depth--;
	if (indent > 0 && i > 0)
		return xbps_json_printf(p, "\n%*s]", p->indent*p->depth, "");
	return xbps_json_printf(p, "]");
}

int
xbps_json_print_xbps_dictionary(struct xbps_json_printer *p, xbps_dictionary_t dict)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;
	const char *item_sep = p->compact ? "," : ", ";
	const char *key_sep = p->compact ? ":": ": ";
	bool first = true;
	int indent = 0;
	int r;

	iter = xbps_dictionary_iterator(dict);
	if (!iter)
		return errno ? -errno : -ENOMEM;

	p->depth++;
	if (!p->compact && p->indent > 0) {
		indent = p->depth*p->indent;
		item_sep = ",\n";
	}

	if ((r = xbps_json_printf(p, "{")) < 0)
		goto err;

	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_object_t obj;
		const char *key;

		if (first) {
			first = false;
			if (p->indent > 0 && (r = xbps_json_printf(p, "\n%*s", indent, "")) < 0) {
				goto err;
			}
		} else if ((r = xbps_json_printf(p, "%s%*s", item_sep, indent, "")) < 0) {
				goto err;
		}

		key = xbps_dictionary_keysym_cstring_nocopy(keysym);
		if ((r = xbps_json_print_quote(p, key)) < 0)
			goto err;
		if ((r = xbps_json_printf(p, "%s", key_sep)) < 0)
			goto err;

		obj = xbps_dictionary_get_keysym(dict, keysym);
		if ((r = xbps_json_print_xbps_object(p, obj)) < 0)
			goto err;
	}

	xbps_object_iterator_release(iter);
	p->depth--;
	if (indent > 0 && !first)
		return xbps_json_printf(p, "\n%*s}", p->indent*p->depth, "");
	return xbps_json_printf(p, "}");

err:
	xbps_object_iterator_release(iter);
	return r;
}

int
xbps_json_print_xbps_object(struct xbps_json_printer *p, xbps_object_t obj)
{
	if (!obj) return xbps_json_printf(p, "null");
	switch (xbps_object_type(obj)) {
	case XBPS_TYPE_ARRAY:       return xbps_json_print_xbps_array(p, obj);
	case XBPS_TYPE_BOOL:        return xbps_json_print_xbps_boolean(p, obj);
	case XBPS_TYPE_DATA:        return xbps_json_printf(p, "true");
	case XBPS_TYPE_DICTIONARY:  return xbps_json_print_xbps_dictionary(p, obj);
	case XBPS_TYPE_DICT_KEYSYM: return -EINVAL;
	case XBPS_TYPE_NUMBER:      return xbps_json_print_xbps_number(p, obj);
	case XBPS_TYPE_STRING:      return xbps_json_print_xbps_string(p, obj);
	case XBPS_TYPE_UNKNOWN:     return -EINVAL;
	}
	return -EINVAL;
}
