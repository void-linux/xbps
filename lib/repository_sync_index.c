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

#include <sys/utsname.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xbps_api.h>
#include "fetch.h"

char SYMEXPORT *
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
int SYMEXPORT
xbps_repository_sync_pkg_index(const char *uri)
{
	struct url *url = NULL;
	struct utsname un;
	struct stat st;
	const char *fetch_outputdir;
	char *rpidx, *lrepodir, *uri_fixedp;
	char *metadir, *tmp_metafile, *lrepofile;
	int rv = 0;
	bool only_sync = false;

	rpidx = lrepodir = uri_fixedp = NULL;
	metadir = tmp_metafile = lrepofile = NULL;

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
	metadir = xbps_xasprintf("%s/%s", xbps_get_rootdir(),
	    XBPS_META_PATH);
	if (metadir == NULL) {
		rv = -1;
		goto out;
	}
	if ((rv = xbps_mkpath(metadir, 0755)) == -1)
		goto out;

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
	    xbps_get_rootdir(), XBPS_META_PATH, uri_fixedp);
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

	/*
	 * Download pkg-index.plist file from repository.
	 */
	if ((rv = xbps_fetch_file(rpidx, fetch_outputdir,
	     true, NULL)) == -1) {
		(void)remove(tmp_metafile);
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
