/*-
 * Copyright (c) 2009-2010 Juan Romero Pardines.
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
#include "xbps_api_impl.h"
#include "queue.h"

/**
 * @file lib/repository_pool.c
 * @brief Repository pool init/fini routines
 * @defgroup repopool Repository pool init/fini functions
 */

struct repository_pool {
	SIMPLEQ_ENTRY(repository_pool) rp_entries;
	struct repository_pool_index *rpi;
};

static SIMPLEQ_HEAD(rpool_head, repository_pool) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

static size_t repolist_refcnt;
static bool repolist_initialized;

int
xbps_repository_pool_init(void)
{
	prop_dictionary_t dict = NULL;
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	struct repository_pool *rpool;
	size_t ntotal = 0, nmissing = 0;
	char *plist;
	int rv = 0;

	if (repolist_initialized) {
		repolist_refcnt++;
		return 0;
	}

	plist = xbps_xasprintf("%s/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, XBPS_REPOLIST);
	if (plist == NULL) {
		rv = errno;
		goto out;
	}

	dict = prop_dictionary_internalize_from_zfile(plist);
	if (dict == NULL) {
		rv = errno;
                free(plist);
		xbps_dbg_printf("%s: cannot internalize plist %s: %s\n",
		    __func__, plist, strerror(errno));
		goto out;
	}
	free(plist);

	array = prop_dictionary_get(dict, "repository-list");
	if (array == NULL) {
		rv = errno;
		goto out;
	}

	iter = prop_array_iterator(array);
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		ntotal++;
		/*
		 * Iterate over the repository pool and add the dictionary
		 * for current repository into the queue.
		 */
		plist =
		    xbps_get_pkg_index_plist(prop_string_cstring_nocopy(obj));
		if (plist == NULL) {
			rv = errno;
			goto out;
		}

		rpool = malloc(sizeof(struct repository_pool));
		if (rpool == NULL) {
			rv = errno;
			goto out;
		}

		rpool->rpi = malloc(sizeof(struct repository_pool_index));
		if (rpool->rpi == NULL) {
			rv = errno;
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
			rv = errno;
			if (errno == ENOENT) {
				nmissing++;
				continue;
			}
			xbps_dbg_printf("%s: cannot internalize plist %s: %s\n",
			    __func__, plist, strerror(errno));
			goto out;
		}
		free(plist);
		xbps_dbg_printf("Registered repository '%s'\n",
		    rpool->rpi->rpi_uri);
		SIMPLEQ_INSERT_TAIL(&rpool_queue, rpool, rp_entries);
	}

	if (ntotal - nmissing == 0)
		goto out;

	repolist_initialized = true;
	repolist_refcnt = 1;
	xbps_dbg_printf("%s: initialized ok.\n", __func__);
out:
	if (iter)
		prop_object_iterator_release(iter);
	if (dict)
		prop_object_release(dict);
	if (rv != 0) 
		xbps_repository_pool_release();

	return rv;

}

void
xbps_repository_pool_release(void)
{
	struct repository_pool *rpool;

	if (--repolist_refcnt > 0)
		return;

	while ((rpool = SIMPLEQ_FIRST(&rpool_queue)) != NULL) {
		SIMPLEQ_REMOVE(&rpool_queue, rpool, repository_pool, rp_entries);
		xbps_dbg_printf("Unregistered repository '%s'\n",
		    rpool->rpi->rpi_uri);
		prop_object_release(rpool->rpi->rpi_repod);
		free(rpool->rpi->rpi_uri);
		free(rpool->rpi);
		free(rpool);
	}
	repolist_refcnt = 0;
	repolist_initialized = false;
	xbps_dbg_printf("%s: released ok.\n", __func__);
}

int
xbps_repository_pool_foreach(
		int (*fn)(struct repository_pool_index *, void *, bool *),
		void *arg)
{
	struct repository_pool *rpool;
	int rv = 0;
	bool done = false;

	if (!repolist_initialized)
		return 0;

	SIMPLEQ_FOREACH(rpool, &rpool_queue, rp_entries) {
		rv = (*fn)(rpool->rpi, arg, &done);
		if (rv != 0 || done)
			break;
	}

	return rv;
}
