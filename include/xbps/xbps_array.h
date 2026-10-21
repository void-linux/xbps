/*     $NetBSD: prop_array.h,v 1.8 2008/09/11 13:15:13 haad Exp $    */

/*-
 * Copyright (c) 2006 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _XBPS_ARRAY_H_
#define	_XBPS_ARRAY_H_

#include <sys/types.h>

#include <stdint.h>

#include <xbps/xbps_object.h>
#include <xbps/macro.h>

typedef struct _prop_array *xbps_array_t;

#ifdef __cplusplus
extern "C" {
#endif

xbps_array_t	xbps_array_create(void);
xbps_array_t	xbps_array_create_with_capacity(unsigned int);

xbps_array_t	xbps_array_copy(xbps_array_t);
xbps_array_t	xbps_array_copy_mutable(xbps_array_t);

unsigned int	xbps_array_capacity(xbps_array_t);
unsigned int	xbps_array_count(xbps_array_t);
bool		xbps_array_ensure_capacity(xbps_array_t, unsigned int);

void		xbps_array_make_immutable(xbps_array_t);
bool		xbps_array_mutable(xbps_array_t);

xbps_object_iterator_t xbps_array_iterator(xbps_array_t);

xbps_object_t	xbps_array_get(xbps_array_t, unsigned int);
bool		xbps_array_set(xbps_array_t, unsigned int, xbps_object_t);
bool		xbps_array_add(xbps_array_t, xbps_object_t);
bool		xbps_array_add_first(xbps_array_t, xbps_object_t);
void		xbps_array_remove(xbps_array_t, unsigned int);

bool		xbps_array_equals(xbps_array_t, xbps_array_t);

char *		xbps_array_externalize(xbps_array_t);
xbps_array_t	xbps_array_internalize(const char *);

bool		xbps_array_externalize_to_file(xbps_array_t, const char *);
xbps_array_t	xbps_array_internalize_from_file(const char *);

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool		xbps_array_get_bool(xbps_array_t, unsigned int,
					 bool *);
bool		xbps_array_set_bool(xbps_array_t, unsigned int,
					 bool);

bool		xbps_array_get_schar(xbps_array_t, unsigned int,
				     signed char *);
bool		xbps_array_get_uchar(xbps_array_t, unsigned int,
				     unsigned char *);
bool		xbps_array_set_schar(xbps_array_t, unsigned int,
				     signed char);
bool		xbps_array_set_uchar(xbps_array_t, unsigned int,
				     unsigned char);

bool		xbps_array_get_short(xbps_array_t, unsigned int,
				     short *);
bool		xbps_array_get_ushort(xbps_array_t, unsigned int,
				      unsigned short *);
bool		xbps_array_set_short(xbps_array_t, unsigned int,
				     short);
bool		xbps_array_set_ushort(xbps_array_t, unsigned int,
				      unsigned short);

bool		xbps_array_get_int(xbps_array_t, unsigned int,
				   int *);
bool		xbps_array_get_uint(xbps_array_t, unsigned int,
				    unsigned int *);
bool		xbps_array_set_int(xbps_array_t, unsigned int,
				   int);
bool		xbps_array_set_uint(xbps_array_t, unsigned int,
				    unsigned int);

bool		xbps_array_get_long(xbps_array_t, unsigned int,
				    long *);
bool		xbps_array_get_ulong(xbps_array_t, unsigned int,
				     unsigned long *);
bool		xbps_array_set_long(xbps_array_t, unsigned int,
				    long);
bool		xbps_array_set_ulong(xbps_array_t, unsigned int,
				     unsigned long);

bool		xbps_array_get_longlong(xbps_array_t, unsigned int,
					long long *);
bool		xbps_array_get_ulonglong(xbps_array_t, unsigned int,
					 unsigned long long *);
bool		xbps_array_set_longlong(xbps_array_t, unsigned int,
					long long);
bool		xbps_array_set_ulonglong(xbps_array_t, unsigned int,
					 unsigned long long );

bool		xbps_array_get_intptr(xbps_array_t, unsigned int,
				      intptr_t *);
bool		xbps_array_get_uintptr(xbps_array_t, unsigned int,
				       uintptr_t *);
bool		xbps_array_set_intptr(xbps_array_t, unsigned int,
				      intptr_t);
bool		xbps_array_set_uintptr(xbps_array_t, unsigned int,
				       uintptr_t);

bool		xbps_array_get_int8(xbps_array_t, unsigned int,
					 int8_t *);
bool		xbps_array_get_uint8(xbps_array_t, unsigned int,
					  uint8_t *);
bool		xbps_array_set_int8(xbps_array_t, unsigned int,
					 int8_t);
bool		xbps_array_set_uint8(xbps_array_t, unsigned int,
					  uint8_t);

bool		xbps_array_get_int16(xbps_array_t, unsigned int,
					  int16_t *);
bool		xbps_array_get_uint16(xbps_array_t, unsigned int,
					   uint16_t *);
bool		xbps_array_set_int16(xbps_array_t, unsigned int,
					  int16_t);
bool		xbps_array_set_uint16(xbps_array_t, unsigned int,
					   uint16_t);

bool		xbps_array_get_int32(xbps_array_t, unsigned int,
					  int32_t *);
bool		xbps_array_get_uint32(xbps_array_t, unsigned int,
					   uint32_t *);
bool		xbps_array_set_int32(xbps_array_t, unsigned int,
					  int32_t);
bool		xbps_array_set_uint32(xbps_array_t, unsigned int,
					   uint32_t);

bool		xbps_array_get_int64(xbps_array_t, unsigned int,
					  int64_t *);
bool		xbps_array_get_uint64(xbps_array_t, unsigned int,
					   uint64_t *);
bool		xbps_array_set_int64(xbps_array_t, unsigned int,
					  int64_t);
bool		xbps_array_set_uint64(xbps_array_t, unsigned int,
					   uint64_t);

bool		xbps_array_set_and_rel(xbps_array_t, unsigned int,
				       xbps_object_t);

bool		xbps_array_add_bool(xbps_array_t, bool);

bool		xbps_array_add_schar(xbps_array_t, signed char);
bool		xbps_array_add_uchar(xbps_array_t, unsigned char);

bool		xbps_array_add_short(xbps_array_t, short);
bool		xbps_array_add_ushort(xbps_array_t, unsigned short);

bool		xbps_array_add_int(xbps_array_t, int);
bool		xbps_array_add_uint(xbps_array_t, unsigned int);

bool		xbps_array_add_long(xbps_array_t, long);
bool		xbps_array_add_ulong(xbps_array_t, unsigned long);

bool		xbps_array_add_longlong(xbps_array_t, long long);
bool		xbps_array_add_ulonglong(xbps_array_t, unsigned long long);

bool		xbps_array_add_intptr(xbps_array_t, intptr_t);
bool		xbps_array_add_uintptr(xbps_array_t, uintptr_t);

bool		xbps_array_add_int8(xbps_array_t, int8_t);
bool		xbps_array_add_uint8(xbps_array_t, uint8_t);

bool		xbps_array_add_int16(xbps_array_t, int16_t);
bool		xbps_array_add_uint16(xbps_array_t, uint16_t);

bool		xbps_array_add_int32(xbps_array_t, int32_t);
bool		xbps_array_add_uint32(xbps_array_t, uint32_t);

bool		xbps_array_add_int64(xbps_array_t, int64_t);
bool		xbps_array_add_uint64(xbps_array_t, uint64_t);

bool		xbps_array_get_string(xbps_array_t, unsigned int,
						const char **);
bool		xbps_array_set_string(xbps_array_t, unsigned int,
						const char *);
bool		xbps_array_add_string(xbps_array_t, const char *);
bool		xbps_array_set_string_nocopy(xbps_array_t, unsigned int,
						const char *);
bool		xbps_array_add_string_nocopy(xbps_array_t, const char *);

bool		xbps_array_get_data(xbps_array_t, unsigned int,
					const void **, size_t *);
bool		xbps_array_set_data(xbps_array_t, unsigned int,
					const void *, size_t);
bool		xbps_array_add_data(xbps_array_t,
					const void *, size_t);
bool		xbps_array_set_data_nocopy(xbps_array_t, unsigned int,
					const void *, size_t);
bool		xbps_array_add_data_nocopy(xbps_array_t,
					const void *, size_t);

bool		xbps_array_add_and_rel(xbps_array_t, xbps_object_t);

/* Deprecated functions. */
bool		xbps_array_add_cstring(xbps_array_t, const char *);
bool		xbps_array_get_cstring(xbps_array_t, unsigned int,
				       char **) DEPRECATED;
bool		xbps_array_set_cstring(xbps_array_t, unsigned int,
				       const char *) DEPRECATED;

bool		xbps_array_add_cstring_nocopy(xbps_array_t,
					      const char *) DEPRECATED;
bool		xbps_array_get_cstring_nocopy(xbps_array_t, unsigned int,
					      const char **) DEPRECATED;
bool		xbps_array_set_cstring_nocopy(xbps_array_t, unsigned int,
					      const char *) DEPRECATED;

#ifdef __cplusplus
}
#endif

#endif /* _XBPS_ARRAY_H_ */
