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
	unsigned int i;
	bool foundrepo = false;
	int rv = 0;

	assert(xhp);

	if (xhp->rpool_initialized)
		return 0;
	else if (xhp->cfg == NULL)
		return ENOTSUP;

	for (i = 0; i < cfg_size(xhp->cfg, "repositories"); i++) {
		rp = malloc(sizeof(struct rpool));
		assert(rp);
		repouri = cfg_getnstr(xhp->cfg, "repositories", i);
		if ((rp->repo = xbps_repo_open(xhp, repouri)) == NULL) {
			free(rp);
			continue;
		}
		rp->repo->idx = xbps_repo_get_plist(rp->repo, XBPS_PKGINDEX);
		if (rp->repo->idx == NULL) {
			xbps_repo_close(rp->repo);
			free(rp);
			continue;
		}
		rp->repo->uri = repouri;
		rp->repo->xhp = xhp;
		SIMPLEQ_INSERT_TAIL(&rpool_queue, rp, entries);
		foundrepo = true;
		xbps_dbg_printf(xhp, "[rpool] `%s' registered.\n", repouri);
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
		free(rp);
	}
	xhp->rpool_initialized = false;
	xbps_dbg_printf(xhp, "[rpool] released ok.\n");
}

int
xbps_rpool_sync(struct xbps_handle *xhp, const char *uri)
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
	SIMPLEQ_FOREACH(rp, &rpool_queue, entries) {
		rv = (*fn)(rp->repo, arg, &done);
		if (rv != 0 || done)
			break;
	}

	return rv;
}
