/*-
 * Copyright (c) 2012-2014 Juan Romero Pardines.
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

#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <fcntl.h>

#include <xbps.h>
#include "defs.h"

int
index_add(struct xbps_handle *xhp, int args, int argmax, char **argv, bool force)
{
	xbps_dictionary_t idx, idxmeta, binpkgd, curpkgd;
	struct xbps_repo *repo = NULL;
	struct stat st;
	char *tmprepodir = NULL, *repodir = NULL;
	int rv = 0, ret = 0;
	bool flush = false;

	assert(argv);
	/*
	 * Read the repository data or create index dictionaries otherwise.
	 */
	if ((tmprepodir = strdup(argv[args])) == NULL)
		return ENOMEM;

	repodir = dirname(tmprepodir);
	repo = xbps_repo_open(xhp, repodir, true);
	if (repo == NULL && errno != ENOENT) {
		fprintf(stderr, "xbps-rindex: cannot open/lock repository "
		    "%s: %s\n", repodir, strerror(errno));
		rv = -1;
		goto out;
	}
	if (repo) {
		idx = xbps_dictionary_copy(repo->idx);
		idxmeta = xbps_dictionary_copy(repo->idxmeta);
	} else {
		idx = xbps_dictionary_create();
		idxmeta = NULL;
	}
	/*
	 * Process all packages specified in argv.
	 */
	for (int i = args; i < argmax; i++) {
		const char *arch = NULL, *pkg = argv[i];
		char *sha256 = NULL, *pkgver = NULL, *pkgname = NULL;

		assert(pkg);
		/*
		 * Read metadata props plist dictionary from binary package.
		 */
		binpkgd = xbps_binpkg_get_plist(pkg, "/props.plist");
		if (binpkgd == NULL) {
			fprintf(stderr, "index: failed to read %s metadata for "
			    "`%s', skipping!\n", XBPS_PKGPROPS, pkg);
			continue;
		}
		xbps_dictionary_get_cstring_nocopy(binpkgd, "architecture", &arch);
		xbps_dictionary_get_cstring(binpkgd, "pkgver", &pkgver);
		if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
			fprintf(stderr, "index: ignoring %s, unmatched arch (%s)\n", pkgver, arch);
			xbps_object_release(binpkgd);
			free(pkgver);
			continue;
		}
		pkgname = xbps_pkg_name(pkgver);
		assert(pkgname);
		/*
		 * Check if this package exists already in the index, but first
		 * checking the version. If current package version is greater
		 * than current registered package, update the index; otherwise
		 * pass to the next one.
		 */
		curpkgd = xbps_dictionary_get(idx, pkgname);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				rv = errno;
				free(pkgver);
				free(pkgname);
				goto out;
			}
		} else if (!force) {
			char *opkgver = NULL, *oarch = NULL;

			/* Only check version if !force */
			xbps_dictionary_get_cstring(curpkgd, "pkgver", &opkgver);
			xbps_dictionary_get_cstring(curpkgd, "architecture", &oarch);
			ret = xbps_cmpver(pkgver, opkgver);

			/*
			 * If the considered package reverts the package in the index,
			 * consider the current package as the newer one.
			 */
			if (ret < 0 && xbps_pkg_reverts(binpkgd, opkgver)) {
				ret = 1;
			/*
			 * If package in the index reverts considered package, consider the
			 * package in the index as the newer one.
			 */
			} else if (ret > 0 && xbps_pkg_reverts(curpkgd, pkgver)) {
				ret = -1;
			}

			if (ret <= 0) {
				/* Same version or index version greater */
				fprintf(stderr, "index: skipping `%s' (%s), already registered.\n", pkgver, arch);
				xbps_object_release(binpkgd);
				free(opkgver);
				free(oarch);
				free(pkgver);
				free(pkgname);
				continue;
			}
			/*
			 * Current package version is greater than
			 * index version.
			 */
			printf("index: removed obsolete entry `%s' (%s).\n", opkgver, oarch);
			xbps_dictionary_remove(idx, pkgname);
			free(opkgver);
			free(oarch);
		}
		/*
		 * Add additional objects for repository ops:
		 * 	- filename-size
		 * 	- filename-sha256
		 */
		if ((sha256 = xbps_file_hash(pkg)) == NULL) {
			free(pkgver);
			free(pkgname);
			rv = EINVAL;
			goto out;
		}
		if (!xbps_dictionary_set_cstring(binpkgd, "filename-sha256", sha256)) {
			free(sha256);
			free(pkgver);
			free(pkgname);
			rv = EINVAL;
			goto out;
		}
		free(sha256);
		if (stat(pkg, &st) == -1) {
			free(pkgver);
			free(pkgname);
			rv = EINVAL;
			goto out;
		}
		if (!xbps_dictionary_set_uint64(binpkgd, "filename-size", (uint64_t)st.st_size)) {
			free(pkgver);
			free(pkgname);
			rv = EINVAL;
			goto out;
		}
		/* Remove unneeded objects */
		xbps_dictionary_remove(binpkgd, "pkgname");
		xbps_dictionary_remove(binpkgd, "version");

		/*
		 * Add new pkg dictionary into the index.
		 */
		if (!xbps_dictionary_set(idx, pkgname, binpkgd)) {
			free(pkgname);
			free(pkgver);
			rv = EINVAL;
			goto out;
		}
		flush = true;
		printf("index: added `%s' (%s).\n", pkgver, arch);
		xbps_object_release(binpkgd);
		free(pkgname);
		free(pkgver);
	}
	/*
	 * Generate repository data files.
	 */
	if (flush) {
		if (!repodata_flush(xhp, repodir, idx, idxmeta)) {
			fprintf(stderr, "%s: failed to write repodata: %s\n",
			    _XBPS_RINDEX, strerror(errno));
			goto out;
		}
	}
	printf("index: %u packages registered.\n", xbps_dictionary_count(idx));

out:
	if (repo)
		xbps_repo_close(repo, true);
	if (tmprepodir)
		free(tmprepodir);

	return rv;
}
