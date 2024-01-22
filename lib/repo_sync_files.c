/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
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

#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xbps.h"
#include "xbps_api_impl.h"
#include "fetch.h"


int HIDDEN xbps_repo_sync_files(struct xbps_handle* xh, const char* uri) {
	mode_t      prev_umask;
	const char *arch, *fetchstr = NULL;
	char *      repodata, *lrepodir, *uri_fixedp;
	int         rv = 0;

	/* ignore non remote repositories */
	if (!xbps_repository_is_remote(uri))
		return 0;
	
	uri_fixedp = xbps_get_remote_repo_string(uri);
	if (uri_fixedp == NULL)
		return -1;

	if (xh->target_arch)
		arch = xh->target_arch;
	else
		arch = xh->native_arch;

	/*
	 * Full path to repository directory to store the plist
	 * index file.
	 */
	lrepodir = xbps_xasprintf("%s/%s", xh->metadir, uri_fixedp);
	free(uri_fixedp);
	/*
	 * Create repodir in metadir.
	 */
	prev_umask = umask(022);
	if ((rv = xbps_mkpath(lrepodir, 0755)) == -1 && errno != EEXIST) {
		xbps_error_printf("[reposync] to create repodir `%s': %s\n", lrepodir, strerror(errno));
		umask(prev_umask);
		free(lrepodir);
		return rv;
	}
	if (chdir(lrepodir) == -1) {
		xbps_error_printf("[reposync] failed to change dir to repodir `%s': %s\n", lrepodir, strerror(errno));
		umask(prev_umask);
		free(lrepodir);
		return -1;
	}
	free(lrepodir);
	/*
	 * Remote repository plist index full URL.
	 */
	repodata = xbps_xasprintf("%s/%s-files", uri, arch);

	/* reposync start cb */
	printf("[*] Updating file-database `%s' ...\n", repodata);
	/*
	 * Download plist index file from repository.
	 */
	if ((rv = xbps_fetch_file(xh, repodata, NULL)) != 1 && fetchLastErrCode != FETCH_UNCHANGED) {
		/* reposync error cb */
		fetchstr = xbps_fetch_error_string();

		xbps_error_printf("[reposync] failed to fetch file `%s': %s\n", repodata, fetchstr ? fetchstr : strerror(errno));
	} else if (rv == 1)
		rv = 0;
	umask(prev_umask);

	free(repodata);

	return rv;
}

int xbps_rpool_sync_files(struct xbps_handle* xhp) {
	const char* repouri = NULL;

	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		if (xbps_repo_sync_files(xhp, repouri) == -1) {
			xbps_dbg_printf(
			    "[rpool] `%s' failed to fetch repository data: %s\n",
			    repouri, fetchLastErrCode == 0 ? strerror(errno) : xbps_fetch_error_string());
			continue;
		}
	}
	return 0;
}