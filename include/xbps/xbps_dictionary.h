/*	$NetBSD: prop_dictionary.h,v 1.9 2008/04/28 20:22:51 martin Exp $	*/

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

#ifndef _XBPS_DICTIONARY_H_
#define	_XBPS_DICTIONARY_H_

#include <stdint.h>
#include <xbps/xbps_object.h>
#include <xbps/xbps_array.h>

typedef struct _prop_dictionary *xbps_dictionary_t;
typedef struct _prop_dictionary_keysym *xbps_dictionary_keysym_t;

#ifdef __cplusplus
extern "C" {
#endif

xbps_dictionary_t xbps_dictionary_create(void);
xbps_dictionary_t xbps_dictionary_create_with_capacity(unsigned int);

xbps_dictionary_t xbps_dictionary_copy(xbps_dictionary_t);
xbps_dictionary_t xbps_dictionary_copy_mutable(xbps_dictionary_t);

unsigned int	xbps_dictionary_count(xbps_dictionary_t);
bool		xbps_dictionary_ensure_capacity(xbps_dictionary_t,
						unsigned int);

void		xbps_dictionary_make_immutable(xbps_dictionary_t);

xbps_object_iterator_t xbps_dictionary_iterator(xbps_dictionary_t);
xbps_array_t	xbps_dictionary_all_keys(xbps_dictionary_t);

xbps_object_t	xbps_dictionary_get(xbps_dictionary_t, const char *);
bool		xbps_dictionary_set(xbps_dictionary_t, const char *,
				    xbps_object_t);
void		xbps_dictionary_remove(xbps_dictionary_t, const char *);

xbps_object_t	xbps_dictionary_get_keysym(xbps_dictionary_t,
					   xbps_dictionary_keysym_t);
bool		xbps_dictionary_set_keysym(xbps_dictionary_t,
					   xbps_dictionary_keysym_t,
					   xbps_object_t);
void		xbps_dictionary_remove_keysym(xbps_dictionary_t,
					      xbps_dictionary_keysym_t);

bool		xbps_dictionary_equals(xbps_dictionary_t, xbps_dictionary_t);

char *		xbps_dictionary_externalize(xbps_dictionary_t);
xbps_dictionary_t xbps_dictionary_internalize(const char *);

bool		xbps_dictionary_externalize_to_file(xbps_dictionary_t,
						    const char *);
bool		xbps_dictionary_externalize_to_zfile(xbps_dictionary_t,
						     const char *);
xbps_dictionary_t xbps_dictionary_internalize_from_file(const char *);
xbps_dictionary_t xbps_dictionary_internalize_from_zfile(const char *);

const char *	xbps_dictionary_keysym_cstring_nocopy(xbps_dictionary_keysym_t);

bool		xbps_dictionary_keysym_equals(xbps_dictionary_keysym_t,
					      xbps_dictionary_keysym_t);

/*
 * Utility routines to make it more convenient to work with values
 * stored in dictionaries.
 */
bool		xbps_dictionary_get_dict(xbps_dictionary_t, const char *,
					 xbps_dictionary_t *);
bool		xbps_dictionary_get_bool(xbps_dictionary_t, const char *,
					 bool *);
bool		xbps_dictionary_set_bool(xbps_dictionary_t, const char *,
					 bool);

bool		xbps_dictionary_get_int8(xbps_dictionary_t, const char *,
					 int8_t *);
bool		xbps_dictionary_get_uint8(xbps_dictionary_t, const char *,
					  uint8_t *);
bool		xbps_dictionary_set_int8(xbps_dictionary_t, const char *,
					 int8_t);
bool		xbps_dictionary_set_uint8(xbps_dictionary_t, const char *,
					  uint8_t);

bool		xbps_dictionary_get_int16(xbps_dictionary_t, const char *,
					  int16_t *);
bool		xbps_dictionary_get_uint16(xbps_dictionary_t, const char *,
					   uint16_t *);
bool		xbps_dictionary_set_int16(xbps_dictionary_t, const char *,
					  int16_t);
bool		xbps_dictionary_set_uint16(xbps_dictionary_t, const char *,
					   uint16_t);

bool		xbps_dictionary_get_int32(xbps_dictionary_t, const char *,
					  int32_t *);
bool		xbps_dictionary_get_uint32(xbps_dictionary_t, const char *,
					   uint32_t *);
bool		xbps_dictionary_set_int32(xbps_dictionary_t, const char *,
					  int32_t);
bool		xbps_dictionary_set_uint32(xbps_dictionary_t, const char *,
					   uint32_t);

bool		xbps_dictionary_get_int64(xbps_dictionary_t, const char *,
					  int64_t *);
bool		xbps_dictionary_get_uint64(xbps_dictionary_t, const char *,
					   uint64_t *);
bool		xbps_dictionary_set_int64(xbps_dictionary_t, const char *,
					  int64_t);
bool		xbps_dictionary_set_uint64(xbps_dictionary_t, const char *,
					   uint64_t);

bool		xbps_dictionary_get_cstring(xbps_dictionary_t, const char *,
					     char **);
bool		xbps_dictionary_set_cstring(xbps_dictionary_t, const char *,
					    const char *);

bool		xbps_dictionary_get_cstring_nocopy(xbps_dictionary_t,
						   const char *,
						   const char **);
bool		xbps_dictionary_set_cstring_nocopy(xbps_dictionary_t,
						   const char *,
						   const char *);
bool		xbps_dictionary_set_and_rel(xbps_dictionary_t,
					    const char *,
					    xbps_object_t);

#ifdef __cplusplus
}
#endif

#endif /* _XBPS_DICTIONARY_H_ */
