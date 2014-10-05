/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines
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
#define _BSD_SOURCE	/* musl has strlcpy if _{BSD,GNU}_SOURCE is defined */
#include <string.h>
#undef _BSD_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <libgen.h>

#include "xbps_api_impl.h"
#include "fetch.h"
#include "compat.h"

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

	gmtime_r(t, &tm);
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
xbps_fetch_file_dest(struct xbps_handle *xhp, const char *uri, const char *filename, const char *flags)
{
	struct stat st, st_tmpfile, *stp;
	struct url *url = NULL;
	struct url_stat url_st;
	struct fetchIO *fio = NULL;
	struct timespec ts[2];
	off_t bytes_dload = 0;
	ssize_t bytes_read = 0, bytes_written = 0;
	char buf[4096], *tempfile = NULL;
	char fetch_flags[8];
	int fd = -1, rv = 0;
	bool refetch = false, restart = false;

	assert(xhp);
	assert(uri);

	/* Extern vars declared in libfetch */
	fetchLastErrCode = 0;

	if (!filename || (url = fetchParseURL(uri)) == NULL)
		return -1;

	memset(&fetch_flags, 0, sizeof(fetch_flags));
	if (flags != NULL)
		strlcpy(fetch_flags, flags, 7);

	tempfile = xbps_xasprintf("%s.part", filename);
	/*
	 * Check if we have to resume a transfer.
	 */
	memset(&st_tmpfile, 0, sizeof(st_tmpfile));
	if (stat(tempfile, &st_tmpfile) == 0) {
		if (st_tmpfile.st_size > 0)
			restart = true;
	} else {
		if (errno != ENOENT) {
			rv = -1;
			goto fetch_file_out;
		}
	}
	/*
	 * Check if we have to refetch a transfer.
	 */
	memset(&st, 0, sizeof(st));
	if (stat(filename, &st) == 0) {
		refetch = true;
		url->last_modified = st.st_mtime;
		strlcat(fetch_flags, "i", sizeof(fetch_flags));
	} else {
		if (errno != ENOENT) {
			rv = -1;
			goto fetch_file_out;
		}
	}
	if (refetch && !restart) {
		/* fetch the whole file, filename available */
		stp = &st;
	} else {
		/* resume transfer, partial file found */
		stp = &st_tmpfile;
		url->offset = stp->st_size;
	}
	/*
	 * Issue a GET request.
	 */
	fio = fetchXGet(url, &url_st, fetch_flags);

	/* debug stuff */
	xbps_dbg_printf(xhp, "st.st_size: %zd\n", (ssize_t)stp->st_size);
	xbps_dbg_printf(xhp, "st.st_atime: %s\n", print_time(&stp->st_atime));
	xbps_dbg_printf(xhp, "st.st_mtime: %s\n", print_time(&stp->st_mtime));
	xbps_dbg_printf(xhp, "url_stat.size: %zd\n", (ssize_t)url_st.size);
	xbps_dbg_printf(xhp, "url_stat.atime: %s\n", print_time(&url_st.atime));
	xbps_dbg_printf(xhp, "url_stat.mtime: %s\n", print_time(&url_st.mtime));

	if (fio == NULL) {
		if (fetchLastErrCode == FETCH_UNCHANGED) {
			/* Last-Modified matched */
			goto fetch_file_out;
		}
		rv = -1;
		goto fetch_file_out;
	}
	if (url_st.size == -1) {
		xbps_dbg_printf(xhp, "Remote file size is unknown, resume "
		     "not possible...\n");
		restart = false;
	} else if (stp->st_size > url_st.size) {
		/*
		 * Remove local file if bigger than remote, and refetch the
		 * whole shit again.
		 */
		xbps_dbg_printf(xhp, "Local file %s is greater than remote, "
		    "removing local file and refetching...\n", filename);
		(void)remove(tempfile);
		restart = false;
	}
	xbps_dbg_printf(xhp, "url->scheme: %s\n", url->scheme);
	xbps_dbg_printf(xhp, "url->host: %s\n", url->host);
	xbps_dbg_printf(xhp, "url->port: %d\n", url->port);
	xbps_dbg_printf(xhp, "url->doc: %s\n", url->doc);
	xbps_dbg_printf(xhp, "url->offset: %zd\n", (ssize_t)url->offset);
	xbps_dbg_printf(xhp, "url->length: %zu\n", url->length);
	xbps_dbg_printf(xhp, "url->last_modified: %s\n",
	    print_time(&url->last_modified));
	/*
	 * If restarting, open the file for appending otherwise create it.
	 */
	if (restart)
		fd = open(tempfile, O_WRONLY|O_APPEND|O_CLOEXEC);
	else
		fd = open(tempfile, O_WRONLY|O_CREAT|O_CLOEXEC|O_TRUNC, 0644);

	if (fd == -1) {
		rv = -1;
		goto fetch_file_out;
	}
	/*
	 * Initialize data for the fetch progress function callback
	 * and let the user know that the transfer is going to start
	 * immediately.
	 */
	xbps_set_cb_fetch(xhp, url_st.size, url->offset, url->offset,
	    filename, true, false, false);
	/*
	 * Start fetching requested file.
	 */
	while ((bytes_read = fetchIO_read(fio, buf, sizeof(buf))) > 0) {
		bytes_written = write(fd, buf, (size_t)bytes_read);
		if (bytes_written != bytes_read) {
			xbps_dbg_printf(xhp,
			    "Couldn't write to %s!\n", tempfile);
			rv = -1;
			goto fetch_file_out;
		}
		bytes_dload += bytes_read;
		/*
		 * Let the fetch progress callback know that
		 * we are sucking more bytes from it.
		 */
		xbps_set_cb_fetch(xhp, url_st.size, url->offset,
		    url->offset + bytes_dload,
		    filename, false, true, false);
	}
	if (bytes_read == -1) {
		xbps_dbg_printf(xhp, "IO error while fetching %s: %s\n",
		    filename, fetchLastErrString);
		errno = EIO;
		rv = -1;
		goto fetch_file_out;
	} else if (url_st.size > 0 && ((bytes_dload + url->offset) != url_st.size)) {
		xbps_dbg_printf(xhp, "file %s is truncated\n", filename);
		errno = EIO;
		rv = -1;
		goto fetch_file_out;
	}

	/*
	 * Let the fetch progress callback know that the file
	 * has been fetched.
	 */
	xbps_set_cb_fetch(xhp, url_st.size, url->offset, bytes_dload,
	    filename, false, false, true);
	/*
	 * Update mtime in local file to match remote file if transfer
	 * was successful.
	 */
	ts[0].tv_sec = url_st.atime ? url_st.atime : url_st.mtime;
	ts[1].tv_sec = url_st.mtime;
	ts[0].tv_nsec = ts[1].tv_nsec = 0;
	if (futimens(fd, ts) == -1) {
		rv = -1;
		goto fetch_file_out;
	}
	(void)close(fd);
	fd = -1;

	/* File downloaded successfully, rename to destfile */
	if (rename(tempfile, filename) == -1) {
		xbps_dbg_printf(xhp, "failed to rename %s to %s: %s",
		    tempfile, filename, strerror(errno));
		rv = -1;
		goto fetch_file_out;
	}
	rv = 1;

fetch_file_out:
	if (fio != NULL)
		fetchIO_close(fio);
	if (fd != -1)
		(void)close(fd);
	if (url != NULL)
		fetchFreeURL(url);
	if (tempfile != NULL)
		free(tempfile);

	return rv;
}

int
xbps_fetch_file(struct xbps_handle *xhp, const char *uri, const char *flags)
{
	char *filename;
	/*
	 * Get the filename specified in URI argument.
	 */
	filename = strrchr(uri, '/') + 1;
	return xbps_fetch_file_dest(xhp, uri, filename, flags);
}


int
xbps_fetch_delta(struct xbps_handle *xhp, const char *basefile, const char *uri, const char *filename, const char *flags)
{
	const char xdelta[] = "/usr/bin/xdelta3";
	char *basehash = NULL, *dname = NULL, *durl = NULL, *tempfile = NULL;
	int status, exitcode;
	pid_t pid;
	int rv = 0;
	struct stat dummystat;

	if (basefile == NULL ||
	    stat(basefile, &dummystat) ||
	    stat(xdelta, &dummystat)) {
		goto fetch_delta_fallback;
	}
	xbps_dbg_printf(xhp, "%s: found. Trying binary diff.\n", xdelta);

	basehash = xbps_file_hash(basefile);
	assert(basehash);

	dname = xbps_xasprintf("%s.%s.vcdiff", basename(__UNCONST(uri)), basehash);
	durl = xbps_xasprintf("%s.%s.vcdiff", uri, basehash);
	tempfile = xbps_xasprintf("%s.tmp", filename);

	if (xbps_fetch_file_dest(xhp, durl, dname, flags) < 0) {
		xbps_dbg_printf(xhp, "error while downloading %s, fallback to full "
				"download\n", durl);
		goto fetch_delta_fallback;
	}

	if ((pid = fork()) == 0) {
		execl(xdelta, xdelta, "-d", "-f", "-s", basefile, dname, tempfile, NULL);
		exit(127);
	} else if (pid < 0) {
		xbps_dbg_printf(xhp, "error while forking, fallback to full "
				"download\n");
		goto fetch_delta_fallback;
	}

	// wait for termination of background process
	waitpid(pid, &status, 0);

	exitcode = WEXITSTATUS(status);
	unlink(dname);
	switch (exitcode) {
	case 0:    // success
		rv = 1;
		if (rename(tempfile, filename) == -1) {
			xbps_dbg_printf(xhp, "failed to rename %s to %s: %s",
				tempfile, filename, strerror(errno));
			rv = -1;
		}
		goto fetch_delta_out;
	case 127:  // cannot execute binary
		xbps_dbg_printf(xhp, "failed to `%s`, fallback to full download\n",
				xdelta);
		goto fetch_delta_fallback;
	default:   // other error
		xbps_dbg_printf(xhp, "`%s` exited with code %d, fallback to full "
				"download\n", xdelta, exitcode);
		goto fetch_delta_fallback;
	}

fetch_delta_fallback:
	rv = xbps_fetch_file_dest(xhp, uri, filename, flags);
fetch_delta_out:
	if (tempfile != NULL)
		free(tempfile);
	if (dname != NULL)
		free(dname);
	if (durl != NULL)
		free(durl);
	if (basehash != NULL)
		free(basehash);

	return rv;
}
