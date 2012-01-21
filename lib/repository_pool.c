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

/*
 * Returns true if repository URI contains "noarch" or matching architecture
 * in last component, false otherwise.
 */
static bool
check_repo_arch(const char *uri)
{
	struct utsname un;
	char *p, *b;

	if ((p = strdup(uri)) == NULL)
		return false;

	uname(&un);
	b = basename(p);
	if ((strcmp(b, "noarch")) && (strcmp(b, un.machine))) {
		free(p);
		return false;
	}
	free(p);
	return true;
}

int HIDDEN
xbps_repository_pool_init(struct xbps_handle *xhp)
{
	prop_dictionary_t d = NULL;
	prop_array_t array;
	size_t i, ntotal = 0, nmissing = 0;
	const char *repouri;
	char *plist;
	int rv = 0;

	if (prop_object_type(xhp->repo_pool) == PROP_TYPE_ARRAY)
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
		 * Check if repository doesn't match our architecture.
		 */
		if (!check_repo_arch(repouri)) {
			xbps_dbg_printf("[rpool] `%s' arch not matched, "
			    "ignoring.\n", repouri);
			nmissing++;
			continue;
		}
		/*
		 * If index file is not there, skip.
		 */
		plist = xbps_pkg_index_plist(repouri);
		if (plist == NULL) {
			rv = errno;
			goto out;
		}
		if (access(plist, R_OK) == -1) {
			xbps_dbg_printf("[rpool] `%s' missing index "
			    "file, ignoring.\n", repouri);
			free(plist);
			nmissing++;
			continue;
		}
		/*
		 * Register repository into the array.
		 */
		d = prop_dictionary_create();
		if (d == NULL) {
			rv = ENOMEM;
			free(plist);
			goto out;
		}
		if (!prop_dictionary_set_cstring_nocopy(d, "uri", repouri)) {
			rv = EINVAL;
			prop_object_release(d);
			free(plist);
			goto out;
		}
		array = prop_array_internalize_from_zfile(plist);
		if (array == NULL) {
			rv = EINVAL;
			prop_object_release(d);
			free(plist);
			goto out;
		}
		free(plist);
		prop_array_make_immutable(array);
		if (!xbps_add_obj_to_dict(d, array, "index")) {
			rv = EINVAL;
			prop_object_release(d);
			goto out;
		}
		if (!prop_array_add(xhp->repo_pool, d)) {
			rv = EINVAL;
			prop_object_release(d);
			goto out;
		}
		xbps_dbg_printf("[rpool] `%s' registered.\n", repouri);
	}
	if (ntotal - nmissing == 0) {
		/* no repositories available, error out */
		rv = ENOTSUP;
		goto out;
	}

	xbps_dbg_printf("[rpool] initialized ok.\n");
out:
	if (rv != 0) 
		xbps_repository_pool_release(xhp);

	return rv;

}

void HIDDEN
xbps_repository_pool_release(struct xbps_handle *xhp)
{
	prop_dictionary_t d;
	size_t i;
	const char *uri;

	if (xhp->repo_pool == NULL)
		return;

	for (i = 0; i < prop_array_count(xhp->repo_pool); i++) {
		d = prop_array_get(xhp->repo_pool, i);
		prop_dictionary_get_cstring_nocopy(d, "uri", &uri);
		xbps_dbg_printf("[rpool] unregistered repository '%s'\n", uri);
		prop_object_release(d);
	}
	prop_object_release(xhp->repo_pool);
	xhp->repo_pool = NULL;
	xbps_dbg_printf("[rpool] released ok.\n");
}

int
xbps_repository_pool_sync(const struct xbps_handle *xhp)
{
	const char *repouri;
	size_t i;
	int rv;

	if (xhp->cfg == NULL)
		return ENOTSUP;

	for (i = 0; i < cfg_size(xhp->cfg, "repositories"); i++) {
		repouri = cfg_getnstr(xhp->cfg, "repositories", i);
		/*
		 * Check if repository doesn't match our architecture.
		 */
		if (!check_repo_arch(repouri)) {
			xbps_dbg_printf("[rpool] `%s' arch not matched, "
			    "ignoring.\n", repouri);
			continue;
		}
		/*
		 * Fetch repository plist index.
		 */
		rv = xbps_repository_sync_pkg_index(repouri, XBPS_PKGINDEX);
		if (rv == -1) {
			xbps_dbg_printf("[rpool] `%s' failed to fetch: %s\n",
			    repouri, fetchLastErrCode == 0 ?
			    strerror(errno) : xbps_fetch_error_string());
			continue;
		}
		/*
		 * Fetch repository plist files index.
		 */
		rv = xbps_repository_sync_pkg_index(repouri,
		    XBPS_PKGINDEX_FILES);
		if (rv == -1) {
			xbps_dbg_printf("[rpool] `%s' failed to fetch: %s\n",
			    repouri, fetchLastErrCode == 0 ?
			    strerror(errno) : xbps_fetch_error_string());
			continue;
		}
	}
	return 0;
}

int
xbps_repository_pool_foreach(
		int (*fn)(struct repository_pool_index *, void *, bool *),
		void *arg)
{
	prop_dictionary_t d;
	struct xbps_handle *xhp = xbps_handle_get();
	struct repository_pool_index *rpi;
	size_t i;
	int rv = 0;
	bool done = false;

	assert(fn != NULL);
	/* Initialize repository pool */
	if ((rv = xbps_repository_pool_init(xhp)) != 0) {
		if (rv == ENOTSUP) {
			xbps_dbg_printf("[rpool] empty repository list.\n");
		} else if (rv != ENOENT && rv != ENOTSUP) {
			xbps_dbg_printf("[rpool] couldn't initialize: %s\n",
			    strerror(rv));
		}
		return rv;
	}
	/* Iterate over repository pool */
	for (i = 0; i < prop_array_count(xhp->repo_pool); i++) {
		rpi = malloc(sizeof(*rpi));
		if (rpi == NULL)
			return ENOMEM;

		d = prop_array_get(xhp->repo_pool, i);
		prop_dictionary_get_cstring_nocopy(d, "uri", &rpi->rpi_uri);
		rpi->rpi_repo = prop_dictionary_get(d, "index");
		rpi->rpi_index = i;

		rv = (*fn)(rpi, arg, &done);
		if (rv != 0 || done) {
			free(rpi);
			break;
		}
		free(rpi);
	}

	return rv;
}
