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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/regpkgdb_dictionary.c
 * @brief Package register database routines
 * @defgroup regpkgdb Package register database functions
 *
 * These functions will initialize and release (resources of)
 * the registered packages database plist file (defined by XBPS_REGPKGDB).
 *
 * The returned dictionary by xbps_regpkgs_dictionary_init() uses
 * the structure as shown in the next graph:
 *
 * @image html images/xbps_regpkgdb_dictionary.png
 *
 * Legend:
 *  - <b>Salmon filled box</b>: \a XBPS_REGPKGDB_PLIST file internalized.
 *  - <b>White filled box</b>: mandatory objects.
 *  - <b>Grey filled box</b>: optional objects.
 *  - <b>Green filled box</b>: possible value set in the object, only one
 *    of them is set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e array, bool, integer, string,
 * dictionary.
 */

static bool regpkgdb_initialized;

int HIDDEN
xbps_regpkgdb_dictionary_init(struct xbps_handle *xhp)
{
	char *plist;

	if (regpkgdb_initialized)
		return 0;

	plist = xbps_xasprintf("%s/%s/%s",
	    prop_string_cstring_nocopy(xhp->rootdir),
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL)
		return ENOMEM;

	xhp->regpkgdb_dictionary =
	    prop_dictionary_internalize_from_zfile(plist);
	if (xhp->regpkgdb_dictionary == NULL) {
		free(plist);
		if (errno != ENOENT)
			xbps_dbg_printf("[regpkgdb] cannot internalize "
			    "regpkgdb dictionary: %s\n", strerror(errno));
		return errno;
	}
	free(plist);
	regpkgdb_initialized = true;
	xbps_dbg_printf("[regpkgdb] initialized ok.\n");

	return 0;
}

void HIDDEN
xbps_regpkgdb_dictionary_release(void)
{
	struct xbps_handle *xhp;

	if (!regpkgdb_initialized)
		return;

	xhp = xbps_handle_get();
	prop_object_release(xhp->regpkgdb_dictionary);
	regpkgdb_initialized = false;
	xbps_dbg_printf("[regpkgdb] released ok.\n");
}
