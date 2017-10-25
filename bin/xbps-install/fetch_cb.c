/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines
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

#include <xbps.h>
#include "defs.h"

static int v_tty; /* stderr is a tty */

static void
get_time(struct timeval *tvp)
{
#ifdef HAVE_CLOCK_GETTIME
	struct timespec ts;
	(void)clock_gettime(CLOCK_MONOTONIC, &ts);
	tvp->tv_sec = ts.tv_sec;
	tvp->tv_usec = ts.tv_nsec / 1000;
#else
	(void)gettimeofday(tvp, NULL);
#endif
}

/*
 * Compute and display ETA
 */
static const char *
stat_eta(const struct xbps_fetch_cb_data *xfpd, void *cbdata)
{
	struct xferstat *xfer = cbdata;
	static char str[25];
	long elapsed, eta;
	off_t received, expected;

	if (xfpd->file_size == -1)
		return "unknown";

	elapsed = xfer->last.tv_sec - xfer->start.tv_sec;
	received = xfpd->file_dloaded - xfpd->file_offset;
	expected = xfpd->file_size - xfpd->file_dloaded;
	eta = (long)((double)elapsed * expected / received);
	if (eta > 3600)
		snprintf(str, sizeof str, "%02ldh%02ldm",
		    eta / 3600, (eta % 3600) / 60);
	else
		snprintf(str, sizeof str, "%02ldm%02lds",
		    eta / 60, eta % 60);

	return str;
}

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
stat_bps(const struct xbps_fetch_cb_data *xfpd, void *cbdata)
{
	struct xferstat *xfer = cbdata;
	static char str[16];
	char size[8];
	double delta, bps;

	delta = (xfer->last.tv_sec + (xfer->last.tv_usec / 1.e6))
	    - (xfer->start.tv_sec + (xfer->start.tv_usec / 1.e6));
	if (compare_double(delta, 0.0001)) {
		snprintf(str, sizeof str, "-- stalled --");
	} else {
		bps = ((double)(xfpd->file_dloaded-xfpd->file_offset)/delta);
		(void)xbps_humanize_number(size, (int64_t)bps);
		snprintf(str, sizeof str, "%s/s", size);
	}
	return str;
}

/*
 * Update the stats display
 */
static void
stat_display(const struct xbps_fetch_cb_data *xfpd, void *cbdata)
{
	struct xferstat *xfer = cbdata;
	struct timeval now;
	char totsize[8];
	int percentage;

	get_time(&now);
	if (now.tv_sec <= xfer->last.tv_sec)
		return;
	xfer->last = now;

	if (xfpd->file_size == -1) {
		percentage = 0;
		snprintf(totsize, 3, "0B");
	} else {
		percentage = (int)((double)(100.0 *
		    (double)xfpd->file_dloaded) / (double)xfpd->file_size);
		(void)xbps_humanize_number(totsize, (int64_t)xfpd->file_size);
	}
	if (v_tty)
		fprintf(stderr, "%s: [%s %d%%] %s ETA: %s\033[K\r",
		    xfpd->file_name, totsize, percentage,
		    stat_bps(xfpd, xfer), stat_eta(xfpd, xfer));
	else {
		printf("%s: [%s %d%%] %s ETA: %s\n",
		    xfpd->file_name, totsize, percentage,
		    stat_bps(xfpd, xfer), stat_eta(xfpd, xfer));
		fflush(stdout);
	}
}

void
fetch_file_progress_cb(const struct xbps_fetch_cb_data *xfpd, void *cbdata)
{
	struct xferstat *xfer = cbdata;
	char size[8];

	if (xfpd->cb_start) {
		/* start transfer stats */
		v_tty = isatty(STDOUT_FILENO);
		get_time(&xfer->start);
		xfer->last.tv_sec = xfer->last.tv_usec = 0;
	} else if (xfpd->cb_update) {
		/* update transfer stats */
		stat_display(xfpd, xfer);
	} else if (xfpd->cb_end) {
		/* end transfer stats */
		(void)xbps_humanize_number(size, (int64_t)xfpd->file_dloaded);
		if (v_tty)
			fprintf(stderr, "%s: %s [avg rate: %s]\033[K\n",
			    xfpd->file_name, size, stat_bps(xfpd, xfer));
		else {
			printf("%s: %s [avg rate: %s]\n",
			    xfpd->file_name, size, stat_bps(xfpd, xfer));
			fflush(stdout);
		}
	}
}
