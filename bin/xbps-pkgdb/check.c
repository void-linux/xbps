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

struct check_context {
	int errors;
	unsigned ctr;
};

static int
check_cb(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	const char *pkgver = NULL;
	char pkgname[XBPS_NAME_SIZE];
	struct check_context *ctx = arg;
	int rv;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	xbps_verbose_printf("Checking %s ...\n", pkgver);

	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		abort();
	}
	if ((rv = check_pkg(xhp, obj, pkgname, ctx->ctr)) != 0)
		ctx->errors += 1;

	return 0;
}

int
check_all(struct xbps_handle *xhp, unsigned int checks)
{
	struct check_context args = {
		.errors = 0,
		.ctr = checks,
	};
	xbps_pkgdb_foreach_cb_multi(xhp, check_cb, &args);
	return args.errors ? -1 : 0;
}

int
check_pkg(struct xbps_handle *xhp,
		    xbps_dictionary_t pkgd,
		    const char *pkgname,
		    unsigned checks)
{
	xbps_dictionary_t opkgd, filesd;
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
		filesd = xbps_plist_dictionary_from_file(buf);
		if (filesd == NULL) {
			xbps_error_printf("%s: cannot read %s, ignoring...\n",
			    pkgname, buf);
			free(buf);
			return -ENOENT;
		}
		rv = xbps_file_sha256_check(buf, sha256);
		free(buf);
		if (rv == ENOENT) {
			xbps_dictionary_remove(opkgd, "metafile-sha256");
			xbps_error_printf("%s: unexistent metafile, "
			    "updating pkgdb.\n", pkgname);
		} else if (rv == ERANGE) {
			xbps_object_release(filesd);
			xbps_error_printf("%s: metadata file has been "
			    "modified!\n", pkgname);
			return -rv;
		}
	}

	if (checks & CHECK_FILES) {
		if (check_pkg_files(xhp, pkgname, filesd))
			errors++;
		if (check_pkg_symlinks(xhp, pkgname, filesd))
			errors++;
	}
	if (checks & CHECK_DEPENDENCIES) {
		if (check_pkg_rundeps(xhp, pkgname, opkgd))
			errors++;
	}
	if (checks & CHECK_ALTERNATIVES) {
		if (check_pkg_alternatives(xhp, pkgname, opkgd))
			errors++;
	}
	if (checks & CHECK_PKGDB) {
		if (check_pkg_unneeded(xhp, pkgname, opkgd))
			errors++;
	}

	if (filesd)
		xbps_object_release(filesd);

	return !!errors;
}
