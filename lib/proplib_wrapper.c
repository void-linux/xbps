/*-
 * Copyright (c) 2013-2015 Juan Romero Pardines.
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

#include "xbps_api_impl.h"
#include <prop/proplib.h>

/* prop_array */

xbps_array_t
xbps_array_create(void)
{
	return prop_array_create();
}

xbps_array_t
xbps_array_create_with_capacity(unsigned int capacity)
{
	return prop_array_create_with_capacity(capacity);
}

xbps_array_t
xbps_array_copy(xbps_array_t a)
{
	return prop_array_copy(a);
}

xbps_array_t
xbps_array_copy_mutable(xbps_array_t a)
{
	return prop_array_copy_mutable(a);
}

unsigned int
xbps_array_capacity(xbps_array_t a)
{
	return prop_array_capacity(a);
}

unsigned int
xbps_array_count(xbps_array_t a)
{
	return prop_array_count(a);
}

bool
xbps_array_ensure_capacity(xbps_array_t a, unsigned int i)
{
	return prop_array_ensure_capacity(a, i);
}

void
xbps_array_make_immutable(xbps_array_t a)
{
	prop_array_make_immutable(a);
}

bool
xbps_array_mutable(xbps_array_t a)
{
	return prop_array_mutable(a);
}

xbps_object_iterator_t
xbps_array_iterator(xbps_array_t a)
{
	return prop_array_iterator(a);
}

xbps_object_t
xbps_array_get(xbps_array_t a, unsigned int i)
{
	return prop_array_get(a, i);
}

bool
xbps_array_set(xbps_array_t a, unsigned int i, xbps_object_t obj)
{
	return prop_array_set(a, i, obj);
}

bool
xbps_array_add(xbps_array_t a, xbps_object_t obj)
{
	return prop_array_add(a, obj);
}

bool
xbps_array_add_first(xbps_array_t a, xbps_object_t obj)
{
	return prop_array_add_first(a, obj);
}

void
xbps_array_remove(xbps_array_t a, unsigned int i)
{
	prop_array_remove(a, i);
}

bool
xbps_array_equals(xbps_array_t a, xbps_array_t b)
{
	return prop_array_equals(a, b);
}

char *
xbps_array_externalize(xbps_array_t a)
{
	return prop_array_externalize(a);
}

xbps_array_t
xbps_array_internalize(const char *s)
{
	return prop_array_internalize(s);
}

bool
xbps_array_externalize_to_file(xbps_array_t a, const char *s)
{
	return prop_array_externalize_to_file(a, s);
}

bool
xbps_array_externalize_to_zfile(xbps_array_t a, const char *s)
{
	return prop_array_externalize_to_zfile(a, s);
}

xbps_array_t
xbps_array_internalize_from_file(const char *s)
{
	return prop_array_internalize_from_file(s);
}

xbps_array_t
xbps_array_internalize_from_zfile(const char *s)
{
	return prop_array_internalize_from_zfile(s);
}

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool
xbps_array_get_bool(xbps_array_t a, unsigned int i, bool *b)
{
	return prop_array_get_bool(a, i, b);
}

bool
xbps_array_set_bool(xbps_array_t a, unsigned int i, bool b)
{
	return prop_array_set_bool(a, i, b);
}

bool
xbps_array_get_int8(xbps_array_t a, unsigned int i, int8_t *v)
{
	return prop_array_get_int8(a, i, v);
}

bool
xbps_array_get_uint8(xbps_array_t a, unsigned int i, uint8_t *v)
{
	return prop_array_get_uint8(a, i, v);
}

bool
xbps_array_set_int8(xbps_array_t a, unsigned int i, int8_t v)
{
	return prop_array_set_int8(a, i, v);
}

bool
xbps_array_set_uint8(xbps_array_t a, unsigned int i, uint8_t v)
{
	return prop_array_set_uint8(a, i, v);
}

bool
xbps_array_get_int16(xbps_array_t a, unsigned int i, int16_t *v)
{
	return prop_array_get_int16(a, i, v);
}

bool
xbps_array_get_uint16(xbps_array_t a, unsigned int i, uint16_t *v)
{
	return prop_array_get_uint16(a, i, v);
}

bool
xbps_array_set_int16(xbps_array_t a, unsigned int i, int16_t v)
{
	return prop_array_set_int16(a, i, v);
}

bool
xbps_array_set_uint16(xbps_array_t a, unsigned int i, uint16_t v)
{
	return prop_array_set_uint16(a, i, v);
}

bool
xbps_array_get_int32(xbps_array_t a, unsigned int i, int32_t *v)
{
	return prop_array_get_int32(a, i, v);
}

bool
xbps_array_get_uint32(xbps_array_t a, unsigned int i, uint32_t *v)
{
	return prop_array_get_uint32(a, i, v);
}

bool
xbps_array_set_int32(xbps_array_t a, unsigned int i, int32_t v)
{
	return prop_array_set_int32(a, i, v);
}

bool
xbps_array_set_uint32(xbps_array_t a, unsigned int i, uint32_t v)
{
	return prop_array_set_uint32(a, i, v);
}

bool
xbps_array_get_int64(xbps_array_t a, unsigned int i, int64_t *v)
{
	return prop_array_get_int64(a, i, v);
}

bool
xbps_array_get_uint64(xbps_array_t a, unsigned int i, uint64_t *v)
{
	return prop_array_get_uint64(a, i, v);
}

bool
xbps_array_set_int64(xbps_array_t a, unsigned int i, int64_t v)
{
	return prop_array_set_int64(a, i, v);
}

bool
xbps_array_set_uint64(xbps_array_t a, unsigned int i, uint64_t v)
{
	return prop_array_set_uint64(a, i, v);
}

bool
xbps_array_add_int8(xbps_array_t a, int8_t v)
{
	return prop_array_add_int8(a, v);
}

bool
xbps_array_add_uint8(xbps_array_t a, uint8_t v)
{
	return prop_array_add_uint8(a, v);
}

bool
xbps_array_add_int16(xbps_array_t a, int16_t v)
{
	return prop_array_add_int16(a, v);
}

bool
xbps_array_add_uint16(xbps_array_t a, uint16_t v)
{
	return prop_array_add_uint16(a, v);
}

bool
xbps_array_add_int32(xbps_array_t a, int32_t v)
{
	return prop_array_add_int32(a, v);
}

bool
xbps_array_add_uint32(xbps_array_t a, uint32_t v)
{
	return prop_array_add_uint32(a, v);
}

bool
xbps_array_add_int64(xbps_array_t a, int64_t v)
{
	return prop_array_add_int64(a, v);
}

bool
xbps_array_add_uint64(xbps_array_t a, uint64_t v)
{
	return prop_array_add_uint64(a, v);
}

bool
xbps_array_get_cstring(xbps_array_t a, unsigned int i, char **s)
{
	return prop_array_get_cstring(a, i, s);
}

bool
xbps_array_set_cstring(xbps_array_t a, unsigned int i, const char *s)
{
	return prop_array_set_cstring(a, i, s);
}

bool
xbps_array_add_cstring(xbps_array_t a, const char *s)
{
	return prop_array_add_cstring(a, s);
}

bool
xbps_array_add_cstring_nocopy(xbps_array_t a, const char *s)
{
	return prop_array_add_cstring_nocopy(a, s);
}

bool
xbps_array_get_cstring_nocopy(xbps_array_t a, unsigned int i, const char **s)
{
	return prop_array_get_cstring_nocopy(a, i, s);
}

bool
xbps_array_set_cstring_nocopy(xbps_array_t a, unsigned int i, const char *s)
{
	return prop_array_set_cstring_nocopy(a, i, s);
}

bool
xbps_array_add_and_rel(xbps_array_t a, xbps_object_t o)
{
	return prop_array_add_and_rel(a, o);
}

/* prop_bool */

xbps_bool_t
xbps_bool_create(bool v)
{
	return prop_bool_create(v);
}

xbps_bool_t
xbps_bool_copy(xbps_bool_t b)
{
	return prop_bool_copy(b);
}

bool
xbps_bool_true(xbps_bool_t b)
{
	return prop_bool_true(b);
}

bool
xbps_bool_equals(xbps_bool_t a, xbps_bool_t b)
{
	return prop_bool_equals(a, b);
}

/* prop_data */

xbps_data_t
xbps_data_create_data(const void *v, size_t s)
{
	return prop_data_create_data(v, s);
}

xbps_data_t
xbps_data_create_data_nocopy(const void *v, size_t s)
{
	return prop_data_create_data_nocopy(v, s);
}

xbps_data_t
xbps_data_copy(xbps_data_t d)
{
	return prop_data_copy(d);
}

size_t
xbps_data_size(xbps_data_t d)
{
	return prop_data_size(d);
}

void *
xbps_data_data(xbps_data_t d)
{
	return prop_data_data(d);
}

const void *
xbps_data_data_nocopy(xbps_data_t d)
{
	return prop_data_data_nocopy(d);
}

bool
xbps_data_equals(xbps_data_t a, xbps_data_t b)
{
	return prop_data_equals(a, b);
}

bool
xbps_data_equals_data(xbps_data_t d, const void *v, size_t s)
{
	return prop_data_equals_data(d, v, s);
}

/* prop_dictionary */

xbps_dictionary_t
xbps_dictionary_create(void)
{
	return prop_dictionary_create();
}

xbps_dictionary_t
xbps_dictionary_create_with_capacity(unsigned int i)
{
	return prop_dictionary_create_with_capacity(i);
}

xbps_dictionary_t
xbps_dictionary_copy(xbps_dictionary_t d)
{
	return prop_dictionary_copy(d);
}

xbps_dictionary_t
xbps_dictionary_copy_mutable(xbps_dictionary_t d)
{
	return prop_dictionary_copy_mutable(d);
}

unsigned int
xbps_dictionary_count(xbps_dictionary_t d)
{
	return prop_dictionary_count(d);
}

bool
xbps_dictionary_ensure_capacity(xbps_dictionary_t d, unsigned int i)
{
	return prop_dictionary_ensure_capacity(d, i);
}

void
xbps_dictionary_make_immutable(xbps_dictionary_t d)
{
	prop_dictionary_make_immutable(d);
}

xbps_object_iterator_t
xbps_dictionary_iterator(xbps_dictionary_t d)
{
	return prop_dictionary_iterator(d);
}

xbps_array_t
xbps_dictionary_all_keys(xbps_dictionary_t d)
{
	return prop_dictionary_all_keys(d);
}

xbps_object_t
xbps_dictionary_get(xbps_dictionary_t d, const char *s)
{
	return prop_dictionary_get(d, s);
}

bool
xbps_dictionary_set(xbps_dictionary_t d, const char *s, xbps_object_t o)
{
	return prop_dictionary_set(d, s, o);
}

void
xbps_dictionary_remove(xbps_dictionary_t d, const char *s)
{
	prop_dictionary_remove(d, s);
}

xbps_object_t
xbps_dictionary_get_keysym(xbps_dictionary_t d, xbps_dictionary_keysym_t k)
{
	return prop_dictionary_get_keysym(d, k);
}

bool
xbps_dictionary_set_keysym(xbps_dictionary_t d, xbps_dictionary_keysym_t k,
					   xbps_object_t o)
{
	return prop_dictionary_set_keysym(d, k, o);
}

void
xbps_dictionary_remove_keysym(xbps_dictionary_t d, xbps_dictionary_keysym_t k)
{
	prop_dictionary_remove_keysym(d, k);
}

bool
xbps_dictionary_equals(xbps_dictionary_t a, xbps_dictionary_t b)
{
	return prop_dictionary_equals(a, b);
}

char *
xbps_dictionary_externalize(xbps_dictionary_t d)
{
	return prop_dictionary_externalize(d);
}

xbps_dictionary_t
xbps_dictionary_internalize(const char *s)
{
	return prop_dictionary_internalize(s);
}

bool
xbps_dictionary_externalize_to_file(xbps_dictionary_t d, const char *s)
{
	return prop_dictionary_externalize_to_file(d, s);
}

bool
xbps_dictionary_externalize_to_zfile(xbps_dictionary_t d, const char *s)
{
	return prop_dictionary_externalize_to_zfile(d, s);
}

xbps_dictionary_t
xbps_dictionary_internalize_from_file(const char *s)
{
	return prop_dictionary_internalize_from_file(s);
}

xbps_dictionary_t
xbps_dictionary_internalize_from_zfile(const char *s)
{
	return prop_dictionary_internalize_from_zfile(s);
}

const char *
xbps_dictionary_keysym_cstring_nocopy(xbps_dictionary_keysym_t k)
{
	return prop_dictionary_keysym_cstring_nocopy(k);
}

bool
xbps_dictionary_keysym_equals(xbps_dictionary_keysym_t a, xbps_dictionary_keysym_t b)
{
	return prop_dictionary_keysym_equals(a, b);
}

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool
xbps_dictionary_get_dict(xbps_dictionary_t d, const char *s,
					 xbps_dictionary_t *rd)
{
	return prop_dictionary_get_dict(d, s, rd);
}

bool
xbps_dictionary_get_bool(xbps_dictionary_t d, const char *s, bool *b)
{
	return prop_dictionary_get_bool(d, s, b);
}

bool
xbps_dictionary_set_bool(xbps_dictionary_t d, const char *s, bool b)
{
	return prop_dictionary_set_bool(d, s, b);
}

bool
xbps_dictionary_get_int8(xbps_dictionary_t d, const char *s, int8_t *v)
{
	return prop_dictionary_get_int8(d, s, v);
}

bool
xbps_dictionary_get_uint8(xbps_dictionary_t d, const char *s, uint8_t *v)
{
	return prop_dictionary_get_uint8(d, s, v);
}

bool
xbps_dictionary_set_int8(xbps_dictionary_t d, const char *s, int8_t v)
{
	return prop_dictionary_set_int8(d, s, v);
}

bool
xbps_dictionary_set_uint8(xbps_dictionary_t d, const char *s, uint8_t v)
{
	return prop_dictionary_set_uint8(d, s, v);
}

bool
xbps_dictionary_get_int16(xbps_dictionary_t d, const char *s, int16_t *v)
{
	return prop_dictionary_get_int16(d, s, v);
}

bool
xbps_dictionary_get_uint16(xbps_dictionary_t d, const char *s, uint16_t *v)
{
	return prop_dictionary_get_uint16(d, s, v);
}

bool
xbps_dictionary_set_int16(xbps_dictionary_t d, const char *s, int16_t v)
{
	return prop_dictionary_set_int16(d, s, v);
}

bool
xbps_dictionary_set_uint16(xbps_dictionary_t d, const char *s, uint16_t v)
{
	return prop_dictionary_set_uint16(d, s, v);
}

bool
xbps_dictionary_get_int32(xbps_dictionary_t d, const char *s, int32_t *v)
{
	return prop_dictionary_get_int32(d, s, v);
}

bool
xbps_dictionary_get_uint32(xbps_dictionary_t d, const char *s, uint32_t *v)
{
	return prop_dictionary_get_uint32(d, s, v);
}

bool
xbps_dictionary_set_int32(xbps_dictionary_t d, const char *s, int32_t v)
{
	return prop_dictionary_set_int32(d, s, v);
}

bool
xbps_dictionary_set_uint32(xbps_dictionary_t d, const char *s, uint32_t v)
{
	return prop_dictionary_set_uint32(d, s, v);
}

bool
xbps_dictionary_get_int64(xbps_dictionary_t d, const char *s, int64_t *v)
{
	return prop_dictionary_get_int64(d, s, v);
}

bool
xbps_dictionary_get_uint64(xbps_dictionary_t d, const char *s, uint64_t *v)
{
	return prop_dictionary_get_uint64(d, s, v);
}

bool
xbps_dictionary_set_int64(xbps_dictionary_t d, const char *s, int64_t v)
{
	return prop_dictionary_set_int64(d, s, v);
}

bool
xbps_dictionary_set_uint64(xbps_dictionary_t d, const char *s, uint64_t v)
{
	return prop_dictionary_set_uint64(d, s, v);
}

bool
xbps_dictionary_get_cstring(xbps_dictionary_t d, const char *s, char **ss)
{
	return prop_dictionary_get_cstring(d, s, ss);
}

bool
xbps_dictionary_set_cstring(xbps_dictionary_t d, const char *s, const char *ss)
{
	return prop_dictionary_set_cstring(d, s, ss);
}

bool
xbps_dictionary_get_cstring_nocopy(xbps_dictionary_t d, const char *s, const char **ss)
{
	return prop_dictionary_get_cstring_nocopy(d, s, ss);
}

bool
xbps_dictionary_set_cstring_nocopy(xbps_dictionary_t d, const char *s, const char *ss)
{
	return prop_dictionary_set_cstring_nocopy(d, s, ss);
}

bool
xbps_dictionary_set_and_rel(xbps_dictionary_t d, const char *s, xbps_object_t o)
{
	return prop_dictionary_set_and_rel(d, s, o);
}

/* prop_number */

xbps_number_t
xbps_number_create_integer(int64_t v)
{
	return prop_number_create_integer(v);
}

xbps_number_t
xbps_number_create_unsigned_integer(uint64_t v)
{
	return prop_number_create_unsigned_integer(v);
}

xbps_number_t
xbps_number_copy(xbps_number_t n)
{
	return prop_number_copy(n);
}

int
xbps_number_size(xbps_number_t n)
{
	return prop_number_size(n);
}

bool
xbps_number_unsigned(xbps_number_t n)
{
	return prop_number_unsigned(n);
}

int64_t
xbps_number_integer_value(xbps_number_t n)
{
	return prop_number_integer_value(n);
}

uint64_t
xbps_number_unsigned_integer_value(xbps_number_t n)
{
	return prop_number_unsigned_integer_value(n);
}

bool
xbps_number_equals(xbps_number_t n, xbps_number_t nn)
{
	return prop_number_equals(n, nn);
}

bool
xbps_number_equals_integer(xbps_number_t n, int64_t v)
{
	return prop_number_equals_integer(n, v);
}

bool
xbps_number_equals_unsigned_integer(xbps_number_t n, uint64_t v)
{
	return prop_number_equals_unsigned_integer(n, v);
}

/* prop_object */

void
xbps_object_retain(xbps_object_t o)
{
	prop_object_retain(o);
}

void
xbps_object_release(xbps_object_t o)
{
	prop_object_release(o);
}

xbps_type_t
xbps_object_type(xbps_object_t o)
{
	return (xbps_type_t)prop_object_type(o);
}

bool
xbps_object_equals(xbps_object_t o, xbps_object_t oo)
{
	return prop_object_equals(o, oo);
}

bool
xbps_object_equals_with_error(xbps_object_t o, xbps_object_t oo, bool *b)
{
	return prop_object_equals_with_error(o, oo, b);
}

xbps_object_t
xbps_object_iterator_next(xbps_object_iterator_t o)
{
	return prop_object_iterator_next(o);
}

void
xbps_object_iterator_reset(xbps_object_iterator_t o)
{
	prop_object_iterator_reset(o);
}

void
xbps_object_iterator_release(xbps_object_iterator_t o)
{
	prop_object_iterator_release(o);
}

/* prop_string */

xbps_string_t
xbps_string_create(void)
{
	return prop_string_create();
}

xbps_string_t
xbps_string_create_cstring(const char *s)
{
	return prop_string_create_cstring(s);
}

xbps_string_t
xbps_string_create_cstring_nocopy(const char *s)
{
	return prop_string_create_cstring_nocopy(s);
}

xbps_string_t
xbps_string_copy(xbps_string_t s)
{
	return prop_string_copy(s);
}

xbps_string_t
xbps_string_copy_mutable(xbps_string_t s)
{
	return prop_string_copy_mutable(s);
}

size_t
xbps_string_size(xbps_string_t s)
{
	return prop_string_size(s);
}

bool
xbps_string_mutable(xbps_string_t s)
{
	return prop_string_mutable(s);
}

char *
xbps_string_cstring(xbps_string_t s)
{
	return prop_string_cstring(s);
}

const char *
xbps_string_cstring_nocopy(xbps_string_t s)
{
	return prop_string_cstring_nocopy(s);
}

bool
xbps_string_append(xbps_string_t s, xbps_string_t ss)
{
	return prop_string_append(s, ss);
}

bool
xbps_string_append_cstring(xbps_string_t s, const char *ss)
{
	return prop_string_append_cstring(s, ss);
}

bool
xbps_string_equals(xbps_string_t s, xbps_string_t ss)
{
	return prop_string_equals(s, ss);
}

bool
xbps_string_equals_cstring(xbps_string_t s, const char *ss)
{
	return prop_string_equals_cstring(s, ss);
}

/* xbps specific helpers */
xbps_array_t
xbps_plist_array_from_file(const char *path)
{
	xbps_array_t a;

	a = xbps_array_internalize_from_zfile(path);
	if (xbps_object_type(a) != XBPS_TYPE_ARRAY) {
		xbps_dbg_printf(
		    "xbps: failed to internalize array from %s\n", path);
	}
	return a;
}

xbps_dictionary_t
xbps_plist_dictionary_from_file(const char *path)
{
	xbps_dictionary_t d;

	d = xbps_dictionary_internalize_from_zfile(path);
	if (xbps_object_type(d) != XBPS_TYPE_DICTIONARY) {
		xbps_dbg_printf(
		    "xbps: failed to internalize dict from %s\n", path);
	}
	return d;
}
