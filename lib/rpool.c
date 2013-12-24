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

static SIMPLEQ_HEAD(rpool_head, xbps_repo) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

/**
 * @file lib/rpool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

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

struct xbps_repo *
xbps_rpool_get_repo(const char *url)
{
	struct xbps_repo *repo;

	SIMPLEQ_FOREACH(repo, &rpool_queue, entries)
		if (strcmp(url, repo->uri) == 0)
			return repo;

	return NULL;
}

int
xbps_rpool_foreach(struct xbps_handle *xhp,
	int (*fn)(struct xbps_repo *, void *, bool *),
	void *arg)
{
	struct xbps_repo *repo;
	const char *repouri;
	int rv = 0;
	bool foundrepo = false, done = false;

	assert(fn != NULL);

	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		repo = xbps_rpool_get_repo(repouri);
		if (!repo) {
			repo = xbps_repo_open(xhp, repouri);
			SIMPLEQ_INSERT_TAIL(&rpool_queue, repo, entries);
			xbps_dbg_printf(xhp, "[rpool] `%s' registered.\n", repouri);
		}
		foundrepo = true;
		rv = (*fn)(repo, arg, &done);
		if (rv != 0 || done)
			break;
	}
	if (!foundrepo)
		rv = ENOTSUP;

	return rv;
}
