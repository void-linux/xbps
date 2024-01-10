/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include <sys/ioctl.h>

#include <assert.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <xbps.h>
#include "defs.h"

int
get_maxcols(void)
{
	struct winsize ws;

	if (!isatty(STDOUT_FILENO)) {
		/* not a TTY, don't use any limit */
		return 0;
	}
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
		/* 80x24 terminal */
		return 80;
	}
	/* TTY columns */
	return ws.ws_col;
}

void
print_package_line(const char *str, unsigned int maxcols, bool reset)
{
	static unsigned int cols;
	static bool first;

	if (reset) {
		cols = 0;
		first = false;
		return;
	}
	cols += strlen(str) + 4;
	if (cols <= maxcols) {
		if (first == false) {
			printf("  ");
			first = true;
		}
	} else {
		printf("\n  ");
		cols = strlen(str) + 4;
	}
	printf("%s ", str);
}

static unsigned int
find_longest_pkgname(struct transaction *trans)
{
	xbps_object_t obj;
	const char *pkgname;
	unsigned int len = 0, max = 0;

	while ((obj = xbps_object_iterator_next(trans->iter)) != NULL) {
		if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname))
			continue;
		len = strlen(pkgname);
		if (max == 0 || len > max)
			max = len;
	}
	xbps_object_iterator_reset(trans->iter);
	return max+1;
}

const char *
ttype2str(xbps_dictionary_t pkgd)
{
	uint8_t r;

	assert(pkgd);

	if (!xbps_dictionary_get_uint8(pkgd, "transaction", &r))
		return NULL;

	switch (r) {
	case XBPS_TRANS_INSTALL:
		return "install";
	case XBPS_TRANS_REINSTALL:
		return "reinstall";
	case XBPS_TRANS_UPDATE:
		return "update";
	case XBPS_TRANS_REMOVE:
		return "remove";
	case XBPS_TRANS_CONFIGURE:
		return "configure";
	case XBPS_TRANS_HOLD:
		return "hold";
	case XBPS_TRANS_DOWNLOAD:
		return "download";
	default:
		return "unknown";
	}

	return NULL;
}

bool
print_trans_colmode(struct transaction *trans, unsigned int cols)
{
	xbps_dictionary_t ipkgd;
	xbps_object_t obj;
	xbps_trans_type_t ttype;
	const char *pkgver, *pkgname, *ipkgver, *ver, *iver, *tract;
	char size[8];
	uint64_t dlsize = 0;
	unsigned int x, blen, pnamelen, hdrlen;

	pnamelen = find_longest_pkgname(trans);
	/* header length */
	hdrlen = pnamelen + 61;
	if (cols <= hdrlen)
		return false;

	printf("\nName ");
	if (pnamelen < 5)
		pnamelen = 5;

	for (x = 5; x < pnamelen; x++)
		printf(" ");

	printf("Action    Version           New version            Download size\n");

	while ((obj = xbps_object_iterator_next(trans->iter)) != NULL) {
		bool dload = false;

		pkgver = pkgname = ipkgver = ver = iver = NULL;
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		xbps_dictionary_get_uint64(obj, "filename-size", &dlsize);
		xbps_dictionary_get_bool(obj, "download", &dload);

		ttype = xbps_transaction_pkg_type(obj);
		tract = ttype2str(obj);
		if (trans->xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
			tract = "download";
		}

		ipkgd = xbps_pkgdb_get_pkg(trans->xhp, pkgname);
		if (trans->xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
			ipkgd = NULL;
		}
		if (ipkgd) {
			xbps_dictionary_get_cstring_nocopy(ipkgd, "pkgver", &ipkgver);
			iver = xbps_pkg_version(ipkgver);
		}
		ver = xbps_pkg_version(pkgver);
		if (iver) {
			int rv = xbps_cmpver(iver, ver);
			if (rv == 1 && ttype != XBPS_TRANS_HOLD)
				tract = "downgrade";
		}
		/* print pkgname and some blanks */
		blen = pnamelen - strlen(pkgname);
		printf("%s", pkgname);
		for (x = 0; x < blen; x++)
			printf(" ");

		/* print action */
		printf("%s ", tract);
		for (x = strlen(tract); x < 9; x++)
			printf(" ");

		/* print installed version */
		if (iver == NULL)
			iver = "-";

		/* print new version */
		printf("%s ", iver);
		for (x = strlen(iver); x < 17; x++)
			printf(" ");

		if (ttype == XBPS_TRANS_REMOVE) {
			ver = "-";
		}
		if (dload)
			(void)xbps_humanize_number(size, (int64_t)dlsize);
		else {
			size[0] = '-';
			size[1] = '\0';
		}
		printf("%s ", ver);
		for (x = strlen(ver); x < 22; x++)
			printf(" ");
		/* print download size */
		printf("%s ", size);
		printf("\n");
	}
	xbps_object_iterator_reset(trans->iter);
	return true;
}
