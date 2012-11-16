/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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

#include <sys/utsname.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/repository_pool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

int HIDDEN
xbps_rpool_init(struct xbps_handle *xhp)
{
	prop_dictionary_t d = NULL;
	prop_array_t array;
	size_t i, ntotal = 0, nmissing = 0;
	const char *repouri;
	char *plist;
	int rv = 0;

	if (xhp->repo_pool != NULL)
		return 0;
	else if (xhp->cfg == NULL)
		return ENOTSUP;

	xhp->repo_pool = prop_array_create();
	if (xhp->repo_pool == NULL)
		return ENOMEM;

	for (i = 0; i < cfg_size(xhp->cfg, "repositories"); i++) {
		repouri = cfg_getnstr(xhp->cfg, "repositories", i);
		ntotal++;
		/*
		 * If index file is not there, skip.
		 */
		plist = xbps_pkg_index_plist(xhp, repouri);
		if (plist == NULL) {
			rv = errno;
			goto out;
		}
		if (access(plist, R_OK) == -1) {
			xbps_dbg_printf(xhp, "[rpool] `%s' cannot be "
			    "internalized: %s\n", repouri, strerror(errno));
			nmissing++;
			continue;
		}
		array = prop_array_internalize_from_zfile(plist);
		free(plist);
		assert(array);
		/*
		 * Register repository into the array.
		 */
		if ((d = prop_dictionary_create()) == NULL) {
			rv = ENOMEM;
			prop_object_release(array);
			goto out;
		}
		if (!prop_dictionary_set_cstring_nocopy(d, "uri", repouri)) {
			rv = EINVAL;
			prop_object_release(array);
			prop_object_release(d);
			goto out;
		}
		if (!prop_dictionary_set(d, "index", array)) {
			rv = EINVAL;
			prop_object_release(array);
			prop_object_release(d);
			goto out;
		}
		prop_object_release(array);
		if (!prop_array_add(xhp->repo_pool, d)) {
			rv = EINVAL;
			prop_object_release(d);
			goto out;
		}
		xbps_dbg_printf(xhp, "[rpool] `%s' registered.\n", repouri);
	}
	if (ntotal - nmissing == 0) {
		/* no repositories available, error out */
		rv = ENOTSUP;
		goto out;
	}

	prop_array_make_immutable(xhp->repo_pool);
	xbps_dbg_printf(xhp, "[rpool] initialized ok.\n");
out:
	if (rv != 0) 
		xbps_rpool_release(xhp);

	return rv;

}

void HIDDEN
xbps_rpool_release(struct xbps_handle *xhp)
{
	prop_dictionary_t d;
	size_t i;
	const char *uri;

	if (xhp->repo_pool == NULL)
		return;

	for (i = 0; i < prop_array_count(xhp->repo_pool); i++) {
		d = prop_array_get(xhp->repo_pool, i);
		if (xhp->flags & XBPS_FLAG_DEBUG) {
			prop_dictionary_get_cstring_nocopy(d, "uri", &uri);
			xbps_dbg_printf(xhp, "[rpool] unregistered "
			    "repository '%s'\n", uri);
		}
		prop_object_release(d);
	}
	prop_object_release(xhp->repo_pool);
	xhp->repo_pool = NULL;
	xbps_dbg_printf(xhp, "[rpool] released ok.\n");
}

int
xbps_rpool_sync(struct xbps_handle *xhp, const char *file, const char *uri)
{
	const char *repouri;
	size_t i;

	if (xhp->cfg == NULL)
		return ENOTSUP;

	for (i = 0; i < cfg_size(xhp->cfg, "repositories"); i++) {
		repouri = cfg_getnstr(xhp->cfg, "repositories", i);
		/* If argument was set just process that repository */
		if (uri && strcmp(repouri, uri))
			continue;

		if (xbps_repository_sync_pkg_index(xhp, repouri, file) == -1) {
			xbps_dbg_printf(xhp,
			    "[rpool] `%s' failed to fetch `%s': %s\n",
			    repouri, file,
			    fetchLastErrCode == 0 ? strerror(errno) :
			    xbps_fetch_error_string());
			continue;
		}
	}
	return 0;
}

int
xbps_rpool_foreach(struct xbps_handle *xhp,
		   int (*fn)(struct xbps_handle *, struct xbps_rpool_index *, void *, bool *),
		   void *arg)
{
	prop_dictionary_t d;
	struct xbps_rpool_index rpi;
	size_t i;
	int rv = 0;
	bool done = false;

	assert(fn != NULL);
	/* Initialize repository pool */
	if ((rv = xbps_rpool_init(xhp)) != 0) {
		if (rv == ENOTSUP) {
			xbps_dbg_printf(xhp,
			    "[rpool] empty repository list.\n");
		} else if (rv != ENOENT && rv != ENOTSUP) {
			xbps_dbg_printf(xhp,
			    "[rpool] couldn't initialize: %s\n",
			    strerror(rv));
		}
		return rv;
	}
	/* Iterate over repository pool */
	for (i = 0; i < prop_array_count(xhp->repo_pool); i++) {
		d = prop_array_get(xhp->repo_pool, i);
		prop_dictionary_get_cstring_nocopy(d, "uri", &rpi.uri);
		rpi.repo = prop_dictionary_get(d, "index");
		rv = (*fn)(xhp, &rpi, arg, &done);
		if (rv != 0 || done)
			break;
	}

	return rv;
}
