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

#define FETCH_ERROR(x) ((x == FETCH_UNAVAIL) || \
			(x == FETCH_NETWORK) || \
			(x == FETCH_ABORT) || \
			(x == FETCH_TIMEOUT) || \
			(x == FETCH_DOWN))
static int
sync_remote_repo(const char *plist, const char *repourl)
{
	/* if file is there, continue */
	if (access(plist, R_OK) == 0)
		return 0;

	/* file not found, fetch it */
	if (xbps_repository_sync_pkg_index(repourl) == -1) {
		if (FETCH_ERROR(fetchLastErrCode))
			return -1;
	}

	return 0;
}
#undef FETCH_ERROR

int HIDDEN
xbps_repository_pool_init(void)
{
	struct xbps_handle *xhp;
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	struct repository_pool *rpool;
	size_t ntotal = 0, nmissing = 0;
	const char *repouri;
	char *plist;
	int rv = 0;
	bool duprepo;

	xhp = xbps_handle_get();
	if (xhp->conf_dictionary == NULL)
		return ENOTSUP;

	if (repolist_initialized)
		return 0;

	array = prop_dictionary_get(xhp->conf_dictionary, "repositories");
	if (array == NULL)
		return errno;

	if (prop_array_count(array) == 0)
		return ENOTSUP;

	iter = prop_array_iterator(array);
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		/*
		 * Check that we do not register duplicate repositories.
		 */
		duprepo = false;
		repouri = prop_string_cstring_nocopy(obj);
		SIMPLEQ_FOREACH(rpool, &rpool_queue, rp_entries) {
			if (strcmp(rpool->rpi->rpi_uri, repouri) == 0) {
				duprepo = true;
				break;
			}
		}
		if (duprepo)
			continue;

		plist = xbps_pkg_index_plist(repouri);
		if (plist == NULL) {
			rv = errno;
			goto out;
		}
		ntotal++;
		if (sync_remote_repo(plist, repouri) == -1) {
			nmissing++;
			free(plist);
			continue;
		}
		/*
		 * Iterate over the repository pool and add the dictionary
		 * for current repository into the queue.
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
			free(plist);
			free(rpool);
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
			free(rpool->rpi->rpi_uri);
			free(rpool->rpi);
			free(rpool);
			free(plist);
			if (errno == ENOENT) {
				errno = 0;
				xbps_dbg_printf("[rpool] missing pkg-index.plist "
				    "for '%s' repository.\n", repouri);
				nmissing++;
				continue;
			}
			rv = errno;
			xbps_dbg_printf("[rpool] cannot internalize plist %s: %s\n",
			    plist, strerror(rv));
			goto out;
		}
		free(plist);
		xbps_dbg_printf("Registered repository '%s'\n",
		    rpool->rpi->rpi_uri);
		SIMPLEQ_INSERT_TAIL(&rpool_queue, rpool, rp_entries);
	}

	if (ntotal - nmissing == 0) {
		/* no repositories available, error out */
		rv = ENOTSUP;
		goto out;
	}

	repolist_initialized = true;
	xbps_dbg_printf("[rpool] initialized ok.\n");
out:
	if (iter)
		prop_object_iterator_release(iter);
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
		xbps_dbg_printf("Unregistering repository '%s'...",
		    rpool->rpi->rpi_uri);
		prop_object_release(rpool->rpi->rpi_repod);
		free(rpool->rpi->rpi_uri);
		free(rpool->rpi);
		free(rpool);
		rpool = NULL;
		xbps_dbg_printf_append("done\n");

	}
	repolist_initialized = false;
	xbps_dbg_printf("[rpool] released ok.\n");
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
