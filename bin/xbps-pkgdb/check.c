/*-
 * Copyright (c) 2009-2015 Juan Romero Pardines.
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

#include <sys/param.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <xbps.h>
#include "defs.h"

static int
pkgdb_cb(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	const char *pkgver;
	char *pkgname;
	int rv, *errors = (int *)arg;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	if (xhp->flags & XBPS_FLAG_VERBOSE)
		printf("Checking %s ...\n", pkgver);

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);
	if ((rv = check_pkg_integrity(xhp, obj, pkgname)) != 0)
		*errors += 1;

	free(pkgname);
	return 0;
}

int
check_pkg_integrity_all(struct xbps_handle *xhp)
{
	int errors = 0;
	xbps_pkgdb_foreach_cb_multi(xhp, pkgdb_cb, &errors);
	return errors ? -1 : 0;
}

int
check_pkg_integrity(struct xbps_handle *xhp,
		    xbps_dictionary_t pkgd,
		    const char *pkgname)
{
	xbps_dictionary_t opkgd, filesd = NULL;
	const char *sha256;
	char *buf;
	int rv = 0, errors = 0;

	filesd = opkgd = NULL;

	/* find real pkg by name */
	opkgd = pkgd;
	if (opkgd == NULL) {
		if (((opkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) &&
		    ((opkgd = xbps_pkgdb_get_virtualpkg(xhp, pkgname)) == NULL)) {
			printf("Package %s is not installed.\n", pkgname);
			return 0;
		}
	}
	/*
	 * Check pkg files metadata signature.
	 */
	if (xbps_dictionary_get_cstring_nocopy(opkgd, "metafile-sha256", &sha256)) {
		buf = xbps_xasprintf("%s/.%s-files.plist",
		    xhp->metadir, pkgname);
		assert(buf);
		filesd = xbps_plist_dictionary_from_file(xhp, buf);
		if (filesd == NULL) {
			fprintf(stderr, "%s: cannot read %s, ignoring...\n",
			    pkgname, buf);
			free(buf);
			return -1;
		}
		rv = xbps_file_hash_check(buf, sha256);
		free(buf);
		if (rv == ENOENT) {
			xbps_dictionary_remove(opkgd, "metafile-sha256");
			fprintf(stderr, "%s: unexistent metafile, "
			    "updating pkgdb.\n", pkgname);
		} else if (rv == ERANGE) {
			xbps_object_release(filesd);
			fprintf(stderr, "%s: metadata file has been "
			    "modified!\n", pkgname);
			return 1;
		}
	}

#define RUN_PKG_CHECK(x, name, arg)				\
do {								\
	if ((rv = check_pkg_##name(x, pkgname, arg)) != 0) { 	\
		errors++;					\
	}							\
} while (0)

	/* Execute pkg checks */
	RUN_PKG_CHECK(xhp, files, filesd);
	RUN_PKG_CHECK(xhp, symlinks, filesd);
	RUN_PKG_CHECK(xhp, rundeps, opkgd);
	RUN_PKG_CHECK(xhp, unneeded, opkgd);

	if (filesd)
		xbps_object_release(filesd);

#undef RUN_PKG_CHECK

	return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}
