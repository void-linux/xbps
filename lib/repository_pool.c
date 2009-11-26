/*-
 * Copyright (c) 2009 Juan Romero Pardines.
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

static size_t repolist_refcnt;
static bool repolist_initialized;

int SYMEXPORT
xbps_repository_pool_init(void)
{
	prop_dictionary_t dict = NULL;
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	struct repository_data *rdata;
	size_t ntotal = 0, nmissing = 0;
	char *plist;
	int rv = 0;

	if (repolist_initialized) {
		repolist_refcnt++;
		return 0;
	}

	SIMPLEQ_INIT(&repodata_queue);

	plist = xbps_xasprintf("%s/%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH, XBPS_REPOLIST);
	if (plist == NULL) {
		rv = EINVAL;
		goto out;
	}

	dict = prop_dictionary_internalize_from_file(plist);
	if (dict == NULL) {
                free(plist);
		rv = errno;
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
			rv = EINVAL;
			goto out;
		}

		rdata = malloc(sizeof(struct repository_data));
		if (rdata == NULL) {
			rv = errno;
			goto out;
		}

		rdata->rd_uri = prop_string_cstring(obj);
		if (rdata->rd_uri == NULL) {
			free(plist);
			rv = errno;
			goto out;
		}
		rdata->rd_repod = prop_dictionary_internalize_from_file(plist);
		if (rdata->rd_repod == NULL) {
			free(plist);
			if (errno == ENOENT) {
				free(rdata->rd_uri);
				free(rdata);
				errno = 0;
				nmissing++;
				continue;
			}
			rv = errno;
			goto out;
		}
		free(plist);
		SIMPLEQ_INSERT_TAIL(&repodata_queue, rdata, chain);
	}

	if (ntotal - nmissing == 0) {
		rv = EINVAL;
		goto out;
	}

	repolist_initialized = true;
	repolist_refcnt = 1;
	DPRINTF(("%s: initialized ok.\n", __func__));
out:
	if (iter)
		prop_object_iterator_release(iter);
	if (dict)
		prop_object_release(dict);
	if (rv != 0)
		xbps_repository_pool_release();

	return rv;

}

void SYMEXPORT
xbps_repository_pool_release(void)
{
	struct repository_data *rdata;

	if (--repolist_refcnt > 0)
		return;

	while ((rdata = SIMPLEQ_FIRST(&repodata_queue)) != NULL) {
		SIMPLEQ_REMOVE(&repodata_queue, rdata, repository_data, chain);
		prop_object_release(rdata->rd_repod);
		free(rdata->rd_uri);
		free(rdata);
	}
	repolist_refcnt = 0;
	repolist_initialized = false;
	DPRINTF(("%s: released ok.\n", __func__));
}
