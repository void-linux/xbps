/*-
 * Copyright (c) 2008, 2009 Joerg Sonnenberger <joerg (at) NetBSD.org>
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
 *-
 * From: $NetBSD: pkg_io.c,v 1.9 2009/08/16 21:10:15 joerg Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <xbps_api.h>
#include "fetch.h"

struct fetch_archive {
	struct url *url;
	struct fetchIO *fetch;
	char buffer[32768];
};

static int
fetch_archive_open(struct archive *a, void *client_data)
{
	struct fetch_archive *f = client_data;

	(void)a;

	f->fetch = fetchGet(f->url, NULL);
	if (f->fetch == NULL)
		return ENOENT;

	return 0;
}

static int
fetch_archive_read(struct archive *a, void *client_data, const void **buf)
{
	struct fetch_archive *f = client_data;

	(void)a;
	*buf = f->buffer;

	return fetchIO_read(f->fetch, f->buffer, sizeof(f->buffer));
}

static int
fetch_archive_close(struct archive *a, void *client_data)
{
	struct fetch_archive *f = client_data;

	(void)a;

	if (f->fetch != NULL)
		fetchIO_close(f->fetch);
	free(f);

	return 0;
}

static struct archive *
open_archive_by_url(struct url *url)
{
	struct fetch_archive *f;
	struct archive *a;

	f = malloc(sizeof(struct fetch_archive));
	if (f == NULL)
		return NULL;

	f->url = url;
	if ((a = archive_read_new()) == NULL) {
		free(f);
		return NULL;
	}
	archive_read_support_compression_all(a);
	archive_read_support_format_tar(a);

	if (archive_read_open(a, f, fetch_archive_open, fetch_archive_read,
	    fetch_archive_close)) {
		archive_read_finish(a);
		return NULL;
	}

	return a;
}

static struct archive *
open_archive(const char *url)
{
	struct url *u;
	struct archive *a;

	if (!xbps_check_is_repo_string_remote(url)) {
		if ((a = archive_read_new()) == NULL)
			return NULL;

		archive_read_support_compression_all(a);
		archive_read_support_format_tar(a);

		if (archive_read_open_filename(a, url,
		    ARCHIVE_READ_BLOCKSIZE)) {
			archive_read_close(a);
			return NULL;
		}
		return a;
	}
	
	if ((u = fetchParseURL(url)) == NULL)
		return NULL;

	a = open_archive_by_url(u);
	fetchFreeURL(u);

	return a;
}

static char *
binpkg_in_cachedir(prop_dictionary_t d, const char *uri)
{
	char *lbinfile;

	lbinfile = xbps_get_binpkg_local_path(d, uri);
	if (lbinfile == NULL)
		return NULL;

	if (access(lbinfile, R_OK) == 0)
		return lbinfile;

	return NULL;
}

char SYMEXPORT *
xbps_get_path_from_pkg_dict_repo(prop_dictionary_t d, const char *uri)
{
	const char *arch, *filen;
	char *path = NULL;

	path = binpkg_in_cachedir(d, uri);
	if (path)
		return path;

	if (!prop_dictionary_get_cstring_nocopy(d, "architecture", &arch))
		return NULL;
	if (!prop_dictionary_get_cstring_nocopy(d, "filename", &filen))
		return NULL;

	return xbps_xasprintf("%s/%s/%s", uri, arch, filen);
}

prop_dictionary_t SYMEXPORT
xbps_get_pkg_plist_dict_from_url(const char *url, const char *plistf)
{
	prop_dictionary_t plistd = NULL;
	struct archive *a;
	struct archive_entry *entry;
	const char *curpath;
	int i = 0;

	if ((a = open_archive(url)) == NULL)
		return NULL;

	while ((archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
		curpath = archive_entry_pathname(entry);
		if (i >= 5) {
			/*
			 * Archive does not contain required plist
			 * file, discard it completely.
			 */
			errno = ENOENT;
			break;
		}
		if (strstr(curpath, plistf) == 0) {
			archive_read_data_skip(a);
			i++;
			continue;
		}
		plistd = xbps_read_dict_from_archive_entry(a, entry);
		if (plistd == NULL) {
			errno = EINVAL;
			break;
		}
		break;
	}
	archive_read_finish(a);

	return plistd;
}

prop_dictionary_t SYMEXPORT
xbps_get_pkg_plist_dict_from_repo(const char *pkgname, const char *plistf)
{
	prop_dictionary_t plistd = NULL, pkgd;
	struct repository_data *rdata;
	char *url = NULL;
	int rv = 0;

	if ((rv = xbps_repository_pool_init()) != 0) {
		errno = rv;
		return NULL;
	}

	/*
	 * Iterate over the the repository pool and search for a plist file
	 * in the binary package named 'pkgname'. The plist file will be
	 * internalized to a proplib dictionary.
	 *
	 * The first repository that has it wins and the loop is stopped.
	 * This will work locally and remotely, thanks to libarchive and
	 * libfetch!
	 */
	SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
		pkgd = xbps_find_pkg_in_dict(rdata->rd_repod,
		    "packages", pkgname);
		if (pkgd == NULL) {
			if (errno != ENOENT)
				break;
			continue;
		}
		url = xbps_get_path_from_pkg_dict_repo(pkgd, rdata->rd_uri);
		if (url == NULL)
			break;
		plistd = xbps_get_pkg_plist_dict_from_url(url, plistf);
		if (plistd != NULL) {
			free(url);
			break;
		}
		free(url);
	}

	xbps_repository_pool_release();
	if (plistd == NULL)
		errno = ENOENT;

	return plistd;
}
