/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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
#include <stdlib.h>
#include <errno.h>

#include "defs.h"

void
unpack_progress_cb_verbose(struct xbps_handle *xhp,
			   struct xbps_unpack_cb_data *xpd,
			   void *cbdata)
{
	(void)xhp;
	(void)cbdata;

	if (xpd->entry == NULL || xpd->entry_total_count <= 0)
		return;

	printf("%s: unpacked %sfile `%s' (%" PRIi64 " bytes)\n",
	    xpd->pkgver,
	    xpd->entry_is_conf ? "configuration " : "", xpd->entry,
	    xpd->entry_size);
}

void
unpack_progress_cb(struct xbps_handle *xhp,
		   struct xbps_unpack_cb_data *xpd,
		   void *cbdata)
{
	(void)xhp;
	(void)cbdata;

	if (xpd->entry_total_count <= 0)
		return;

	printf("%s: unpacked %zd of %zd files...\n",
	    xpd->pkgver, xpd->entry_extract_count, xpd->entry_total_count);
	printf("\033[1A\033[K");
}
