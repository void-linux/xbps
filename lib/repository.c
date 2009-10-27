/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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

int SYMEXPORT
xbps_register_repository(const char *uri)
{
	prop_dictionary_t dict;
	prop_array_t array;
	prop_object_t obj = NULL;
	const char *rootdir;
	char *plist;
	int rv = 0;

	assert(uri != NULL);

	rootdir = xbps_get_rootdir();
	plist = xbps_xasprintf("%s/%s/%s", rootdir,
	    XBPS_META_PATH, XBPS_REPOLIST);
	if (plist == NULL)
		return errno;

	/* First check if we have the repository plist file. */
	dict = prop_dictionary_internalize_from_file(plist);
	if (dict == NULL) {
		/* Looks like not, create it. */
		dict = prop_dictionary_create();
		if (dict == NULL) {
			free(plist);
			return errno;
		}
		/* Create the array and add the repository URI on it. */
		array = prop_array_create();
		if (array == NULL) {
			rv = errno;
			goto out;
		}
		if (!prop_array_set_cstring_nocopy(array, 0, uri)) {
			rv = errno;
			goto out;
		}
		/* Add the array obj into the main dictionary. */
		if (!xbps_add_obj_to_dict(dict, array, "repository-list")) {
			rv = errno;
			goto out;
		}
	} else {
		/* Append into the array, the plist file exists. */
		array = prop_dictionary_get(dict, "repository-list");
		if (array == NULL) {
			rv = errno;
			goto out;
		}
		/* It seems that this object is already there */
		if (xbps_find_string_in_array(array, uri)) {
			errno = EEXIST;
			goto out;
		}

		obj = prop_string_create_cstring(uri);
		if (!xbps_add_obj_to_array(array, obj)) {
			prop_object_release(obj);
			rv = errno;
			goto out;
		}
	}

	/* Write dictionary into plist file. */
	if (!prop_dictionary_externalize_to_file(dict, plist)) {
		if (obj)
			prop_object_release(obj);
		rv = errno;
		goto out;
	}

out:
	prop_object_release(dict);
	free(plist);

	return rv;
}

int SYMEXPORT
xbps_unregister_repository(const char *uri)
{
	prop_dictionary_t dict;
	prop_array_t array;
	const char *rootdir;
	char *plist;
	int rv = 0;

	assert(uri != NULL);

	rootdir = xbps_get_rootdir();
	plist = xbps_xasprintf("%s/%s/%s", rootdir,
	    XBPS_META_PATH, XBPS_REPOLIST);
	if (plist == NULL)
		return errno;

	dict = prop_dictionary_internalize_from_file(plist);
	if (dict == NULL) {
		free(plist);
		return errno;
	}

	array = prop_dictionary_get(dict, "repository-list");
	if (array == NULL) {
		rv = errno;
		goto out;
	}

	rv = xbps_remove_string_from_array(array, uri);
	if (rv == 0) {
		/* Update plist file. */
		if (!prop_dictionary_externalize_to_file(dict, plist))
			rv = errno;
	}

out:
	prop_object_release(dict);
	free(plist);

	return rv;
}

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
	const char *rootdir = xbps_get_rootdir();
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
	    rootdir, XBPS_META_PATH, uri_fixedp, un.machine);
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
	    rootdir, XBPS_META_PATH, uri_fixedp);
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
	rv = xbps_fetch_file(rpidx, lrepodir);

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
