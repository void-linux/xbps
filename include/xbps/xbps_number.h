/*	$NetBSD: prop_number.h,v 1.6 2008/04/28 20:22:51 martin Exp $	*/

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

#ifndef _XBPS_NUMBER_H_
#define	_XBPS_NUMBER_H_

#include <stdint.h>
#include <xbps/xbps_object.h>

typedef struct _prop_number *xbps_number_t;

#ifdef __cplusplus
extern "C" {
#endif

xbps_number_t	xbps_number_create_integer(int64_t);
xbps_number_t	xbps_number_create_unsigned_integer(uint64_t);

xbps_number_t	xbps_number_copy(xbps_number_t);

int		xbps_number_size(xbps_number_t);
bool		xbps_number_unsigned(xbps_number_t);

int64_t		xbps_number_integer_value(xbps_number_t);
uint64_t	xbps_number_unsigned_integer_value(xbps_number_t);

bool		xbps_number_equals(xbps_number_t, xbps_number_t);
bool		xbps_number_equals_integer(xbps_number_t, int64_t);
bool		xbps_number_equals_unsigned_integer(xbps_number_t, uint64_t);

#ifdef __cplusplus
}
#endif

#endif /* _XBPS_NUMBER_H_ */
