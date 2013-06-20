/*	$NetBSD: prop_string.h,v 1.3 2008/04/28 20:22:51 martin Exp $	*/

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

#ifndef _XBPS_STRING_H_
#define	_XBPS_STRING_H_

#include <stdint.h>
#include <sys/types.h>
#include <xbps/xbps_object.h>

typedef struct _prop_string *xbps_string_t;

#ifdef __cplusplus
extern "C" {
#endif

xbps_string_t	xbps_string_create(void);
xbps_string_t	xbps_string_create_cstring(const char *);
xbps_string_t	xbps_string_create_cstring_nocopy(const char *);

xbps_string_t	xbps_string_copy(xbps_string_t);
xbps_string_t	xbps_string_copy_mutable(xbps_string_t);

size_t		xbps_string_size(xbps_string_t);
bool		xbps_string_mutable(xbps_string_t);

char *		xbps_string_cstring(xbps_string_t);
const char *	xbps_string_cstring_nocopy(xbps_string_t);

bool		xbps_string_append(xbps_string_t, xbps_string_t);
bool		xbps_string_append_cstring(xbps_string_t, const char *);

bool		xbps_string_equals(xbps_string_t, xbps_string_t);
bool		xbps_string_equals_cstring(xbps_string_t, const char *);

#ifdef __cplusplus
}
#endif

#endif /* _XBPS_STRING_H_ */
