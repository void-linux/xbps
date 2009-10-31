/*-
 * Copyright (c) 2009 Juan Romero Pardines
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
#include <xbps_api.h>
#include "fetch.h"

struct xferstat {
	struct timeval	 start;
	struct timeval	 last;
	off_t		 size;
	off_t		 offset;
	off_t		 rcvd;
	const char 	 *name;
};

/*
 * Compute and display ETA
 */
static const char *
stat_eta(struct xferstat *xsp)
{
	static char str[16];
	long elapsed, eta;
	off_t received, expected;

	elapsed = xsp->last.tv_sec - xsp->start.tv_sec;
	received = xsp->rcvd - xsp->offset;
	expected = xsp->size - xsp->rcvd;
	eta = (long)(elapsed * expected / received);
	if (eta > 3600)
		snprintf(str, sizeof str, "%02ldh%02ldm",
		    eta / 3600, (eta % 3600) / 60);
	else
		snprintf(str, sizeof str, "%02ldm%02lds",
		    eta / 60, eta % 60);
	return str;
}

/*
 * Compute and display transfer rate
 */
static const char *
stat_bps(struct xferstat *xsp)
{
	static char str[16];
	char size[32];
	double delta, bps;

	delta = (xsp->last.tv_sec + (xsp->last.tv_usec / 1.e6))
	    - (xsp->start.tv_sec + (xsp->start.tv_usec / 1.e6));
	if (delta == 0.0) {
		snprintf(str, sizeof str, "-- stalled --");
	} else {
		bps = ((double)(xsp->rcvd - xsp->offset) / delta);
		(void)xbps_humanize_number(size, 6, (int64_t)bps, "",
		    HN_AUTOSCALE, HN_NOSPACE|HN_DECIMAL);
		snprintf(str, sizeof str, "%sB/s", size);
	}
	return str;
}

/*
 * Update the stats display
 */
static void
stat_display(struct xferstat *xsp)
{
	struct timeval now;
	char totsize[32], recvsize[32];

	gettimeofday(&now, NULL);
	if (now.tv_sec <= xsp->last.tv_sec)
		return;
	xsp->last = now;

	(void)xbps_humanize_number(totsize, 7, (int64_t)xsp->size, "",
	    HN_AUTOSCALE, HN_NOSPACE|HN_DECIMAL);
	(void)xbps_humanize_number(recvsize, 7, (int64_t)xsp->rcvd, "",
	    HN_AUTOSCALE, HN_NOSPACE|HN_DECIMAL);
	printf("Downloading %s ...", xsp->name);
	printf("\n\t%sB [%d%% of %sB]", recvsize,
	    (int)((double)(100.0 * (double)xsp->rcvd) / (double)xsp->size),
	    totsize);
	printf(" %s", stat_bps(xsp));
	if (xsp->size > 0 && xsp->rcvd > 0 &&
	    xsp->last.tv_sec >= xsp->start.tv_sec + 10)
		printf(" ETA: %s", stat_eta(xsp));
	printf("\n\033[2A\033[K");
}

/*
 * Initialize the transfer statistics
 */
static void
stat_start(struct xferstat *xsp, const char *name, off_t *size, off_t *offset)
{
	gettimeofday(&xsp->start, NULL);
	xsp->last.tv_sec = xsp->last.tv_usec = 0;
	xsp->name = name;
	xsp->size = *size;
	xsp->offset = *offset;
	xsp->rcvd = *offset;
}

/*
 * Update the transfer statistics
 */
static void
stat_update(struct xferstat *xsp, off_t rcvd)
{
	xsp->rcvd = rcvd + xsp->offset;
	stat_display(xsp);
}

/*
 * Finalize the transfer statistics
 */
static void
stat_end(struct xferstat *xsp)
{
	char size[32];

	(void)xbps_humanize_number(size, 6, (int64_t)xsp->size, "",
	    HN_AUTOSCALE, HN_NOSPACE|HN_DECIMAL);
	printf("\033[1A\033[K");
	printf("Downloaded %s successfully (%sB at %s)\n",
	    xsp->name, size, stat_bps(xsp));
	printf("\033[J");
}

const char SYMEXPORT *
xbps_fetch_error_string(void)
{
	return fetchLastErrString;
}

#ifdef DEBUG
static const char *
print_time(time_t *t)
{
	struct tm tm;
	static char buf[255];

	localtime_r(t, &tm);
	strftime(buf, sizeof(buf), "%d %b %Y %H:%M", &tm);
	return buf;
}
#endif

int SYMEXPORT
xbps_fetch_file(const char *uri, const char *outputdir, const char *flags)
{
	struct stat st;
	struct xferstat xs;
	struct url *url = NULL;
	struct url_stat url_st;
	struct fetchIO *fio = NULL;
	struct timeval tv[2];
	ssize_t bytes_read, bytes_written;
	off_t bytes_dld = -1;
	char buf[4096], *filename, *destfile = NULL, fetchflags[8];
	int fd = -1, rv = 0;
	bool restart = false;

	bytes_read = bytes_written = -1;
	fetchLastErrCode = 0;

	/*
	 * Get the filename specified in URI argument.
	 */
	filename = strrchr(uri, '/');
	if (filename == NULL)
		return EINVAL;
	filename++;

	/*
	 * Compute destination file path.
	 */
	destfile = xbps_xasprintf("%s/%s", outputdir, filename);
	if (destfile == NULL) {
		rv = errno;
		goto out;
	}

	/*
	 * Check if we have to resume a transfer.
	 */
	memset(&st, 0, sizeof(st));
	if (stat(destfile, &st) == 0)
		restart = true;
	else {
		if (errno != ENOENT) {
			rv = errno;
			goto out;
		}
	}

	/*
	 * Prepare stuff for libfetch.
	 */
	if ((url = fetchParseURL(uri)) == NULL) {
		rv = fetchLastErrCode;
		goto out;

	}
	/*
	 * Set client flags.
	 */
	if (flags != NULL)
		strcat(fetchflags, flags);

	/*
	 * Establish connection to remote host.
	 */
	url->offset = st.st_size;
	fio = fetchXGet(url, &url_st, fetchflags);
#ifdef DEBUG
	printf("st.st_size: %zd\n", (ssize_t)st.st_size);
	printf("st.st_atime: %s\n", print_time(&st.st_atime));
	printf("st.st_mtime: %s\n", print_time(&st.st_mtime));

	printf("url->scheme: %s\n", url->scheme);
	printf("url->host: %s\n", url->host);
	printf("url->port: %d\n", url->port);
	printf("url->doc: %s\n", url->doc);
	printf("url->offset: %zd\n", (ssize_t)url->offset);
	printf("url->length: %zu\n", url->length);
	printf("url->last_modified: %s\n", print_time(&url->last_modified));

	printf("url_stat.size: %zd\n", (ssize_t)url_st.size);
	printf("url_stat.atime: %s\n", print_time(&url_st.atime));
	printf("url_stat.mtime: %s\n", print_time(&url_st.mtime));
#endif
	if (fio == NULL) {
		/* Local and remote size match, do nothing */
		if (url->length == 0)
			goto out;

		rv = fetchLastErrCode;
		goto out;
	}

	if (url_st.size == -1) {
		printf("Remote file size is unknown!\n");
		rv = EINVAL;
		goto out;
	} else if (st.st_size > url_st.size) {
		printf("Local file %s is greater than remote file!\n",
		    filename);
		rv = EFBIG;
		goto out;
	} else if (st.st_size == url_st.size && st.st_mtime == url_st.mtime) {
		/* Local and remote size/mtime match, do nothing. */
		rv = 0;
		goto out;
	}
	printf("Connected to %s.\n", url->host);

	/*
	 * If restarting, open the file for appending otherwise create it.
	 */
	if (restart)
		fd = open(destfile, O_WRONLY|O_APPEND);
	else
		fd = open(destfile, O_WRONLY|O_CREAT, 0644);

	if (fd == -1) {
		rv = errno;
		goto out;
	}

	/*
	 * Start fetching requested file.
	 */
	if (xbps_fetch_start_cb != NULL)
		(*xbps_fetch_start_cb)(filename, &url_st.size, &url->offset);
	else
		stat_start(&xs, filename, &url_st.size, &url->offset);

	while ((bytes_read = fetchIO_read(fio, buf, sizeof(buf))) > 0) {
		bytes_written = write(fd, buf, (size_t)bytes_read);
		if (bytes_written != bytes_read) {
			printf("Couldn't write to %s!\n", destfile);
			rv = errno;
			goto out;
		}
		bytes_dld += bytes_read;
		if (xbps_fetch_update_cb != NULL)
			(*xbps_fetch_update_cb)(&bytes_dld);
		else
			stat_update(&xs, bytes_dld);
	}
	if (bytes_read == -1) {
		printf("IO error while fetching %s: %s\n", filename,
		    fetchLastErrString);
		rv = EINVAL;
		goto out;
	}
	if (xbps_fetch_end_cb != NULL)
		(*xbps_fetch_end_cb)();
	else
		stat_end(&xs);

	/*
	 * Update mtime to match remote file if download was successful.
	 */
	tv[0].tv_sec = url_st.atime ? url_st.atime : url_st.mtime;
	tv[1].tv_sec = url_st.mtime;
	tv[0].tv_usec = tv[1].tv_usec = 0;
	rv = utimes(destfile, tv);

out:
	if (fd != -1)
		(void)close(fd);
	if (fio)
		fetchIO_close(fio);
	if (url)
		fetchFreeURL(url);
	if (destfile)
		free(destfile);

	return rv;
}
