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

#include "prop_number.h"
#include "prop_string.h"
#include "xbps/xbps_object.h"
#include "xbps/xbps_dictionary.h"
#include "xbps/xbps_array.h"
#include "xbps/xbps_bool.h"
#include "xbps/xbps_string.h"

#include "xbps_api_impl.h"

#include "proplib.h"

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

xbps_array_t
xbps_array_internalize_from_file(const char *s)
{
	return prop_array_internalize_from_file(s);
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
xbps_array_get_schar(xbps_array_t a, unsigned int i, signed char *v)
{
	return prop_array_get_schar(a, i, v);
}

bool
xbps_array_get_uchar(xbps_array_t a, unsigned int i, unsigned char *v)
{
	return prop_array_get_uchar(a, i, v);
}

bool
xbps_array_set_schar(xbps_array_t a, unsigned int i, signed char v)
{
	return prop_array_set_schar(a, i, v);
}

bool
xbps_array_set_uchar(xbps_array_t a, unsigned int i, unsigned char v)
{
	return prop_array_set_uchar(a, i, v);
}

bool
xbps_array_get_short(xbps_array_t a, unsigned int i, short *v)
{
	return prop_array_get_short(a, i, v);
}

bool
xbps_array_get_ushort(xbps_array_t a, unsigned int i, unsigned short *v)
{
	return prop_array_get_ushort(a, i, v);
}

bool
xbps_array_set_short(xbps_array_t a, unsigned int i, short v)
{
	return prop_array_set_short(a, i, v);
}

bool
xbps_array_set_ushort(xbps_array_t a, unsigned int i, unsigned short v)
{
	return prop_array_set_ushort(a, i, v);
}

bool
xbps_array_get_int(xbps_array_t a, unsigned int i, int *v)
{
	return prop_array_get_int(a, i, v);
}

bool
xbps_array_get_uint(xbps_array_t a, unsigned int i, unsigned int *v)
{
	return prop_array_get_uint(a, i, v);
}

bool
xbps_array_set_int(xbps_array_t a, unsigned int i, int v)
{
	return prop_array_set_int(a, i, v);
}

bool
xbps_array_set_uint(xbps_array_t a, unsigned int i, unsigned int v)
{
	return prop_array_set_uint(a, i, v);
}

bool
xbps_array_get_long(xbps_array_t a, unsigned int i, long *v)
{
	return prop_array_get_long(a, i, v);
}

bool
xbps_array_get_ulong(xbps_array_t a, unsigned int i, unsigned long *v)
{
	return prop_array_get_ulong(a, i, v);
}

bool
xbps_array_set_long(xbps_array_t a, unsigned int i, long v)
{
	return prop_array_set_long(a, i, v);
}

bool
xbps_array_set_ulong(xbps_array_t a, unsigned int i, unsigned long v)
{
	return prop_array_set_ulong(a, i, v);
}

bool
xbps_array_get_longlong(xbps_array_t a, unsigned int i, long long *v)
{
	return prop_array_get_longlong(a, i, v);
}

bool
xbps_array_get_ulonglong(xbps_array_t a, unsigned int i, unsigned long long *v)
{
	return prop_array_get_ulonglong(a, i, v);
}

bool
xbps_array_set_longlong(xbps_array_t a, unsigned int i, long long v)
{
	return prop_array_set_longlong(a, i, v);
}

bool
xbps_array_set_ulonglong(xbps_array_t a, unsigned int i, unsigned long long v)
{
	return prop_array_set_ulonglong(a, i, v);
}

bool
xbps_array_get_intptr(xbps_array_t a, unsigned int i, intptr_t *v)
{
	return prop_array_get_intptr(a, i, v);
}

bool
xbps_array_get_uintptr(xbps_array_t a, unsigned int i, uintptr_t *v)
{
	return prop_array_get_uintptr(a, i, v);
}

bool
xbps_array_set_intptr(xbps_array_t a, unsigned int i, intptr_t v)
{
	return prop_array_set_intptr(a, i, v);
}

bool
xbps_array_set_uintptr(xbps_array_t a, unsigned int i, uintptr_t v)
{
	return prop_array_set_uintptr(a, i, v);
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
xbps_array_set_and_rel(xbps_array_t a, unsigned int i, xbps_object_t v)
{
	return prop_array_set_and_rel(a, i, v);
}

bool
xbps_array_add_bool(xbps_array_t a, bool v)
{
	return prop_array_add_bool(a, v);
}

bool
xbps_array_add_schar(xbps_array_t a, signed char v)
{
	return prop_array_add_schar(a, v);
}

bool
xbps_array_add_uchar(xbps_array_t a, unsigned char v)
{
	return prop_array_add_uchar(a, v);
}

bool
xbps_array_add_short(xbps_array_t a, short v)
{
	return prop_array_add_short(a, v);
}
bool
xbps_array_add_ushort(xbps_array_t a, unsigned short v)
{
	return prop_array_add_ushort(a, v);
}

bool
xbps_array_add_int(xbps_array_t a, int v)
{
	return prop_array_add_int(a, v);
}
bool
xbps_array_add_uint(xbps_array_t a, unsigned int v)
{
	return prop_array_add_uint(a, v);
}

bool
xbps_array_add_long(xbps_array_t a, long v)
{
	return prop_array_add_long(a, v);
}
bool
xbps_array_add_ulong(xbps_array_t a, unsigned long v)
{
	return prop_array_add_ulong(a, v);
}

bool
xbps_array_add_longlong(xbps_array_t a, long long v)
{
	return prop_array_add_longlong(a, v);
}
bool
xbps_array_add_ulonglong(xbps_array_t a, unsigned long long v)
{
	return prop_array_add_ulonglong(a, v);
}

bool
xbps_array_add_intptr(xbps_array_t a, intptr_t v)
{
	return prop_array_add_intptr(a, v);
}
bool
xbps_array_add_uintptr(xbps_array_t a, uintptr_t v)
{
	return prop_array_add_uintptr(a, v);
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
xbps_array_get_string(xbps_array_t a, unsigned int i, const char **v)
{
	return prop_array_get_string(a, i, v);
}

bool
xbps_array_set_string(xbps_array_t a, unsigned int i, const char *v)
{
	return prop_array_set_string(a, i, v);
}

bool
xbps_array_add_string(xbps_array_t a, const char *v)
{
return prop_array_add_string(a, v);
}

bool
xbps_array_set_string_nocopy(xbps_array_t a, unsigned int i, const char *v)
{
	return prop_array_set_string_nocopy(a, i, v);
}

bool
xbps_array_add_string_nocopy(xbps_array_t a, const char *v)
{
	return prop_array_add_string_nocopy(a, v);
}

bool
xbps_array_get_data(xbps_array_t a, unsigned int i, const void **v, size_t *l)
{
	return prop_array_get_data(a, i, v, l);
}

bool
xbps_array_set_data(xbps_array_t a, unsigned int i, const void *v, size_t l)
{
	return prop_array_set_data(a, i, v, l);
}

bool
xbps_array_add_data(xbps_array_t a, const void *v, size_t l)
{
	return prop_array_add_data(a, v, l);
}

bool
xbps_array_set_data_nocopy(xbps_array_t a, unsigned int i, const void *v, size_t l)
{
	return prop_array_set_data_nocopy(a, i, v, l);
}

bool
xbps_array_add_data_nocopy(xbps_array_t a, const void *v, size_t l)
{
	return prop_array_add_data_nocopy(a, v, l);
}

bool
xbps_array_add_and_rel(xbps_array_t a, xbps_object_t o)
{
	return prop_array_add_and_rel(a, o);
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
xbps_bool_value(xbps_bool_t b)
{
	return prop_bool_value(b);
}

bool
xbps_bool_equals(xbps_bool_t a, xbps_bool_t b)
{
	return prop_bool_equals(a, b);
}

/* prop_data */

xbps_data_t
xbps_data_create_copy(const void *v, size_t s)
{
	return prop_data_create_copy(v, s);
}

xbps_data_t
xbps_data_create_nocopy(const void *v, size_t s)
{
	return prop_data_create_nocopy(v, s);
}

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

xbps_dictionary_t
xbps_dictionary_internalize_from_file(const char *s)
{
	return prop_dictionary_internalize_from_file(s);
}

const char *
xbps_dictionary_keysym_value(xbps_dictionary_keysym_t k)
{
	return prop_dictionary_keysym_cstring_nocopy(k);
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
xbps_dictionary_get_schar(xbps_dictionary_t d, const char *key, signed char *v)
{
	return prop_dictionary_get_schar(d, key, v);
}

bool
xbps_dictionary_get_uchar(xbps_dictionary_t d, const char *key, unsigned char *v)
{
	return prop_dictionary_get_uchar(d, key, v);
}

bool
xbps_dictionary_set_schar(xbps_dictionary_t d, const char *key, signed char v)
{
	return prop_dictionary_set_schar(d, key, v);
}

bool
xbps_dictionary_set_uchar(xbps_dictionary_t d, const char *key, unsigned char v)
{
	return prop_dictionary_set_uchar(d, key, v);
}

bool
xbps_dictionary_get_short(xbps_dictionary_t d, const char *key, short *v)
{
	return prop_dictionary_get_short(d, key, v);
}

bool
xbps_dictionary_get_ushort(xbps_dictionary_t d, const char *key, unsigned short *v)
{
	return prop_dictionary_get_ushort(d, key, v);
}

bool
xbps_dictionary_set_short(xbps_dictionary_t d, const char *key, short v)
{
	return prop_dictionary_set_short(d, key, v);
}

bool
xbps_dictionary_set_ushort(xbps_dictionary_t d, const char *key, unsigned short v)
{
	return prop_dictionary_set_ushort(d, key, v);
}

bool
xbps_dictionary_get_int(xbps_dictionary_t d, const char *key, int *v)
{
	return prop_dictionary_get_int(d, key, v);
}

bool
xbps_dictionary_get_uint(xbps_dictionary_t d, const char *key, unsigned int *v)
{
	return prop_dictionary_get_uint(d, key, v);
}

bool
xbps_dictionary_set_int(xbps_dictionary_t d, const char *key, int v)
{
	return prop_dictionary_set_int(d, key, v);
}

bool
xbps_dictionary_set_uint(xbps_dictionary_t d, const char *key, unsigned int v)
{
	return prop_dictionary_set_uint(d, key, v);
}

bool
xbps_dictionary_get_long(xbps_dictionary_t d, const char *key, long *v)
{
	return prop_dictionary_get_long(d, key, v);
}

bool
xbps_dictionary_get_ulong(xbps_dictionary_t d, const char *key, unsigned long *v)
{
	return prop_dictionary_get_ulong(d, key, v);
}

bool
xbps_dictionary_set_long(xbps_dictionary_t d, const char *key, long v)
{
	return prop_dictionary_set_long(d, key, v);
}

bool
xbps_dictionary_set_ulong(xbps_dictionary_t d, const char *key, unsigned long v)
{
	return prop_dictionary_set_ulong(d, key, v);
}

bool
xbps_dictionary_get_longlong(xbps_dictionary_t d, const char *key, long long *v)
{
	return prop_dictionary_get_longlong(d, key, v);
}

bool
xbps_dictionary_get_ulonglong(xbps_dictionary_t d, const char *key, unsigned long long *v)
{
	return prop_dictionary_get_ulonglong(d, key, v);
}

bool
xbps_dictionary_set_longlong(xbps_dictionary_t d, const char *key, long long v)
{
	return prop_dictionary_set_longlong(d, key, v);
}

bool
xbps_dictionary_set_ulonglong(xbps_dictionary_t d, const char *key, unsigned long long v)
{
	return prop_dictionary_set_ulonglong(d, key, v);
}

bool
xbps_dictionary_get_intptr(xbps_dictionary_t d, const char *key, intptr_t *v)
{
	return prop_dictionary_get_intptr(d, key, v);
}

bool
xbps_dictionary_get_uintptr(xbps_dictionary_t d, const char *key, uintptr_t *v)
{
	return prop_dictionary_get_uintptr(d, key, v);
}

bool
xbps_dictionary_set_intptr(xbps_dictionary_t d, const char *key, intptr_t v)
{
	return prop_dictionary_set_intptr(d, key, v);
}

bool
xbps_dictionary_set_uintptr(xbps_dictionary_t d, const char *key, uintptr_t v)
{
	return prop_dictionary_set_uintptr(d, key, v);
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
xbps_dictionary_get_string(xbps_dictionary_t d, const char *s, const char **v)
{
	return prop_dictionary_get_string(d, s, v);
}

bool
xbps_dictionary_set_string(xbps_dictionary_t d, const char *s, const char *v)
{
	return prop_dictionary_set_string(d, s, v);
}

bool
xbps_dictionary_set_string_nocopy(xbps_dictionary_t d, const char *s, const char *v)
{
	return prop_dictionary_set_string_nocopy(d, s, v);
}


bool
xbps_dictionary_get_data(xbps_dictionary_t d, const char *s, const void **v, size_t *l)
{
	return prop_dictionary_get_data(d, s, v, l);
}

bool
xbps_dictionary_set_data(xbps_dictionary_t d, const char *s, const void *v, size_t l)
{
	return prop_dictionary_set_data(d, s, v, l);
}

bool
xbps_dictionary_set_data_nocopy(xbps_dictionary_t d, const char *s, const void *v, size_t l)
{
	return prop_dictionary_set_data_nocopy(d, s, v, l);
}


bool
xbps_dictionary_set_and_rel(xbps_dictionary_t d, const char *s, xbps_object_t v)
{
	return prop_dictionary_set_and_rel(d, s, v);
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

/* prop_number */

xbps_number_t
xbps_number_create_signed(intmax_t v)
{
	return prop_number_create_signed(v);
}

xbps_number_t
xbps_number_create_unsigned(uintmax_t v)
{
	return prop_number_create_unsigned(v);
}

intmax_t
xbps_number_signed_value(xbps_number_t n)
{
	return prop_number_signed_value(n);
}

uintmax_t
xbps_number_unsigned_value(xbps_number_t n)
{
	return prop_number_unsigned_value(n);
}

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

bool
xbps_number_schar_value(xbps_number_t n, signed char *v)
{
	return prop_number_schar_value(n, v);
}

bool
xbps_number_short_value(xbps_number_t n, short *v)
{
	return prop_number_short_value(n, v);
}

bool
xbps_number_int_value(xbps_number_t n, int *v)
{
	return prop_number_int_value(n, v);
}
bool
xbps_number_long_value(xbps_number_t n, long *v)
{
	return prop_number_long_value(n, v);
}

bool
xbps_number_longlong_value(xbps_number_t n, long long *v)
{
	return prop_number_longlong_value(n, v);
}

bool
xbps_number_intptr_value(xbps_number_t n, intptr_t *v)
{
	return prop_number_intptr_value(n, v);
}

bool
xbps_number_int8_value(xbps_number_t n, int8_t *v)
{
	return prop_number_int8_value(n, v);
}

bool
xbps_number_int16_value(xbps_number_t n, int16_t *v)
{
	return prop_number_int16_value(n, v);
}

bool
xbps_number_int32_value(xbps_number_t n, int32_t *v)
{
	return prop_number_int32_value(n, v);
}

bool
xbps_number_int64_value(xbps_number_t n, int64_t *v)
{
	return prop_number_int64_value(n, v);
}

bool
xbps_number_uchar_value(xbps_number_t n, unsigned char *v)
{
	return prop_number_uchar_value(n, v);
}

bool
xbps_number_ushort_value(xbps_number_t n, unsigned short *v)
{
	return prop_number_ushort_value(n, v);
}

bool
xbps_number_uint_value(xbps_number_t n, unsigned int *v)
{
	return prop_number_uint_value(n, v);
}

bool
xbps_number_ulong_value(xbps_number_t n, unsigned long *v)
{
	return prop_number_ulong_value(n, v);
}

bool
xbps_number_ulonglong_value(xbps_number_t n, unsigned long long *v)
{
	return prop_number_ulonglong_value(n, v);
}

bool
xbps_number_uintptr_value(xbps_number_t n, uintptr_t *v)
{
	return prop_number_uintptr_value(n, v);
}

bool
xbps_number_uint8_value(xbps_number_t n, uint8_t *v)
{
	return prop_number_uint8_value(n, v);
}

bool
xbps_number_uint16_value(xbps_number_t n, uint16_t *v)
{
	return prop_number_uint16_value(n, v);
}

bool
xbps_number_uint32_value(xbps_number_t n, uint32_t *v)
{
	return prop_number_uint32_value(n, v);
}

bool
xbps_number_uint64_value(xbps_number_t n, uint64_t *v)
{
	return prop_number_uint64_value(n, v);
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
xbps_number_equals_signed(xbps_number_t n, intmax_t v)
{
	return prop_number_equals_signed(n, v);
}

bool
xbps_number_equals_unsigned(xbps_number_t n, uintmax_t v)
{
	return prop_number_equals_unsigned(n, v);
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

char *
xbps_object_externalize(xbps_object_t o)
{
	return prop_object_externalize(o);
}

char *
xbps_object_externalize_with_format(xbps_object_t o, xbps_prop_format_t fmt)
{
	return prop_object_externalize_with_format(o, (prop_format_t)fmt);
}

bool
xbps_object_externalize_to_file(xbps_object_t o, const char *path)
{
	return prop_object_externalize_to_file(o, path);
}

bool
xbps_object_externalize_to_file_with_format(xbps_object_t o, const char *path, xbps_prop_format_t fmt)
{
	return prop_object_externalize_to_file_with_format(o, path, (prop_format_t)fmt);
}

xbps_object_t
xbps_object_internalize(const char *s)
{
	return prop_object_internalize(s);
}

xbps_object_t
xbps_object_internalize_from_file(const char *s)
{
	return prop_object_internalize_from_file(s);
}

/* prop_string */

xbps_string_t
xbps_string_create_format(const char *fmt, ...)
{
	va_list ap;
	xbps_string_t s;

	va_start(ap, fmt);
	s = prop_string_create_vformat(fmt, ap);
	va_end(ap);

	return s;
}

xbps_string_t
xbps_string_create_copy(const char *v)
{
	return prop_string_create_copy(v);
}

xbps_string_t
xbps_string_create_nocopy(const char *v)
{
	return prop_string_create_nocopy(v);
}

xbps_string_t
xbps_string_copy(xbps_string_t v)
{
	return prop_string_copy(v);
}

bool
xbps_string_copy_value(xbps_string_t s, void *d, size_t l)
{
	return prop_string_copy_value(s, d, l);
}

size_t
xbps_string_size(xbps_string_t s)
{
	return prop_string_size(s);
}

const char *
xbps_string_value(xbps_string_t s)
{
	return prop_string_value(s);
}

bool
xbps_string_equals(xbps_string_t s, xbps_string_t ss)
{
	return prop_string_equals(s, ss);
}

bool
xbps_string_equals_string(xbps_string_t s, const char *v)
{
	return prop_string_equals_string(s, v);
}

int
xbps_string_compare(xbps_string_t s, xbps_string_t ss)
{
	return prop_string_compare(s, ss);
}

int
xbps_string_compare_string(xbps_string_t s, const char *ss)
{
	return prop_string_compare_string(s, ss);
}

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
xbps_string_copy_mutable(xbps_string_t s)
{
	return prop_string_copy_mutable(s);
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
xbps_string_equals_cstring(xbps_string_t s, const char *ss)
{
	return prop_string_equals_cstring(s, ss);
}

xbps_dictionary_t
xbps_plist_dictionary_from_file(const char *path)
{
	xbps_dictionary_t d;

	d = xbps_dictionary_internalize_from_file(path);
	if (xbps_object_type(d) != XBPS_TYPE_DICTIONARY) {
		xbps_dbg_printf(
		    "xbps: failed to internalize dict from %s\n", path);
	}
	return d;
}
