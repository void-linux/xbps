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
	 * 	http://www.foo.org/blah/xbps/binpkg-repo
	 *
	 * becomes:
	 *
	 * 	http___www_foo_org_blah_xbps_binpkg_repo
	 * 	
	 */
	p = xbps_xasprintf("%s://%s%s", url->scheme, url->host, url->doc);
	fetchFreeURL(url);
	if (p == NULL)
		return NULL;

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
xbps_repository_sync_pkg_index(const char *uri)
{
	struct xbps_handle *xhp;
	struct url *url = NULL;
	struct utsname un;
	struct stat st;
	const char *fetch_outputdir;
	char *rpidx, *lrepodir, *uri_fixedp;
	char *metadir, *tmp_metafile, *lrepofile;
	int sverrno, rv = 0;
	bool only_sync = false;

	assert(uri != NULL);
	tmp_metafile = rpidx = lrepodir = lrepofile = NULL;
	xhp = xbps_handle_get();

	/* ignore non remote repositories */
	if (!xbps_check_is_repository_uri_remote(uri))
		return 0;

	if (uname(&un) == -1)
		return -1;

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
	metadir = xbps_xasprintf("%s/%s", xhp->rootdir, XBPS_META_PATH);
	if (metadir == NULL) {
		rv = -1;
		goto out;
	}
	if ((rv = xbps_mkpath(metadir, 0755)) == -1) {
		xbps_dbg_printf("%s: failed to create metadir `%s': %s\n",
		    __func__, metadir, strerror(errno));
		goto out;
	}
	/*
	 * Remote repository pkg-index.plist full URL.
	 */
	rpidx = xbps_xasprintf("%s/%s/%s", uri, un.machine, XBPS_PKGINDEX);
	if (rpidx == NULL) {
		rv = -1;
		goto out;
	}
	/*
	 * Save temporary file in XBPS_META_PATH, and rename if it
	 * was downloaded successfully.
	 */
	tmp_metafile = xbps_xasprintf("%s/%s", metadir, XBPS_PKGINDEX);
	if (tmp_metafile == NULL) {
		rv = -1;
		goto out;
	}
	/*
	 * Full path to local repository directory to store the
	 * package index file.
	 */
	lrepodir = xbps_xasprintf("%s/%s/%s",
	    xhp->rootdir, XBPS_META_PATH, uri_fixedp);
	if (lrepodir == NULL) {
		rv = -1;
		goto out;
	}
	/*
	 * If directory exists probably the pkg-index.plist file
	 * was downloaded previously...
	 */
	rv = stat(lrepodir, &st);
	if (rv == 0 && S_ISDIR(st.st_mode)) {
		only_sync = true;
		fetch_outputdir = lrepodir;
	} else
		fetch_outputdir = metadir;

	if (xhp->xbps_transaction_cb) {
		xhp->xtcd->state = XBPS_TRANS_STATE_REPOSYNC;
		xhp->xtcd->repourl = uri;
		xhp->xbps_transaction_cb(xhp->xtcd);
	}
	/*
	 * Download pkg-index.plist file from repository.
	 */
	if (xbps_fetch_file(rpidx, fetch_outputdir, true, NULL) == -1) {
		sverrno = errno;
		if (fetchLastErrCode)
			rv = fetchLastErrCode;
		else
			rv = errno;

		if (xhp->xbps_transaction_err_cb) {
			xhp->xtcd->state = XBPS_TRANS_STATE_REPOSYNC;
			xhp->xtcd->repourl = uri;
			xhp->xtcd->err = rv;
			xhp->xbps_transaction_cb(xhp->xtcd);
		}
		rv = -1;
		errno = sverrno;
		goto out;
	}
	if (only_sync)
		goto out;

	lrepofile = xbps_xasprintf("%s/%s", lrepodir, XBPS_PKGINDEX);
	if (lrepofile == NULL) {
		rv = -1;
		goto out;
	}
	/*
	 * Create local repodir to store pkg-index.plist file.
	 */
	if ((rv = xbps_mkpath(lrepodir, 0755)) == -1)
		goto out;

	/*
	 * Rename to destination file now it has been fetched successfully.
	 */
	if ((rv = rename(tmp_metafile, lrepofile)) == 0)
		rv = 1;

out:
	if (rpidx)
		free(rpidx);
	if (lrepodir)
		free(lrepodir);
	if (metadir)
		free(metadir);
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
