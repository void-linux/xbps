/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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

#include <xbps_api.h>

/**
 * @file lib/regpkgs_dictionary.c
 * @brief Installed packages database init/fini routines
 * @defgroup regpkgdb Installed packages database init/fini functions
 *
 * These functions will initialize and release (resources of)
 * the installed packages database.
 *
 * The returned dictionary by xbps_regpkgs_dictionary_init() (if initialized
 * successfully) will have the following structure:
 *
 * @image html images/xbps_regpkgdb_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: XBPS_REGPKGDB_PLIST file internalized.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *  - <b>Green bg box</b>: possible value set in the object, only one of them
 *    will be set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static prop_dictionary_t regpkgs_dict;
static size_t regpkgs_refcount;
static bool regpkgs_initialized;

prop_dictionary_t
xbps_regpkgs_dictionary_init(void)
{
	char *plist;

	if (regpkgs_initialized == false) {
		plist = xbps_xasprintf("%s/%s/%s", xbps_get_rootdir(),
		    XBPS_META_PATH, XBPS_REGPKGDB);
		if (plist == NULL)
			return NULL;

		regpkgs_dict = prop_dictionary_internalize_from_file(plist);
		if (regpkgs_dict == NULL) {
			free(plist);
			return NULL;
		}
		free(plist);
		regpkgs_initialized = true;
		DPRINTF(("%s: initialized ok.\n", __func__));
	}	
	regpkgs_refcount++;

	return regpkgs_dict;
}

void
xbps_regpkgs_dictionary_release(void)
{
	if (--regpkgs_refcount > 0)
		return;

	prop_object_release(regpkgs_dict);
	regpkgs_dict = NULL;
	regpkgs_initialized = false;
	DPRINTF(("%s: released ok.\n", __func__));
}
