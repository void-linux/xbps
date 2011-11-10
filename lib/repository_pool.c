/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/repository_pool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

struct repository_pool {
	SIMPLEQ_ENTRY(repository_pool) rp_entries;
	struct repository_pool_index *rpi;
};

static SIMPLEQ_HEAD(rpool_head, repository_pool) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

static bool repolist_initialized;

/*
 * Returns true if repository URI contains "noarch" or matching architecture
 * in last component, false otherwise.
 */
static bool
check_repo_arch(const char *uri)
{
	struct utsname un;
	char *p;

	uname(&un);
	p = strrchr(uri, '/');
	if (p == NULL)
		return false;
	p++;
	if (*p == '\0')
		return false;
	else if (strcmp(p, "noarch") == 0)
		return true;
	else if (strcmp(p, un.machine) == 0)
		return true;

	return false;
}

int HIDDEN
xbps_repository_pool_init(void)
{
	prop_string_t obj;
	struct xbps_handle *xhp;
	struct repository_pool *rpool;
	size_t i, ntotal = 0, nmissing = 0;
	const char *repouri;
	char *plist;
	int rv = 0;
	bool duprepo;

	if (repolist_initialized)
		return 0;

	xhp = xbps_handle_get();
	if (prop_object_type(xhp->repos_array) != PROP_TYPE_ARRAY)
		return ENOTSUP;

	if (prop_array_count(xhp->repos_array) == 0)
		return ENOTSUP;

	for (i = 0; i < prop_array_count(xhp->repos_array); i++) {
		obj = prop_array_get(xhp->repos_array, i);
		repouri = prop_string_cstring_nocopy(obj);
		/*
		 * Check that we do not register duplicate repositories.
		 */
		duprepo = false;
		SIMPLEQ_FOREACH(rpool, &rpool_queue, rp_entries) {
			if (strcmp(rpool->rpi->rpi_uri, repouri) == 0) {
				duprepo = true;
				break;
			}
		}
		if (duprepo)
			continue;

		ntotal++;
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
		 * Check if repository doesn't match our architecture.
		 */
		if (!check_repo_arch(repouri)) {
			xbps_dbg_printf("[rpool] `%s' arch not matched, "
			    "ignoring.\n", repouri);
			free(plist);
			nmissing++;
			continue;
		}
		/*
		 * Internalize repository's index dictionary and add it
		 * into the queue.
		 */
		rpool = malloc(sizeof(struct repository_pool));
		if (rpool == NULL) {
			rv = errno;
			free(plist);
			goto out;
		}

		rpool->rpi = malloc(sizeof(struct repository_pool_index));
		if (rpool->rpi == NULL) {
			rv = errno;
			free(rpool);
			free(plist);
			goto out;
		}

		rpool->rpi->rpi_uri = prop_string_cstring(obj);
		if (rpool->rpi->rpi_uri == NULL) {
			rv = errno;
			free(rpool->rpi);
			free(rpool);
			free(plist);
			goto out;
		}
		rpool->rpi->rpi_repod =
		    prop_dictionary_internalize_from_zfile(plist);
		if (rpool->rpi->rpi_repod == NULL) {
			rv = errno;
			free(rpool->rpi->rpi_uri);
			free(rpool->rpi);
			free(rpool);
			free(plist);
			xbps_dbg_printf("[rpool] cannot internalize plist %s: %s\n",
			    plist, strerror(rv));
			goto out;
		}
		free(plist);
		xbps_dbg_printf("[rpool] `%s' registered.\n", repouri);
		SIMPLEQ_INSERT_TAIL(&rpool_queue, rpool, rp_entries);
	}

	if (ntotal - nmissing == 0) {
		/* no repositories available, error out */
		rv = ENOTSUP;
		goto out;
	}

	repolist_initialized = true;
	prop_object_release(xhp->repos_array);
	xbps_dbg_printf("[rpool] initialized ok.\n");
out:
	if (rv != 0) 
		xbps_repository_pool_release();

	return rv;

}

void HIDDEN
xbps_repository_pool_release(void)
{
	struct repository_pool *rpool;

	if (!repolist_initialized)
		return;

	while ((rpool = SIMPLEQ_FIRST(&rpool_queue)) != NULL) {
		SIMPLEQ_REMOVE(&rpool_queue, rpool, repository_pool, rp_entries);
		xbps_dbg_printf("[rpool] unregistered repository '%s'\n",
		    rpool->rpi->rpi_uri);
		prop_object_release(rpool->rpi->rpi_repod);
		free(rpool->rpi->rpi_uri);
		free(rpool->rpi);
		free(rpool);
		rpool = NULL;
	}
	repolist_initialized = false;
	xbps_dbg_printf("[rpool] released ok.\n");
}

int
xbps_repository_pool_sync(void)
{
	const struct xbps_handle *xhp;
	const char *repouri;
	size_t i;
	int rv;

	xhp = xbps_handle_get();
	if (xhp->repos_array == NULL)
		return ENOTSUP;

	if (prop_array_count(xhp->repos_array) == 0)
		return ENOTSUP;

	for (i = 0; i < prop_array_count(xhp->repos_array); i++) {
		prop_array_get_cstring_nocopy(xhp->repos_array, i, &repouri);
		/*
		 * Check if repository doesn't match our architecture.
		 */
		if (!check_repo_arch(repouri)) {
			xbps_dbg_printf("[rpool] `%s' arch not matched, "
			    "ignoring.\n", repouri);
			continue;
		}
		/*
		 * Fetch repository index file.
		 */
		rv = xbps_repository_sync_pkg_index(repouri);
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
	struct repository_pool *rpool, *rpool_new;
	int rv = 0;
	bool done = false;

	assert(fn != NULL);
	/*
	 * Initialize repository pool.
	 */
	if ((rv = xbps_repository_pool_init()) != 0) {
		if (rv == ENOTSUP) {
			xbps_dbg_printf("[rpool] empty repository list.\n");
		} else if (rv != ENOENT && rv != ENOTSUP) {
			xbps_dbg_printf("[rpool] couldn't initialize: %s\n",
			    strerror(rv));
		}
		return rv;
	}

	SIMPLEQ_FOREACH_SAFE(rpool, &rpool_queue, rp_entries, rpool_new) {
		rv = (*fn)(rpool->rpi, arg, &done);
		if (rv != 0 || done)
			break;
	}

	return rv;
}
