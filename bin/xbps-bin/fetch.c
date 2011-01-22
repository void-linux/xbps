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

#include <xbps_api.h>
#include "defs.h"

struct xferstat {
	struct timeval start;
	struct timeval last;
};

/*
 * Compute and display ETA
 */
static const char *
stat_eta(struct xbps_fetch_progress_data *xfpd, struct xferstat *xsp)
{
	static char str[16];
	long elapsed, eta;
	off_t received, expected;

	elapsed = xsp->last.tv_sec - xsp->start.tv_sec;
	received = xfpd->file_dloaded - xfpd->file_offset;
	expected = xfpd->file_size - xfpd->file_dloaded;
	eta = (long)(elapsed * expected / received);
	if (eta > 3600)
		snprintf(str, sizeof str, "%02ldh%02ldm",
		    eta / 3600, (eta % 3600) / 60);
	else
		snprintf(str, sizeof str, "%02ldm%02lds",
		    eta / 60, eta % 60);

	return str;
}

/** High precision double comparison
 * \param[in] a The first double to compare
 * \param[in] b The second double to compare
 * \return true on equal within precison, false if not equal within defined precision.
 */
static inline bool
compare_double(const double a, const double b)
{
	const double precision = 0.00001;

	if ((a - precision) < b && (a + precision) > b)
		return true;
	else
		return false;
}

/*
 * Compute and display transfer rate
 */
static const char *
stat_bps(struct xbps_fetch_progress_data *xfpd, struct xferstat *xsp)
{
	static char str[16];
	char size[8];
	double delta, bps;

	delta = (xsp->last.tv_sec + (xsp->last.tv_usec / 1.e6))
	    - (xsp->start.tv_sec + (xsp->start.tv_usec / 1.e6));
	if (compare_double(delta, 0.0001)) {
		snprintf(str, sizeof str, "-- stalled --");
	} else {
		bps =
		    ((double)(xfpd->file_dloaded - xfpd->file_offset) / delta);
		(void)xbps_humanize_number(size, (int64_t)bps);
		snprintf(str, sizeof str, "%s/s", size);
	}
	return str;
}

/*
 * Update the stats display
 */
static void
stat_display(struct xbps_fetch_progress_data *xfpd, struct xferstat *xsp)
{
	struct timeval now;
	char totsize[8], recvsize[8];

	gettimeofday(&now, NULL);
	if (now.tv_sec <= xsp->last.tv_sec)
		return;
	xsp->last = now;

	(void)xbps_humanize_number(totsize, (int64_t)xfpd->file_size);
	(void)xbps_humanize_number(recvsize, (int64_t)xfpd->file_dloaded);
	fprintf(stderr,"\r%s: %s [%d%% of %s]", xfpd->file_name, recvsize,
	    (int)((double)(100.0 *
	    (double)xfpd->file_dloaded) / (double)xfpd->file_size), totsize);
	fprintf(stderr," %s", stat_bps(xfpd, xsp));
	if (xfpd->file_size > 0 && xfpd->file_dloaded > 0 &&
	    xsp->last.tv_sec >= xsp->start.tv_sec + 10)
		fprintf(stderr," ETA: %s", stat_eta(xfpd, xsp));

	fprintf(stderr,"\033[K");
}

/*
 * Initialize the transfer statistics
 */
static void
stat_start(struct xferstat *xsp)
{
	gettimeofday(&xsp->start, NULL);
	xsp->last.tv_sec = xsp->last.tv_usec = 0;
}

/*
 * Update the transfer statistics
 */
static void
stat_update(struct xbps_fetch_progress_data *xfpd, struct xferstat *xsp)
{
	xfpd->file_dloaded += xfpd->file_offset;
	stat_display(xfpd, xsp);
}

/*
 * Finalize the transfer statistics
 */
static void
stat_end(struct xbps_fetch_progress_data *xfpd, struct xferstat *xsp)
{
	char size[8];

	(void)xbps_humanize_number(size, (int64_t)xfpd->file_size);
	fprintf(stderr,"\rDownloaded %s for %s [avg rate: %s]",
	    size, xfpd->file_name, stat_bps(xfpd, xsp));
	fprintf(stderr,"\033[K\n");
}

void
fetch_file_progress_cb(void *data)
{
	struct xbps_fetch_progress_data *xfpd = data;
	static struct xferstat xs;

	if (xfpd->cb_start)
		stat_start(&xs);

	if (xfpd->cb_update)
		stat_update(xfpd, &xs);

	if (xfpd->cb_end)
		stat_end(xfpd, &xs);
}
