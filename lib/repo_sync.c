/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"
#include "fetch.h"

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
int HIDDEN
xbps_repo_sync(struct xbps_handle *xhp, const char *uri)
{
	mode_t prev_umask;
	const char *arch, *fetchstr = NULL;
	char *repodata, *lrepodir, *uri_fixedp;
	int rv = 0;

	assert(uri != NULL);

	/* ignore non remote repositories */
	if (!xbps_repository_is_remote(uri))
		return 0;

	uri_fixedp = xbps_get_remote_repo_string(uri);
	if (uri_fixedp == NULL)
		return -1;

	if (xhp->target_arch)
		arch = xhp->target_arch;
	else
		arch = xhp->native_arch;

	/*
	 * Full path to repository directory to store the plist
	 * index file.
	 */
	lrepodir = xbps_xasprintf("%s/%s", xhp->metadir, uri_fixedp);
	free(uri_fixedp);
	/*
	 * Create repodir in metadir.
	 */
	prev_umask = umask(022);
	if ((rv = xbps_mkpath(lrepodir, 0755)) == -1) {
		if (errno != EEXIST) {
			xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL,
			    errno, NULL, "[reposync] failed "
			    "to create repodir `%s': %s", lrepodir,
			strerror(errno));
			umask(prev_umask);
			free(lrepodir);
			return rv;
		}
	}
	if (chdir(lrepodir) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL, errno, NULL,
		    "[reposync] failed to change dir to repodir `%s': %s",
		    lrepodir, strerror(errno));
		umask(prev_umask);
		free(lrepodir);
		return -1;
	}
	free(lrepodir);
	/*
	 * Remote repository plist index full URL.
	 */
	repodata = xbps_xasprintf("%s/%s-repodata", uri, arch);

	/* reposync start cb */
	xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC, 0, repodata, NULL);
	/*
	 * Download plist index file from repository.
	 */
	if ((rv = xbps_fetch_file(xhp, repodata, NULL)) == -1) {
		/* reposync error cb */
		fetchstr = xbps_fetch_error_string();
		xbps_set_cb_state(xhp, XBPS_STATE_REPOSYNC_FAIL,
		    fetchLastErrCode != 0 ? fetchLastErrCode : errno, NULL,
		    "[reposync] failed to fetch file `%s': %s",
		    repodata, fetchstr ? fetchstr : strerror(errno));
	} else if (rv == 1)
		rv = 0;
	umask(prev_umask);

	free(repodata);

	return rv;
}
