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

struct pkgdb_cb_args {
	int errors;
	unsigned ctr;
};

static int
pkgdb_cb(struct xbps_handle *xhp UNUSED,
		xbps_object_t obj,
		const char *key UNUSED,
		void *arg,
		bool *done UNUSED)
{
	const char *pkgver = NULL;
	char pkgname[XBPS_NAME_SIZE];
	struct pkgdb_cb_args *p = arg;
	int rv;

	xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	if (xhp->flags & XBPS_FLAG_VERBOSE)
		printf("Checking %s ...\n", pkgver);

	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		abort();
	}
	if ((rv = check_pkg_integrity(xhp, obj, pkgname, p->ctr)) != 0)
		p->errors += 1;

	return 0;
}

int
check_pkg_integrity_all(struct xbps_handle *xhp, unsigned checks_to_run)
{
	struct pkgdb_cb_args args = {
		.errors = 0,
		.ctr = checks_to_run,
	};
	xbps_pkgdb_foreach_cb_multi(xhp, pkgdb_cb, &args);
	return args.errors ? -1 : 0;
}

int
check_pkg_integrity(struct xbps_handle *xhp,
		    xbps_dictionary_t pkgd,
		    const char *pkgname,
		    unsigned checks_to_run)
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
		filesd = xbps_plist_dictionary_from_file(xhp, buf);
		if (filesd == NULL) {
			fprintf(stderr, "%s: cannot read %s, ignoring...\n",
			    pkgname, buf);
			free(buf);
			return -1;
		}
		rv = xbps_file_sha256_check(buf, sha256);
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
	if (check_pkg_##name(x, pkgname, arg)) { 	\
		errors++;					\
	}							\
} while (0)

	/* Execute pkg checks */
	if (checks_to_run & CHECK_FILES) {
		RUN_PKG_CHECK(xhp, files, filesd);
		RUN_PKG_CHECK(xhp, symlinks, filesd);
	}
	if (checks_to_run & CHECK_DEPENDENCIES)
		RUN_PKG_CHECK(xhp, rundeps, opkgd);
	if (checks_to_run & CHECK_ALTERNATIVES)
		RUN_PKG_CHECK(xhp, alternatives, opkgd);
	/* pkgdb internal checks go here */
	if (checks_to_run & CHECK_PKGDB) {
		RUN_PKG_CHECK(xhp, unneeded, opkgd);
	}

	if (filesd)
		xbps_object_release(filesd);

#undef RUN_PKG_CHECK

	return !!errors;
}

int
get_checks_to_run(unsigned *ctr, char *checks)
{
	char *const available[] = {
		[0] = "files",
		/* 'deps' is an alias for 'dependencies' */
		[1] = "dependencies",
		[2] = "deps",
		[3] = "alternatives",
		[4] = "pkgdb",
		NULL
	};

	/* Reset ctr before adding options */
	*ctr = 0;

	while (*checks) {
		char *value;
		int opt = getsubopt(&checks, available, &value);

		/* Checks don't support options like foo=bar */
		if (value)
			return 1;

		switch(opt) {
			case 0:
				*ctr |= CHECK_FILES;
				break;
			case 1:
			case 2:
				*ctr |= CHECK_DEPENDENCIES;
				break;
			case 3:
				*ctr |= CHECK_ALTERNATIVES;
				break;
			case 4:
				*ctr |= CHECK_PKGDB;
				break;
			default:
				return 1;
		}
	}

	/* If getsubopt exited because of end of string, return success */
	return 0;
}
