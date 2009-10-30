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

static int	mkpath(char *, mode_t);

char SYMEXPORT *
xbps_get_remote_repo_string(const char *uri)
{
	struct url *url;
	size_t i;
	char *p;

	if ((url = fetchParseURL(uri)) == NULL)
		return NULL;

	/*
	 * Replace dots and slashes with underscores, so that
	 * provided URL:
	 *
	 * 	www.foo.org/blah/xbps/binpkg-repo
	 *
	 * becomes:
	 *
	 * 	www_foo_org_blah_xbps_binpkg_repo
	 * 	
	 */
	p = xbps_xasprintf("%s%s", url->host, url->doc);
	fetchFreeURL(url);
	if (p == NULL)
		return NULL;

	for (i = 0; i < strlen(p); i++) {
		if (p[i] == '.' || p[i] == '/')
			p[i] = '_';
	}

	return p;
}

int SYMEXPORT
xbps_sync_repository_pkg_index(const char *uri)
{
	struct url *url;
	struct utsname un;
	char *rpidx, *dir, *lrepodir, *uri_fixedp = NULL;
	int rv = 0;

	if (uname(&un) == -1)
		return errno;

	if ((url = fetchParseURL(uri)) == NULL)
		return errno;

	uri_fixedp = xbps_get_remote_repo_string(uri);
	if (uri_fixedp == NULL) {
		fetchFreeURL(url);
		return errno;
	}

	/*
	 * Create local arch repodir:
	 *
	 * 	<rootdir>/var/db/xbps/repo/<url_path_blah>/<arch>
	 */
	lrepodir = xbps_xasprintf("%s/%s/repo/%s/%s",
	    xbps_get_rootdir(), XBPS_META_PATH, uri_fixedp, un.machine);
	if (lrepodir == NULL) {
		fetchFreeURL(url);
		free(uri_fixedp);
		return errno;
	}
	if (mkpath(lrepodir, 0755) == -1) {
		free(lrepodir);
		free(uri_fixedp);
		fetchFreeURL(url);
		return errno;
	}
	/*
	 * Create local noarch repodir:
	 *
	 * 	<rootdir>/var/db/xbps/repo/<url_path_blah>/noarch
	 */
	dir = xbps_xasprintf("%s/%s/repo/%s/noarch",
	    xbps_get_rootdir(), XBPS_META_PATH, uri_fixedp);
	free(uri_fixedp);
	fetchFreeURL(url);
	if (dir == NULL) {
		free(lrepodir);
		return errno;
	}
	if (mkpath(dir, 0755) == -1) {
		free(dir);
		free(lrepodir);
		return errno;
	}
	free(dir);
	/*
	 * Download pkg-index.plist file from repository.
	 */
	rpidx = xbps_xasprintf("%s/%s/%s", uri, un.machine, XBPS_PKGINDEX);
	if (rpidx == NULL) {
		free(lrepodir);
		return errno;
	}
	rv = xbps_fetch_file(rpidx, lrepodir, NULL);

	free(rpidx);
	free(lrepodir);

	return rv;
}

/*
 * The following is a modified function from NetBSD's src/bin/mkdir/mkdir.c
 */

/*
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * mkpath -- create directories.
 *	path     - path
 *	mode     - file mode of terminal directory
 */
static int
mkpath(char *path, mode_t mode)
{
	struct stat sb;
	char *slash = path;
	int done = 0, rv;
	mode_t dir_mode;

	/*
	 * The default file mode is a=rwx (0777) with selected permissions
	 * removed in accordance with the file mode creation mask.  For
	 * intermediate path name components, the mode is the default modified
	 * by u+wx so that the subdirectories can always be created.
	 */
	if (mode == 0)
		mode = (S_IRWXU | S_IRWXG | S_IRWXO) & ~umask(0);

	dir_mode = mode | S_IWUSR | S_IXUSR;

	for (;;) {
		slash += strspn(slash, "/");
		slash += strcspn(slash, "/");

		done = (*slash == '\0');
		*slash = '\0';

		rv = mkdir(path, done ? mode : dir_mode);
		if (rv < 0) {
			/*
			 * Can't create; path exists or no perms.
			 * stat() path to determine what's there now.
			 */
			int	sverrno;

			sverrno = errno;
			if (stat(path, &sb) < 0) {
					/* Not there; use mkdir()s error */
				errno = sverrno;
				return -1;
			}
			if (!S_ISDIR(sb.st_mode)) {
					/* Is there, but isn't a directory */
				errno = ENOTDIR;
				return -1;
			}
		}
		if (done)
			break;

		*slash = '/';
	}

	return 0;
}
