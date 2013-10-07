/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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

struct rpool {
	SIMPLEQ_ENTRY(rpool) entries;
	struct xbps_repo *repo;
};

static SIMPLEQ_HEAD(rpool_head, rpool) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

/**
 * @file lib/rpool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

int HIDDEN
xbps_rpool_init(struct xbps_handle *xhp)
{
	struct rpool *rp;
	const char *repouri;
	char *p;
	bool foundrepo = false;
	int retval, rv = 0;

	assert(xhp);

	if (xhp->rpool_initialized)
		return 0;

	p = xbps_xasprintf("%s/%s", xhp->metadir, XBPS_REPOKEYS);
	xhp->repokeys = xbps_dictionary_internalize_from_file(p);
	free(p);

	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		rp = malloc(sizeof(struct rpool));
		assert(rp);
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		if ((rp->repo = xbps_repo_open(xhp, repouri)) == NULL) {
			rp->repo = calloc(1, sizeof(struct xbps_repo));
			assert(rp->repo);
			rp->repo->xhp = xhp;
			rp->repo->uri = repouri;
			if (xbps_repository_is_remote(repouri))
				rp->repo->is_remote = true;
		}
		if (rp->repo->is_remote) {
			/*
			 * Import the RSA public key (if it's signed).
			 */
			retval = xbps_repo_key_import(rp->repo);
			if (retval == EAGAIN) {
				/* signed but public key was not imported */
				xbps_dbg_printf(xhp, "[rpool] `%s': public-key not yet imported.\n", repouri);
				rp->repo->is_signed = true;
				rp->repo->is_verified = false;
			} else if (retval != 0 && retval != EAGAIN) {
				/* any error */
				xbps_dbg_printf(xhp, "[rpool] %s: key_import %s\n",
				    repouri, strerror(retval));
			}
			if (!rp->repo->is_signed) {
				/* ignore unsigned repositories */
				xbps_repo_close(rp->repo);
			} else {
				/*
				 * Check the repository index signature against
				 * stored public key.
				 */
				retval = xbps_repo_key_verify(rp->repo);
				if (retval == 0) {
					/* signed, verified */
					xbps_set_cb_state(xhp, XBPS_STATE_REPO_SIGVERIFIED, 0, NULL, NULL);
				} else if (retval == EPERM) {
					/* signed, unverified */
					xbps_set_cb_state(xhp, XBPS_STATE_REPO_SIGUNVERIFIED, 0, NULL, NULL);
					xbps_repo_close(rp->repo);
					rp->repo->is_verified = false;
				} else {
					/* any error */
					xbps_dbg_printf(xhp, "[rpool] %s: key_verify %s\n",
					    repouri, strerror(retval));
					xbps_repo_close(rp->repo);
				}
			}
		}
		/*
		 * If repository has passed signature checks, add it to the pool.
		 */
		SIMPLEQ_INSERT_TAIL(&rpool_queue, rp, entries);
		foundrepo = true;
		xbps_dbg_printf(xhp, "[rpool] `%s' registered (%s, %s).\n",
		    repouri, rp->repo->is_signed ? "signed" : "unsigned",
		    rp->repo->is_verified ? "verified" : "unverified");
	}
	if (!foundrepo) {
		/* no repositories available, error out */
		rv = ENOTSUP;
		goto out;
	}
	xhp->rpool_initialized = true;
	xbps_dbg_printf(xhp, "[rpool] initialized ok.\n");
out:
	if (rv != 0)
		xbps_rpool_release(xhp);

	return rv;

}

void HIDDEN
xbps_rpool_release(struct xbps_handle *xhp)
{
	struct rpool *rp;

	if (!xhp->rpool_initialized)
		return;

	while ((rp = SIMPLEQ_FIRST(&rpool_queue))) {
		SIMPLEQ_REMOVE(&rpool_queue, rp, rpool, entries);
		xbps_repo_close(rp->repo);
		free(rp->repo);
		free(rp);
	}
	xbps_object_release(xhp->repokeys);
	xhp->repokeys = NULL;
	xhp->rpool_initialized = false;
	xbps_dbg_printf(xhp, "[rpool] released ok.\n");
}

int
xbps_rpool_sync(struct xbps_handle *xhp, const char *uri)
{
	const char *repouri;

	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		/* If argument was set just process that repository */
		if (uri && strcmp(repouri, uri))
			continue;

		if (xbps_repo_sync(xhp, repouri) == -1) {
			xbps_dbg_printf(xhp,
			    "[rpool] `%s' failed to fetch repository data: %s\n",
			    repouri, fetchLastErrCode == 0 ? strerror(errno) :
			    xbps_fetch_error_string());
			continue;
		}
	}
	return 0;
}

int
xbps_rpool_foreach(struct xbps_handle *xhp,
	int (*fn)(struct xbps_repo *, void *, bool *),
	void *arg)
{
	struct rpool *rp;
	int rv = 0;
	bool done = false;

	assert(fn != NULL);
	/* Initialize repository pool */
	if ((rv = xbps_rpool_init(xhp)) != 0) {
		if (rv == ENOTSUP) {
			xbps_dbg_printf(xhp, "[rpool] empty repository list.\n");
		} else if (rv != ENOENT && rv != ENOTSUP) {
			xbps_dbg_printf(xhp, "[rpool] couldn't initialize: %s\n", strerror(rv));
		}
		return rv;
	}
	/* Iterate over repository pool */
	SIMPLEQ_FOREACH(rp, &rpool_queue, entries) {
		rv = (*fn)(rp->repo, arg, &done);
		if (rv != 0 || done)
			break;
	}

	return rv;
}
