/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines
 * Copyright (c) 2000-2004 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 * From FreeBSD fetch(8):
 * $FreeBSD: src/usr.bin/fetch/fetch.c,v 1.84.2.1 2009/08/03 08:13:06 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "xbps_api_impl.h"
#include "fetch.h"

/**
 * @file lib/download.c
 * @brief Download routines
 * @defgroup download Download functions
 *
 * XBPS download related functions, frontend for NetBSD's libfetch.
 */
static const char *
print_time(time_t *t)
{
	struct tm tm;
	static char buf[255];

	localtime_r(t, &tm);
	strftime(buf, sizeof(buf), "%d %b %Y %H:%M", &tm);
	return buf;
}

void HIDDEN
xbps_fetch_set_cache_connection(int global, int per_host)
{
	if (global == 0)
		global = XBPS_FETCH_CACHECONN;
	if (per_host == 0)
		per_host = XBPS_FETCH_CACHECONN_HOST;

	fetchConnectionCacheInit(global, per_host);
}

void HIDDEN
xbps_fetch_unset_cache_connection(void)
{
	fetchConnectionCacheClose();
}

const char *
xbps_fetch_error_string(void)
{
	if (fetchLastErrCode == 0 || fetchLastErrCode == FETCH_OK)
		return NULL;

	return fetchLastErrString;
}

int
xbps_fetch_file(const char *uri,
		const char *outputdir,
		bool refetch,
		const char *flags)
{
	struct xbps_handle *xhp;
	struct stat st;
	struct url *url = NULL;
	struct url_stat url_st;
	struct fetchIO *fio = NULL;
	struct timeval tv[2];
	off_t bytes_dload = -1;
	ssize_t bytes_read = -1, bytes_written;
	char buf[4096], *filename, *destfile = NULL;
	int fd = -1, rv = 0;
	bool restart = false;

	assert(uri != NULL);
	assert(outputdir != NULL);

	fetchLastErrCode = 0;

	xhp = xbps_handle_get();
	fetchTimeout = xhp->fetch_timeout;
	/*
	 * Get the filename specified in URI argument.
	 */
	if ((filename = strrchr(uri, '/')) == NULL)
		return -1;

	/* Skip first '/' */
	filename++;
	/*
	 * Compute destination file path.
	 */
	destfile = xbps_xasprintf("%s/%s", outputdir, filename);
	if (destfile == NULL) {
		rv = -1;
		goto out;
	}
	/*
	 * Check if we have to resume a transfer.
	 */
	memset(&st, 0, sizeof(st));
	if (stat(destfile, &st) == 0) {
		if (st.st_size > 0)
			restart = true;
	} else {
		if (errno != ENOENT) {
			rv = -1;
			goto out;
		}
	}
	/*
	 * Prepare stuff for libfetch.
	 */
	if ((url = fetchParseURL(uri)) == NULL) {
		rv = -1;
		goto out;

	}
	/*
	 * Check if we want to refetch from scratch a file.
	 */
	if (refetch) {
		/*
		 * Issue a HEAD request to know size and mtime.
		 */
		if ((rv = fetchStat(url, &url_st, NULL)) == -1)
			goto out;

		/*
		 * If mtime and size match do nothing.
		 */
		if (restart && url_st.size && url_st.mtime &&
		    url_st.size == st.st_size &&
		    url_st.mtime == st.st_mtime)
			goto out;

		/*
		 * If size match do nothing.
		 */
		if (restart && url_st.size && url_st.size == st.st_size)
			goto out;

		/*
		 * Remove current file (if exists).
		 */
		if (restart && remove(destfile) == -1) {
			rv = -1;
			goto out;
		}
		restart = false;
		url->offset = 0;
		/*
		 * Issue the GET request to refetch.
		 */
		fio = fetchGet(url, flags);
	} else {
		/*
		 * Issue a GET and skip the HEAD request, some servers
		 * (googlecode.com) return a 404 in HEAD requests!
		 */
		url->offset = st.st_size;
		fio = fetchXGet(url, &url_st, flags);
	}

	/* debug stuff */
	xbps_dbg_printf("st.st_size: %zd\n", (ssize_t)st.st_size);
	xbps_dbg_printf("st.st_atime: %s\n", print_time(&st.st_atime));
	xbps_dbg_printf("st.st_mtime: %s\n", print_time(&st.st_mtime));
	xbps_dbg_printf("url->scheme: %s\n", url->scheme);
	xbps_dbg_printf("url->host: %s\n", url->host);
	xbps_dbg_printf("url->port: %d\n", url->port);
	xbps_dbg_printf("url->doc: %s\n", url->doc);
	xbps_dbg_printf("url->offset: %zd\n", (ssize_t)url->offset);
	xbps_dbg_printf("url->length: %zu\n", url->length);
	xbps_dbg_printf("url->last_modified: %s\n",
	    print_time(&url->last_modified));
	xbps_dbg_printf("url_stat.size: %zd\n", (ssize_t)url_st.size);
	xbps_dbg_printf("url_stat.atime: %s\n", print_time(&url_st.atime));
	xbps_dbg_printf("url_stat.mtime: %s\n", print_time(&url_st.mtime));

	if (fio == NULL && fetchLastErrCode != FETCH_OK) {
		if (!refetch && restart && fetchLastErrCode == FETCH_UNAVAIL) {
			/*
			 * In HTTP when 416 is returned and length==0
			 * means that local and remote file size match.
			 * Because we are requesting offset==st_size! grr,
			 * stupid http servers...
			 */
			if (url->length == 0)
				goto out;
		}
		rv = -1;
		goto out;
	}
	if (url_st.size == -1) {
		xbps_dbg_printf("Remote file size is unknown!\n");
		errno = EINVAL;
		rv = -1;
		goto out;
	} else if (st.st_size > url_st.size) {
		/*
		 * Remove local file if bigger than remote, and refetch the
		 * whole shit again.
		 */
		xbps_dbg_printf("Local file %s is greater than remote, "
		    "removing local file and refetching...\n", filename);
		(void)remove(destfile);
	} else if (restart && url_st.mtime && url_st.size &&
		   url_st.size == st.st_size && url_st.mtime == st.st_mtime) {
		/* Local and remote size/mtime match, do nothing. */
		goto out;
	}
	/*
	 * If restarting, open the file for appending otherwise create it.
	 */
	if (restart)
		fd = open(destfile, O_WRONLY|O_APPEND);
	else
		fd = open(destfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);

	if (fd == -1) {
		rv = -1;
		goto out;
	}
	/*
	 * Initialize data for the fetch progress function callback
	 * and let the user know that the transfer is going to start
	 * immediately.
	 */
	xbps_set_cb_fetch(url_st.size, url->offset, url->offset,
	    filename, true, false, false);
	/*
	 * Start fetching requested file.
	 */
	while ((bytes_read = fetchIO_read(fio, buf, sizeof(buf))) > 0) {
		bytes_written = write(fd, buf, (size_t)bytes_read);
		if (bytes_written != bytes_read) {
			xbps_dbg_printf("Couldn't write to %s!\n", destfile);
			rv = -1;
			goto out;
		}
		bytes_dload += bytes_read;
		/*
		 * Let the fetch progress callback know that
		 * we are sucking more bytes from it.
		 */
		xbps_set_cb_fetch(url_st.size, url->offset, url->offset + bytes_dload,
		    filename, false, true, false);
	}
	if (bytes_read == -1) {
		xbps_dbg_printf("IO error while fetching %s: %s\n", filename,
		    fetchLastErrString);
		errno = EIO;
		rv = -1;
		goto out;
	}
	if (fd == -1) {
		rv = -1;
		goto out;
	}
	/*
	 * Let the fetch progress callback know that the file
	 * has been fetched.
	 */
	xbps_set_cb_fetch(url_st.size, url->offset, bytes_dload,
	    filename, false, false, true);
	/*
	 * Update mtime in local file to match remote file if transfer
	 * was successful.
	 */
	tv[0].tv_sec = url_st.atime ? url_st.atime : url_st.mtime;
	tv[1].tv_sec = url_st.mtime;
	tv[0].tv_usec = tv[1].tv_usec = 0;
	if (utimes(destfile, tv) == -1) {
		rv = -1;
		goto out;
	}
	/* File downloaded successfully */
	rv = 1;

out:
	if (fd != -1)
		(void)close(fd);
	if (fio != NULL)
		fetchIO_close(fio);
	if (url != NULL)
		fetchFreeURL(url);
	if (destfile != NULL)
		free(destfile);

	return rv;
}
