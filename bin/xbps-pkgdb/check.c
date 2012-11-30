/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps_api.h>
#include "defs.h"

struct checkpkg {
	size_t totalpkgs;
	size_t npkgs;
	size_t nbrokenpkgs;
};

static int
cb_pkg_integrity(struct xbps_handle *xhp,
		 prop_object_t obj,
		 void *arg,
		 bool *done)
{
	struct checkpkg *cpkg = arg;
	const char *pkgname, *version;

	(void)done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(obj, "version", &version);

	printf("[%zu/%zu] checking %s-%s ...\n",
	    cpkg->npkgs, cpkg->totalpkgs, pkgname, version);

	if (check_pkg_integrity(xhp, obj, pkgname) != 0)
		cpkg->nbrokenpkgs++;

	cpkg->npkgs++;
	return 0;
}

int
check_pkg_integrity_all(struct xbps_handle *xhp)
{
	struct checkpkg cpkg;
	int rv;

	memset(&cpkg, 0, sizeof(cpkg));
	/* force an update to get total pkg count */
	(void)xbps_pkgdb_update(xhp, false);
	cpkg.totalpkgs = prop_array_count(xhp->pkgdb);

	(void)xbps_pkgdb_foreach_cb(xhp, cb_pkg_integrity, &cpkg);
	if ((rv = xbps_pkgdb_update(xhp, true)) != 0) {
		xbps_error_printf("failed to write pkgdb: %s\n",
		    strerror(rv));
		return rv;
	}
	printf("%zu package%s processed: %zu broken.\n", cpkg.npkgs,
	    cpkg.npkgs == 1 ? "" : "s", cpkg.nbrokenpkgs);
	return 0;
}

int
check_pkg_integrity(struct xbps_handle *xhp,
		    prop_dictionary_t pkgd,
		    const char *pkgname)
{
	prop_array_t rundeps;
	prop_dictionary_t opkgd, propsd;
	const char *sha256;
	char *buf;
	int rv = 0;

	propsd = opkgd = NULL;

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
	 * Check for props.plist metadata file.
	 */
	propsd = xbps_pkgdb_get_pkg_metadata(xhp, pkgname);
	if (propsd == NULL) {
		printf("%s: unexistent metafile, converting to 0.18 "
		    "format...\n", pkgname);
		if ((rv = convert_pkgd_metadir(xhp, opkgd)) != 0)
			return rv;

		return 0;

	} else if (prop_dictionary_count(propsd) == 0) {
		xbps_error_printf("%s: incomplete metadata file.\n", pkgname);
		return 1;
	}
	/*
	 * Check if pkgdb pkg has been converted to 0.19 format,
	 * which adds "run_depends" array object.
	 */
	rundeps = prop_dictionary_get(opkgd, "run_depends");
	if (rundeps == NULL) {
		rundeps = prop_dictionary_get(propsd, "run_depends");
		if (rundeps == NULL)
			rundeps = prop_array_create();

		prop_dictionary_set(opkgd, "run_depends", rundeps);
		/* remove requiredby object, unneeded since 0.19 */
		prop_dictionary_remove(opkgd, "requiredby");
	}

	/*
	 * Check pkg metadata signature.
	 */
	prop_dictionary_get_cstring_nocopy(opkgd, "metafile-sha256", &sha256);
	if (sha256 != NULL) {
		buf = xbps_xasprintf("%s/.%s.plist",
		    xhp->metadir, pkgname);
		rv = xbps_file_hash_check(buf, sha256);
		free(buf);
		if (rv == ERANGE) {
			fprintf(stderr, "%s: metadata file has been "
			    "modified!\n", pkgname);
			return 1;
		}
	}

#define RUN_PKG_CHECK(x, name, arg)				\
do {								\
	rv = check_pkg_##name(x, pkgname, arg);			\
	if (rv == -1) {						\
		xbps_error_printf("%s: the %s test "		\
		    "returned error!\n", pkgname, #name);	\
		return rv;					\
	}							\
} while (0)

	/* Execute pkg checks */
	RUN_PKG_CHECK(xhp, files, propsd);
	RUN_PKG_CHECK(xhp, symlinks, propsd);
	RUN_PKG_CHECK(xhp, rundeps, propsd);
	RUN_PKG_CHECK(xhp, unneeded, opkgd);

#undef RUN_PKG_CHECK

	if ((rv == 0) && (pkgd == NULL))
		(void)xbps_pkgdb_update(xhp, true);

	return 0;
}
