/*-
 * Copyright (c) 2012-2019 Juan Romero Pardines.
 * Copyright (c) 2019-2020 Piotr Wójcik.
 * Copyright (c) 2026 Alex March.
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

#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xbps.h>
#include "defs.h"

/*
 * Removes the entry for pkgname from dict, only if it matches
 * pkgver and arch exactly.
 */
static bool
purge_entry(xbps_dictionary_t dict, const char *pkgname, const char *pkgver,
	const char *arch)
{
	xbps_dictionary_t pkgd;
	const char *opkgver = NULL, *oarch = NULL;

	pkgd = xbps_dictionary_get(dict, pkgname);
	if (!pkgd)
		return false;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &opkgver);
	xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &oarch);
	if (!opkgver || !oarch || strcmp(opkgver, pkgver) != 0 ||
	    strcmp(oarch, arch) != 0)
		return false;

	xbps_dictionary_remove(dict, pkgname);
	return true;
}

static int
index_purge_pkg(struct xbps_handle *xhp, xbps_dictionary_t index, xbps_dictionary_t stage,
		const char *file)
{
	char pkgname[XBPS_NAME_SIZE];
	char *pkgver, *arch;
	bool purged = false;
	int r = 0;

	/*
	 * Take package properties from the file name, the binary
	 * package is not read and does not need to exist on disk.
	 */
	pkgver = xbps_binpkg_pkgver(file);
	arch = xbps_binpkg_arch(file);
	if (!pkgver || !arch || !xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		xbps_error_printf("index: `%s' is not a binary package path\n", file);
		r = -EINVAL;
		goto out;
	}
	if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
		fprintf(stderr, "index: ignoring %s, unmatched arch (%s)\n", pkgver, arch);
		goto out;
	}

	if (purge_entry(stage, pkgname, pkgver, arch)) {
		printf("stage: purged `%s' (%s).\n", pkgver, arch);
		purged = true;
	}
	if (purge_entry(index, pkgname, pkgver, arch)) {
		printf("index: purged `%s' (%s).\n", pkgver, arch);
		purged = true;
	}
	if (!purged)
		fprintf(stderr, "index: skipping `%s' (%s), not registered.\n", pkgver, arch);

out:
	free(pkgver);
	free(arch);
	return r;
}

/*
 * Removes the specified binary packages from repository's index and stage.
 */
int
index_purge(struct xbps_handle *xhp, int args, int argc, char **argv, const char *compression)
{
	xbps_dictionary_t index, stage;
	struct xbps_repo *repo;
	char *tmprepodir = NULL, *repodir = NULL;
	int lockfd;
	int r = 0;
	const char *repoarch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;

	if ((tmprepodir = strdup(argv[args])) == NULL)
		return EXIT_FAILURE;
	repodir = dirname(tmprepodir);

	lockfd = xbps_repo_lock(repodir, repoarch);
	if (lockfd < 0) {
		xbps_error_printf("xbps-rindex: cannot lock repository "
		    "%s: %s\n", repodir, strerror(-lockfd));
		free(tmprepodir);
		return EXIT_FAILURE;
	}

	repo = xbps_repo_open(xhp, repodir);
	if (!repo) {
		r = -errno;
		xbps_repo_unlock(repodir, repoarch, lockfd);
		free(tmprepodir);
		if (r == -ENOENT)
			return EXIT_SUCCESS;
		xbps_error_printf("cannot read repository data: %s\n", strerror(-r));
		return EXIT_FAILURE;
	}

	index = xbps_dictionary_copy_mutable(repo->index);
	stage = xbps_dictionary_copy_mutable(repo->stage);

	for (int i = args; i < argc; i++) {
		r = index_purge_pkg(xhp, index, stage, argv[i]);
		if (r < 0)
			goto out;
	}

	if (xbps_dictionary_equals(index, repo->index) &&
	    xbps_dictionary_equals(stage, repo->stage))
		goto out;

	r = repodata_flush(repodir, repoarch, index, stage, repo->idxmeta, compression);
	if (r < 0) {
		xbps_error_printf("failed to write repodata: %s\n", strerror(-r));
		goto out;
	}
	printf("stage: %u packages registered.\n", xbps_dictionary_count(stage));
	printf("index: %u packages registered.\n", xbps_dictionary_count(index));

out:
	xbps_object_release(index);
	xbps_object_release(stage);
	xbps_repo_release(repo);
	xbps_repo_unlock(repodir, repoarch, lockfd);
	free(tmprepodir);
	return r < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
