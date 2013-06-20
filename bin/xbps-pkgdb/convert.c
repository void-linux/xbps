/*-
 * Copyright (c) 2013 Juan Romero Pardines.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

/*
 * Convert pkgdb format to 0.21.
 */
static void
pkgdb_format_021(struct xbps_handle *xhp, const char *plist_new)
{
	xbps_array_t array, rdeps;
	xbps_dictionary_t pkgdb, pkgd;
	unsigned int i;
	char *pkgname, *plist;

	plist = xbps_xasprintf("%s/pkgdb.plist", xhp->metadir);
	if (access(plist, R_OK) == -1) {
		if (errno == ENOENT) {
			/* missing file, no conversion needed */
			free(plist);
			return;
		}
		xbps_error_printf("cannot read %s: %s\n",
		    plist, strerror(errno));
		exit(EXIT_FAILURE);
	}

	array = xbps_array_internalize_from_zfile(plist);
	if (xbps_object_type(array) != XBPS_TYPE_ARRAY) {
		xbps_error_printf("unknown object type for %s\n",
		    plist, strerror(errno));
		exit(EXIT_FAILURE);
	}

	pkgdb = xbps_dictionary_create();
	assert(pkgdb);

	for (i = 0; i < xbps_array_count(array); i++) {
		pkgd = xbps_array_get(array, i);
		xbps_dictionary_get_cstring(pkgd, "pkgname", &pkgname);
		rdeps = xbps_dictionary_get(pkgd, "run_depends");
		/* remove unneeded objs */
		xbps_dictionary_remove(pkgd, "pkgname");
		xbps_dictionary_remove(pkgd, "version");
		if (xbps_array_count(rdeps) == 0)
			xbps_dictionary_remove(pkgd, "run_depends");

		xbps_dictionary_set(pkgdb, pkgname, pkgd);
		free(pkgname);
	}

	if (xbps_array_count(array) != xbps_dictionary_count(pkgdb)) {
		xbps_error_printf("failed conversion! unmatched obj count "
		    "(got %zu, need %zu)\n", xbps_dictionary_count(pkgdb),
		    xbps_array_count(array));
		exit(EXIT_FAILURE);
	}

	if (!xbps_dictionary_externalize_to_file(pkgdb, plist_new)) {
		xbps_error_printf("failed to write %s: %s\n",
		    plist_new, strerror(errno));
		exit(EXIT_FAILURE);
	}

	xbps_object_release(array);
	xbps_object_release(pkgdb);
	free(plist);

	printf("Conversion to 0.21 pkgdb format successfully\n");
}

void
convert_pkgdb_format(struct xbps_handle *xhp)
{
	char *plist;

	plist = xbps_xasprintf("%s/%s", xhp->metadir, XBPS_PKGDB);
	if ((access(plist, R_OK) == -1) && (errno == ENOENT))
		pkgdb_format_021(xhp, plist);

	free(plist);
}
