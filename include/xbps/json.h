/* SPDX-FileCopyrightText: Copyright 2023 Duncan Overbruck <mail@duncano.de> */
/* SPDX-License-Identifier: BSD-2-Clause */
#ifndef _XBPS_JSON_H_
#define _XBPS_JSON_H_

#include <stdio.h>
#include <stdint.h>

#include <xbps/xbps_array.h>
#include <xbps/xbps_bool.h>
#include <xbps/xbps_dictionary.h>
#include <xbps/xbps_number.h>
#include <xbps/xbps_object.h>
#include <xbps/xbps_string.h>

struct xbps_json_printer {
	FILE *file;
	unsigned depth;
	uint8_t indent;
	bool compact;
};

int xbps_json_print_escape(struct xbps_json_printer *p, const char *s);
int xbps_json_print_quote(struct xbps_json_printer *p, const char *s);
int xbps_json_print_bool(struct xbps_json_printer *p, bool b);

int xbps_json_print_xbps_string(struct xbps_json_printer *p, xbps_string_t str);
int xbps_json_print_xbps_number(struct xbps_json_printer *p, xbps_number_t num);
int xbps_json_print_xbps_boolean(struct xbps_json_printer *p, xbps_bool_t b);
int xbps_json_print_xbps_array(struct xbps_json_printer *p, xbps_array_t array);
int xbps_json_print_xbps_dictionary(struct xbps_json_printer *p, xbps_dictionary_t dict);
int xbps_json_print_xbps_object(struct xbps_json_printer *p, xbps_object_t obj);

#endif /* !_XBPS_JSON_H_ */
