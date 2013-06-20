/*	$NetBSD: prop_object.h,v 1.7 2008/04/28 20:22:51 martin Exp $	*/

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

#ifndef _XBPS_OBJECT_H_
#define	_XBPS_OBJECT_H_

#include <stdint.h>
#include <stdbool.h>

typedef void *xbps_object_t;

typedef enum {
	XBPS_TYPE_UNKNOWN	=	0x00000000,
	XBPS_TYPE_BOOL		=	0x626f6f6c,	/* 'bool' */
	XBPS_TYPE_NUMBER	=	0x6e6d6272,	/* 'nmbr' */
	XBPS_TYPE_STRING	=	0x73746e67,	/* 'stng' */
	XBPS_TYPE_DATA		=	0x64617461,	/* 'data' */
	XBPS_TYPE_ARRAY		=	0x61726179,	/* 'aray' */
	XBPS_TYPE_DICTIONARY	=	0x64696374,	/* 'dict' */
	XBPS_TYPE_DICT_KEYSYM	=	0x646b6579	/* 'dkey' */
} xbps_type_t;

#ifdef  __cplusplus
extern "C" {
#endif

void		xbps_object_retain(xbps_object_t);
void		xbps_object_release(xbps_object_t);

xbps_type_t	xbps_object_type(xbps_object_t);

bool		xbps_object_equals(xbps_object_t, xbps_object_t);
bool		xbps_object_equals_with_error(xbps_object_t, xbps_object_t, bool *);

typedef struct _prop_object_iterator *xbps_object_iterator_t;

xbps_object_t	xbps_object_iterator_next(xbps_object_iterator_t);
void		xbps_object_iterator_reset(xbps_object_iterator_t);
void		xbps_object_iterator_release(xbps_object_iterator_t);

#ifdef __cplusplus
}
#endif

#endif /* _XBPS_OBJECT_H_ */
