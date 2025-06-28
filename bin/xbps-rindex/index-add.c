/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbps.h>
#include "defs.h"

static int
repodata_commit(const char *repodir, const char *repoarch,
	xbps_dictionary_t index, xbps_dictionary_t stage, xbps_dictionary_t meta,
	const char *compression)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;
	int r;
	xbps_dictionary_t oldshlibs, usedshlibs;

	if (xbps_dictionary_count(stage) == 0)
		return 0;

	/*
	 * Find old shlibs-provides
	 */
	oldshlibs = xbps_dictionary_create();
	usedshlibs = xbps_dictionary_create();

	iter = xbps_dictionary_iterator(stage);
	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg = xbps_dictionary_get(index, pkgname);
		xbps_array_t pkgshlibs;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			xbps_dictionary_set_cstring(oldshlibs, shlib, pkgname);
		}
	}
	xbps_object_iterator_release(iter);

	/*
	 * throw away all unused shlibs
	 */
	iter = xbps_dictionary_iterator(index);
	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg = xbps_dictionary_get(stage, pkgname);
		xbps_array_t pkgshlibs;
		if (!pkg)
			pkg = xbps_dictionary_get_keysym(index, keysym);
		pkgshlibs = xbps_dictionary_get(pkg, "shlib-requires");

		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			bool alloc = false;
			xbps_array_t users;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			if (!xbps_dictionary_get(oldshlibs, shlib))
				continue;
			users = xbps_dictionary_get(usedshlibs, shlib);
			if (!users) {
				users = xbps_array_create();
				xbps_dictionary_set(usedshlibs, shlib, users);
				alloc = true;
			}
			xbps_array_add_cstring(users, pkgname);
			if (alloc)
				xbps_object_release(users);
		}
	}
	xbps_object_iterator_release(iter);

	/*
	 * purge all packages that are fullfilled by the index and
	 * not in the stage.
	 */
	iter = xbps_dictionary_iterator(index);
	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(index, keysym);
		xbps_array_t pkgshlibs;


		if (xbps_dictionary_get(stage,
					xbps_dictionary_keysym_cstring_nocopy(keysym))) {
			continue;
		}

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			xbps_dictionary_remove(usedshlibs, shlib);
		}
	}
	xbps_object_iterator_release(iter);

	/*
	 * purge all packages that are fullfilled by the stage
	 */
	iter = xbps_dictionary_iterator(stage);
	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(stage, keysym);
		xbps_array_t pkgshlibs;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			xbps_dictionary_remove(usedshlibs, shlib);
		}
	}
	xbps_object_iterator_release(iter);

	if (xbps_dictionary_count(usedshlibs) != 0) {
		printf("Inconsistent shlibs:\n");
		iter = xbps_dictionary_iterator(usedshlibs);
		while ((keysym = xbps_object_iterator_next(iter))) {
			const char *shlib = xbps_dictionary_keysym_cstring_nocopy(keysym),
					*provider = NULL, *pre;
			xbps_array_t users = xbps_dictionary_get(usedshlibs, shlib);
			xbps_dictionary_get_cstring_nocopy(oldshlibs, shlib, &provider);

			printf("  %s (provided by: %s; used by: ", shlib, provider);
			pre = "";
			for (unsigned int i = 0; i < xbps_array_count(users); i++) {
				const char *user = NULL;
				xbps_array_get_cstring_nocopy(users, i, &user);
				printf("%s%s", pre, user);
				pre = ", ";
			}
			printf(")\n");
		}
		xbps_object_iterator_release(iter);
		iter = xbps_dictionary_iterator(stage);
		while ((keysym = xbps_object_iterator_next(iter))) {
			xbps_dictionary_t pkg = xbps_dictionary_get_keysym(stage, keysym);
			const char *pkgver = NULL, *arch = NULL;
			xbps_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver);
			xbps_dictionary_get_cstring_nocopy(pkg, "architecture", &arch);
			printf("stage: added `%s' (%s)\n", pkgver, arch);
		}
		xbps_object_iterator_release(iter);
	} else {
		iter = xbps_dictionary_iterator(stage);
		while ((keysym = xbps_object_iterator_next(iter))) {
			const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
			xbps_dictionary_t pkg = xbps_dictionary_get_keysym(stage, keysym);
			const char *pkgver = NULL, *arch = NULL;
			xbps_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver);
			xbps_dictionary_get_cstring_nocopy(pkg, "architecture", &arch);
			printf("index: added `%s' (%s).\n", pkgver, arch);
			xbps_dictionary_set(index, pkgname, pkg);
		}
		xbps_object_iterator_release(iter);
		stage = NULL;
	}

	r = repodata_flush(repodir, repoarch, index, stage, meta, compression);
	xbps_object_release(usedshlibs);
	xbps_object_release(oldshlibs);
	return r;
}

static int
index_add_pkg(struct xbps_handle *xhp, xbps_dictionary_t index, xbps_dictionary_t stage,
		const char *file, bool force)
{
	char sha256[XBPS_SHA256_SIZE];
	char pkgname[XBPS_NAME_SIZE];
	struct stat st;
	const char *arch = NULL;
	const char *pkgver = NULL;
	xbps_dictionary_t binpkgd, curpkgd;
	int r;

	/*
	 * Read metadata props plist dictionary from binary package.
	 */
	binpkgd = xbps_archive_fetch_plist(file, "/props.plist");
	if (!binpkgd) {
		xbps_error_printf("index: failed to read %s metadata for "
		    "`%s', skipping!\n", XBPS_PKGPROPS, file);
		return 0;
	}
	xbps_dictionary_get_cstring_nocopy(binpkgd, "architecture", &arch);
	xbps_dictionary_get_cstring_nocopy(binpkgd, "pkgver", &pkgver);
	if (!xbps_pkg_arch_match(xhp, arch, NULL)) {
		fprintf(stderr, "index: ignoring %s, unmatched arch (%s)\n", pkgver, arch);
		goto out;
	}
	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		r = -EINVAL;
		goto err;
	}

	/*
	 * Check if this package exists already in the index, but first
	 * checking the version. If current package version is greater
	 * than current registered package, update the index; otherwise
	 * pass to the next one.
	 */
	curpkgd = xbps_dictionary_get(stage, pkgname);
	if (!curpkgd)
		curpkgd = xbps_dictionary_get(index, pkgname);

	if (curpkgd && !force) {
		const char *opkgver = NULL, *oarch = NULL;
		int cmp;

		xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &opkgver);
		xbps_dictionary_get_cstring_nocopy(curpkgd, "architecture", &oarch);

		cmp = xbps_cmpver(pkgver, opkgver);
		if (cmp < 0 && xbps_pkg_reverts(binpkgd, opkgver)) {
			/*
			 * If the considered package reverts the package in the index,
			 * consider the current package as the newer one.
			 */
			cmp = 1;
		} else if (cmp > 0 && xbps_pkg_reverts(curpkgd, pkgver)) {
			/*
			 * If package in the index reverts considered package, consider the
			 * package in the index as the newer one.
			 */
			cmp = -1;
		}
		if (cmp <= 0) {
			fprintf(stderr, "index: skipping `%s' (%s), already registered.\n", pkgver, arch);
			goto out;
		}
	}

	if (!xbps_file_sha256(sha256, sizeof(sha256), file))
		goto err_errno;
	if (!xbps_dictionary_set_cstring(binpkgd, "filename-sha256", sha256))
		goto err_errno;
	if (stat(file, &st) == -1)
		goto err_errno;
	if (!xbps_dictionary_set_uint64(binpkgd, "filename-size", (uint64_t)st.st_size))
		goto err_errno;

	xbps_dictionary_remove(binpkgd, "pkgname");
	xbps_dictionary_remove(binpkgd, "version");
	xbps_dictionary_remove(binpkgd, "packaged-with");

	/*
	 * Add new pkg dictionary into the stage index
	 */
	if (!xbps_dictionary_set(stage, pkgname, binpkgd))
		goto err_errno;

out:
	xbps_object_release(binpkgd);
	return 0;
err_errno:
	r = -errno;
err:
	xbps_object_release(binpkgd);
	return r;
}

int
index_add(struct xbps_handle *xhp, int args, int argc, char **argv, bool force, const char *compression)
{
	xbps_dictionary_t index, stage, meta;
	struct xbps_repo *repo;
	char *tmprepodir = NULL, *repodir = NULL;
	int lockfd;
	int r;
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
	if (!repo && errno != ENOENT) {
		free(tmprepodir);
		return EXIT_FAILURE;
	}

	if (repo) {
		index = xbps_dictionary_copy_mutable(repo->index);
		stage = xbps_dictionary_copy_mutable(repo->stage);
		meta = xbps_dictionary_copy_mutable(repo->idxmeta);
	} else {
		index = xbps_dictionary_create();
		stage = xbps_dictionary_create();
		meta = NULL;
	}

	for (int i = args; i < argc; i++) {
		r = index_add_pkg(xhp, index, stage, argv[i], force);
		if (r < 0)
			goto err2;
	}

	r = repodata_commit(repodir, repoarch, index, stage, meta, compression);
	if (r < 0) {
		xbps_error_printf("failed to write repodata: %s\n", strerror(-r));
		goto err2;
	}
	printf("index: %u packages registered.\n", xbps_dictionary_count(index));

	xbps_object_release(index);
	xbps_object_release(stage);
	if (meta)
		xbps_object_release(meta);
	xbps_repo_release(repo);
	xbps_repo_unlock(repodir, repoarch, lockfd);
	free(tmprepodir);
	return EXIT_SUCCESS;

err2:
	xbps_object_release(index);
	xbps_object_release(stage);
	if (meta)
		xbps_object_release(meta);
	xbps_repo_release(repo);
	xbps_repo_unlock(repodir, repoarch, lockfd);
	free(tmprepodir);
	return EXIT_FAILURE;
}
