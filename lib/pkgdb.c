/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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
 * @file lib/pkgdb.c
 * @brief Package database handling routines
 * @defgroup pkgdb Package database handling functions
 *
 * Functions to manipulate the main package database plist file (pkgdb).
 *
 * The following image shown below shows the proplib structure used
 * by the main package database plist:
 *
 * @image html images/xbps_pkgdb_array.png
 *
 * Legend:
 *  - <b>Salmon filled box</b>: \a XBPS_PKGDB file internalized.
 *  - <b>White filled box</b>: mandatory objects.
 *  - <b>Grey filled box</b>: optional objects.
 *  - <b>Green filled box</b>: possible value set in the object, only one
 *    of them is set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e array, bool, integer, string,
 * dictionary.
 */
int HIDDEN
xbps_pkgdb_init(struct xbps_handle *xhp)
{
	int rv;

	assert(xhp != NULL);

	if (xhp->pkgdb != NULL)
		return 0;

	if ((rv = xbps_pkgdb_update(false)) != 0) {
		if (rv != ENOENT)
			xbps_dbg_printf("[pkgdb] cannot internalize "
			    "pkgdb array: %s\n", strerror(rv));

		return rv;
	}
	xbps_dbg_printf("[pkgdb] initialized ok.\n");

	return 0;
}

int
xbps_pkgdb_update(bool flush)
{
	struct xbps_handle *xhp = xbps_handle_get();
	char *plist, *metadir;
	int rv = 0;

	plist = xbps_xasprintf("%s/%s/%s", xhp->rootdir,
	    XBPS_META_PATH, XBPS_PKGDB);
	if (plist == NULL)
		return ENOMEM;

	if (xhp->pkgdb != NULL && flush) {
		metadir = xbps_xasprintf("%s/%s", xhp->rootdir,
		    XBPS_META_PATH);
		if (metadir == NULL) {
			free(plist);
			return ENOMEM;
		}
		/* Create metadir if doesn't exist */
		if (access(metadir, X_OK) == -1) {
			if (errno == ENOENT) {
				if (xbps_mkpath(metadir, 0755) != 0) {
					xbps_dbg_printf("[pkgdb] failed to "
					    "create metadir %s: %s\n", metadir,
					    strerror(errno));
					rv = errno;
					free(metadir);
					free(plist);
					return rv;
				}
			} else {
				free(plist);
				return errno;
			}
		}
		free(metadir);
		/* flush dictionary to storage */
		if (!prop_array_externalize_to_zfile(xhp->pkgdb, plist)) {
			free(plist);
			return errno;
		}
		prop_object_release(xhp->pkgdb);
		xhp->pkgdb = NULL;
	}
	/* update copy in memory */
	xhp->pkgdb = prop_array_internalize_from_zfile(plist);
	if (xhp->pkgdb == NULL)
		rv = errno;

	free(plist);

	return rv;
}

void HIDDEN
xbps_pkgdb_release(struct xbps_handle *xhp)
{
	assert(xhp != NULL);

	if (xhp->pkgdb == NULL)
		return;

	prop_object_release(xhp->pkgdb);
	xhp->pkgdb = NULL;
	xbps_dbg_printf("[pkgdb] released ok.\n");
}

static int
foreach_pkg_cb(int (*fn)(prop_object_t, void *, bool *),
	       void *arg,
	       bool reverse)
{
	struct xbps_handle *xhp = xbps_handle_get();
	int rv;

	if ((rv = xbps_pkgdb_init(xhp)) != 0)
		return rv;

	if (reverse)
		rv = xbps_callback_array_iter_reverse(xhp->pkgdb, fn, arg);
	else
		rv = xbps_callback_array_iter(xhp->pkgdb, fn, arg);

	return rv;
}

int
xbps_pkgdb_foreach_reverse_cb(int (*fn)(prop_object_t, void *, bool *),
			      void *arg)
{
	return foreach_pkg_cb(fn, arg, true);
}

int
xbps_pkgdb_foreach_cb(int (*fn)(prop_object_t, void *, bool *),
		      void *arg)
{
	return foreach_pkg_cb(fn, arg, false);
}

prop_dictionary_t
xbps_pkgdb_get_pkgd(const char *pkg, bool bypattern)
{
	struct xbps_handle *xhp = xbps_handle_get();
	prop_dictionary_t pkgd = NULL;

	if (xbps_pkgdb_init(xhp) != 0)
		return NULL;

	if (bypattern)
		pkgd = xbps_find_pkg_in_array_by_pattern(xhp->pkgdb, pkg);
	else
		pkgd = xbps_find_pkg_in_array_by_name(xhp->pkgdb, pkg);

	if (pkgd != NULL)
		return prop_dictionary_copy(pkgd);

	return NULL;
}

bool
xbps_pkgdb_remove_pkgd(const char *pkg, bool bypattern, bool flush)
{
	struct xbps_handle *xhp = xbps_handle_get();
	bool rv = false;

	if (xbps_pkgdb_init(xhp) != 0)
		return false;

	if (bypattern)
		rv = xbps_remove_pkg_from_array_by_pattern(xhp->pkgdb, pkg);
	else
		rv = xbps_remove_pkg_from_array_by_name(xhp->pkgdb, pkg);

	if (!flush || !rv)
		return rv;

	if ((xbps_pkgdb_update(true)) != 0)
		return false;

	return true;
}

bool
xbps_pkgdb_replace_pkgd(prop_dictionary_t pkgd,
			const char *pkg,
			bool bypattern,
			bool flush)
{
	struct xbps_handle *xhp = xbps_handle_get();
	int rv;

	if (xbps_pkgdb_init(xhp) != 0)
		return false;

	if (bypattern)
		rv = xbps_array_replace_dict_by_pattern(xhp->pkgdb, pkgd, pkg);
	else
		rv = xbps_array_replace_dict_by_name(xhp->pkgdb, pkgd, pkg);

	if (!flush)
		return rv != 0 ? false : true;

	if ((xbps_pkgdb_update(true)) != 0)
		return false;

	return true;
}
