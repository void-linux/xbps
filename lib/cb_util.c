/*-
 * Copyright (c) 2011-2013 Juan Romero Pardines.
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
#include <stdarg.h>
#include <errno.h>

#include "xbps_api_impl.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

void HIDDEN
xbps_set_cb_fetch(struct xbps_handle *xhp,
		  off_t file_size,
		  off_t file_offset,
		  off_t file_dloaded,
		  const char *file_name,
		  bool cb_start,
		  bool cb_update,
		  bool cb_end)
{
	struct xbps_fetch_cb_data xfcd;

	if (xhp->fetch_cb == NULL)
		return;

	xfcd.xhp = xhp;
	xfcd.file_size = file_size;
	xfcd.file_offset = file_offset;
	xfcd.file_dloaded = file_dloaded;
	xfcd.file_name = file_name;
	xfcd.cb_start = cb_start;
	xfcd.cb_update = cb_update;
	xfcd.cb_end = cb_end;
	(*xhp->fetch_cb)(&xfcd, xhp->fetch_cb_data);
}

int HIDDEN
xbps_set_cb_state(struct xbps_handle *xhp,
		  xbps_state_t state,
		  int err,
		  const char *arg,
		  const char *fmt,
		  ...)
{
	struct xbps_state_cb_data xscd;
	char *buf = NULL;
	va_list va;
	int retval;

	if (xhp->state_cb == NULL)
		return 0;

	xscd.xhp = xhp;
	xscd.state = state;
	xscd.err = err;
	xscd.arg = arg;
	if (fmt != NULL) {
		va_start(va, fmt);
		retval = vasprintf(&buf, fmt, va);
		va_end(va);
		if (retval <= 0)
			xscd.desc = NULL;
		else
			xscd.desc = buf;
	}
	retval = (*xhp->state_cb)(&xscd, xhp->state_cb_data);
	if (buf != NULL)
		free(buf);

	return retval;
}
