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

#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"
#include "fetch.h"

/**
 * @file lib/repository_sync_index.c
 * @brief Repository package index synchronization routines
 * @defgroup reposync Repository package index synchronization functions
 *
 * Functions to manipulate repository package index plist file
 * synchronizations.
 */

char HIDDEN *
xbps_get_remote_repo_string(const char *uri)
{
	struct url *url;
	size_t i;
	char *p;

	if ((url = fetchParseURL(uri)) == NULL)
		return NULL;

	/*
	 * Replace '.' ':' and '/' characters with underscores, so that
	 * provided URL:
	 *
	 * 	http://nocturno.local:8080/repo/x86_64
	 *
	 * becomes:
	 *
	 * 	http___nocturno_local_8080_repo_x86_64
	 * 	
	 */
	if (url->port != 0)
		p = xbps_xasprintf("%s://%s:%u%s", url->scheme,
		    url->host, url->port, url->doc);
	else
		p = xbps_xasprintf("%s://%s%s", url->scheme,
		    url->host, url->doc);

	fetchFreeURL(url);
	for (i = 0; i < strlen(p); i++) {
		if (p[i] == '.' || p[i] == '/' || p[i] == ':')
			p[i] = '_';
	}

	return p;
}

/*
 * Returns -1 on error, 0 if transfer was not necessary (local/remote
 * size and/or mtime match) and 1 if downloaded successfully.
 */
int
xbps_repository_sync_pkg_index(struct xbps_handle *xhp,
			       const char *uri,
			       const char *plistf)
{
	prop_array_t array;
	struct url *url = NULL;
	struct stat st;
	const char *fetch_outputdir, *fetchstr = NULL;
	char *rpidx, *lrepodir, *uri_fixedp;
	char *tmp_metafile, *lrepofile;
	int rv = 0;
	bool only_sync = false;

	assert(uri != NULL);
	tmp_metafile = rpidx = lrepodir = lrepofile = NULL;

	/* ignore non remote repositories */
	if (!xbps_check_is_repository_uri_remote(uri))
		return 0;

	if ((url = fetchParseURL(uri)) == NULL)
		return -1;

	uri_fixedp = xbps_get_remote_repo_string(uri);
	if (uri_fixedp == NULL) {
		fetchFreeURL(url);
		return -1;
	}
	/*
	 * Create metadir if necessary.
	 */
	if ((rv = xbps_mkpath(xhp->metadir, 0755)) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL,
		    errno, NULL, NULL,
		    "[reposync] failed to create metadir `%s': %s",
		    xhp->metadir, strerror(errno));
		goto out;
	}
	/*
	 * Remote repository plist index full URL.
	 */
	rpidx = xbps_xasprintf("%s/%s", uri, plistf);
	/*
	 * Save temporary file in metadir, and rename if it
	 * was downloaded successfully.
	 */
	tmp_metafile = xbps_xasprintf("%s/%s", xhp->metadir, plistf);
	/*
	 * Full path to repository directory to store the plist
	 * index file.
	 */
	lrepodir = xbps_xasprintf("%s/%s", xhp->metadir, uri_fixedp);
	/*
	 * If directory exists probably the plist index file
	 * was downloaded previously...
	 */
	rv = stat(lrepodir, &st);
	if (rv == 0 && S_ISDIR(st.st_mode)) {
		only_sync = true;
		fetch_outputdir = lrepodir;
	} else
		fetch_outputdir = xhp->metadir;

	/* reposync start cb */
	xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC, 0, uri, plistf, NULL);
	/*
	 * Download plist index file from repository.
	 */
	if (xbps_fetch_file(xhp, rpidx, fetch_outputdir, true, NULL) == -1) {
		/* reposync error cb */
		fetchstr = xbps_fetch_error_string();
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL,
		    fetchLastErrCode != 0 ? fetchLastErrCode : errno,
		    NULL, NULL,
		    "[reposync] failed to fetch file `%s': %s",
		    rpidx, fetchstr ? fetchstr : strerror(errno));
		rv = -1;
		goto out;
	}
	if (only_sync)
		goto out;
	/*
	 * Make sure that downloaded plist file can be internalized, i.e
	 * some HTTP servers don't return proper errors and sometimes
	  you get an HTML ASCII file :-)
	 */
	array = prop_array_internalize_from_zfile(tmp_metafile);
	if (array == NULL) {
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL, 0, NULL, NULL,
		    "[reposync] downloaded file `%s' is not valid.", rpidx);
		(void)unlink(tmp_metafile);
		rv = -1;
		goto out;
	}
	prop_object_release(array);

	lrepofile = xbps_xasprintf("%s/%s", lrepodir, plistf);
	/*
	 * Create local repodir to store plist index file.
	 */
	if ((rv = xbps_mkpath(lrepodir, 0755)) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL, errno, NULL, NULL,
		    "[reposync] failed to create repodir for `%s': %s",
		    lrepodir, strerror(rv));
		goto out;
	}

	/*
	 * Rename to destination file now it has been fetched successfully.
	 */
	if ((rv = rename(tmp_metafile, lrepofile)) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL, errno, NULL, NULL,
		    "[reposync] failed to rename index file `%s' to `%s': %s",
		    tmp_metafile, lrepofile, strerror(errno));
	} else {
		rv = 1; /* success */
	}

out:
	if (rpidx)
		free(rpidx);
	if (lrepodir)
		free(lrepodir);
	if (tmp_metafile)
		free(tmp_metafile);
	if (lrepofile)
		free(lrepofile);
	if (url)
		fetchFreeURL(url);
	if (uri_fixedp)
		free(uri_fixedp);

	return rv;
}
