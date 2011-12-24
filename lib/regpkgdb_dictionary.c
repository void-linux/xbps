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

int HIDDEN
xbps_regpkgdb_dictionary_init(struct xbps_handle *xhp)
{
	int rv;

	assert(xhp != NULL);

	if (xhp->regpkgdb != NULL)
		return 0;

	rv = xbps_regpkgdb_update(xhp, false);
	if (rv != 0) {
		if (rv != ENOENT)
			xbps_dbg_printf("[regpkgdb] cannot internalize "
			    "regpkgdb dictionary: %s\n", strerror(rv));

		return rv;
	}
	xbps_dbg_printf("[regpkgdb] initialized ok.\n");

	return 0;
}

int
xbps_regpkgdb_update(struct xbps_handle *xhp, bool flush)
{
	char *plist, *metadir;
	int rv = 0;

	plist = xbps_xasprintf("%s/%s/%s", xhp->rootdir,
	    XBPS_META_PATH, XBPS_REGPKGDB);
	if (plist == NULL)
		return ENOMEM;

	if (xhp->regpkgdb != NULL && flush) {
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
					xbps_dbg_printf("[regpkgdb] failed to "
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
		if (!prop_dictionary_externalize_to_zfile(xhp->regpkgdb,
		    plist)) {
			free(plist);
			return errno;
		}
		prop_object_release(xhp->regpkgdb);
		xhp->regpkgdb = NULL;
	}
	/* update copy in memory */
	xhp->regpkgdb = prop_dictionary_internalize_from_zfile(plist);
	if (xhp->regpkgdb == NULL)
		rv = errno;

	free(plist);

	return rv;
}

void HIDDEN
xbps_regpkgdb_dictionary_release(struct xbps_handle *xhp)
{
	assert(xhp != NULL);

	if (xhp->regpkgdb == NULL)
		return;

	prop_object_release(xhp->regpkgdb);
	xhp->regpkgdb = NULL;
	xbps_dbg_printf("[regpkgdb] released ok.\n");
}

static int
foreach_pkg_cb(int (*fn)(prop_object_t, void *, bool *),
	       void *arg,
	       bool reverse)
{
	struct xbps_handle *xhp = xbps_handle_get();
	int rv;

	/* initialize regpkgdb */
	if ((rv = xbps_regpkgdb_dictionary_init(xhp)) != 0)
		return rv;

	if (reverse) {
		rv = xbps_callback_array_iter_reverse_in_dict(
		    xhp->regpkgdb, "packages", fn, arg);
	} else {
		rv = xbps_callback_array_iter_in_dict(
		    xhp->regpkgdb, "packages", fn, arg);
	}
	return rv;
}

int
xbps_regpkgdb_foreach_reverse_pkg_cb(int (*fn)(prop_object_t, void *, bool *),
				     void *arg)
{
	return foreach_pkg_cb(fn, arg, true);
}

int
xbps_regpkgdb_foreach_pkg_cb(int (*fn)(prop_object_t, void *, bool *),
			     void *arg)
{
	return foreach_pkg_cb(fn, arg, false);
}

prop_dictionary_t
xbps_regpkgdb_get_pkgd(const char *pkg, bool bypattern)
{
	struct xbps_handle *xhp = xbps_handle_get();
	prop_dictionary_t pkgd = NULL;

	if (xbps_regpkgdb_dictionary_init(xhp) != 0)
		return NULL;

	if (bypattern)
		pkgd = xbps_find_pkg_in_dict_by_pattern(xhp->regpkgdb,
		    "packages", pkg);
	else
		pkgd = xbps_find_pkg_in_dict_by_name(xhp->regpkgdb,
		    "packages", pkg);

	return prop_dictionary_copy(pkgd);
}
