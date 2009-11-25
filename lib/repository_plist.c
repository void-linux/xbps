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

prop_dictionary_t SYMEXPORT
xbps_get_pkg_plist_dict_from_repo(const char *pkgname, const char *plistf)
{
	prop_dictionary_t plistd = NULL, pkgd;
	struct repository_data *rdata;
	struct archive *a;
	struct archive_entry *entry;
	const char *arch, *filen;
	char *url = NULL;
	int i = 0, rv = 0;
	bool found = false;

	if ((rv = xbps_prepare_repolist_data()) != 0) {
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
		if ((pkgd = xbps_find_pkg_in_dict(rdata->rd_repod,
		    "packages", pkgname)) == NULL)
			continue;
		if (!prop_dictionary_get_cstring_nocopy(pkgd,
		    "architecture", &arch))
			break;
		if (!prop_dictionary_get_cstring_nocopy(pkgd,
		    "filename", &filen))
			break;
		url = xbps_xasprintf("%s/%s/%s", rdata->rd_uri, arch, filen);
		if (url == NULL)
			break;

		if ((a = open_archive(url)) == NULL) {
			free(url);
			goto out;
		}
		free(url);

		while ((archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
			const char *curpath = archive_entry_pathname(entry);

			if (i >= 5) {
				/*
				 * Archive does not contain required plist
				 * file, discard it completely.
				 */
				archive_read_finish(a);
				errno = ENOENT;
				goto out;
			}
			if (strstr(curpath, plistf) == 0) {
				archive_read_data_skip(a);
				i++;
				continue;
			}
			plistd = xbps_read_dict_from_archive_entry(a, entry);
			if (plistd == NULL) {
				archive_read_finish(a);
				errno = EINVAL;
				goto out;
			}
			found = true;
			break;
		}
		archive_read_finish(a);
		if (found)
			break;
	}

out:
	xbps_release_repolist_data();
	if (found == false)
		errno = ENOENT;

	return plistd;
}
