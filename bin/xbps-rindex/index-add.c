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

static int
set_build_date(const xbps_dictionary_t pkgd, time_t timestamp)
{
	char outstr[64];
	struct tm tmp;

	if (!localtime_r(&timestamp, &tmp))
		return -1;

	if (strftime(outstr, sizeof(outstr)-1, "%F %R %Z", &tmp) == 0)
		return -1;

	if (!xbps_dictionary_set_cstring(pkgd, "build-date", outstr))
		return -1;
	return 0;
}

static bool
repodata_commit(struct xbps_handle *xhp, const char *repodir,
	xbps_dictionary_t idx, xbps_dictionary_t meta, xbps_dictionary_t stage) {
	const char *pkgname;
	xbps_object_iterator_t iter;
	xbps_object_t keysym;
	int rv;

	if(xbps_dictionary_count(stage) == 0) {
		// Nothing to do.
		return true;
	}

	/*
	 * Find old shlibs-provides
	 */
	xbps_dictionary_t oldshlibs = xbps_dictionary_create();
	xbps_dictionary_t usedshlibs = xbps_dictionary_create();

	iter = xbps_dictionary_iterator(stage);
	while ((keysym = xbps_object_iterator_next(iter))) {
		pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t oldpkg = xbps_dictionary_get(idx, pkgname);

		xbps_array_t pkgshlibs = xbps_dictionary_get(oldpkg, "shlib-provides");
		for(unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			xbps_dictionary_set_cstring(oldshlibs, shlib, pkgname);
		}
	}
	xbps_object_iterator_release(iter);

	/*
	 * throw away all unused shlibs
	 */
	iter = xbps_dictionary_iterator(idx);
	while ((keysym = xbps_object_iterator_next(iter))) {
		pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg = xbps_dictionary_get(stage, pkgname);
		if(!pkg)
			pkg = xbps_dictionary_get_keysym(idx, keysym);
		xbps_array_t pkgshlibs = xbps_dictionary_get(pkg, "shlib-requires");

		for(unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL, *user = NULL;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			xbps_dictionary_get_cstring_nocopy(pkg, shlib, &user);
			if(!user)
				continue;
			xbps_array_t users = xbps_dictionary_get(usedshlibs, shlib);
			if(!users) {
				users = xbps_array_create();
				xbps_dictionary_set(usedshlibs, shlib, users);
			}
			xbps_array_add_cstring(users, user);
		}
	}
	xbps_object_iterator_release(iter);

	/*
	 * purge all packages that are fullfilled by the stage
	 */
	iter = xbps_dictionary_iterator(stage);
	while ((keysym = xbps_object_iterator_next(iter))) {
		pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t newpkg = xbps_dictionary_get(idx, pkgname);

		xbps_array_t pkgshlibs = xbps_dictionary_get(newpkg, "shlib-provides");
		for(unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib;
			xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib);
			xbps_dictionary_remove(usedshlibs, shlib);
		}
	}
	xbps_object_iterator_release(iter);

	if(xbps_dictionary_count(usedshlibs) != 0) {
		puts("Unfullfilled shlibs:");
		iter = xbps_dictionary_iterator(usedshlibs);
		while ((keysym = xbps_object_iterator_next(iter))) {
			const char *shlib = xbps_dictionary_keysym_cstring_nocopy(keysym), *provider = NULL;
			xbps_array_t users = xbps_dictionary_get(usedshlibs, shlib);
			xbps_dictionary_get_cstring_nocopy(usedshlibs, shlib, &provider);

			printf(" %s (provided by: %s; used by: ", shlib, provider);
			const char *pre = "";
			for(unsigned int i = 0; i < xbps_array_count(users); i++) {
				const char *user;
				xbps_array_get_cstring_nocopy(users, i, &user);
				xbps_dictionary_remove(usedshlibs, shlib);
				printf("%s%s",pre, user);
				pre = ", ";
			}
			puts(")");
		}
		puts("Changes are commit to stage.");
		xbps_object_iterator_release(iter);
		// TODO FLUSH STAGE
		rv = true;
	}
	else {
		iter = xbps_dictionary_iterator(stage);
		while ((keysym = xbps_object_iterator_next(iter))) {
			pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
			xbps_dictionary_t newpkg = xbps_dictionary_get_keysym(stage, keysym);
			xbps_dictionary_set(idx, pkgname, newpkg);
		}
		xbps_object_iterator_release(iter);
		rv = repodata_flush(xhp, repodir, idx, meta);
	}
	xbps_object_release(usedshlibs);
	xbps_object_release(oldshlibs);
	return rv;
}

int
index_add(struct xbps_handle *xhp, int args, int argmax, char **argv, bool force)
{
	xbps_dictionary_t idx, idxmeta, idxstage, binpkgd, curpkgd;
	struct xbps_repo *repo = NULL;
	struct stat st;
	char *tmprepodir = NULL, *repodir = NULL, *rlockfname = NULL;
	int rv = 0, ret = 0, rlockfd = -1;

	assert(argv);
	/*
	 * Read the repository data or create index dictionaries otherwise.
	 */
	if ((tmprepodir = strdup(argv[args])) == NULL)
		return ENOMEM;

	repodir = dirname(tmprepodir);
	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		fprintf(stderr, "xbps-rindex: cannot lock repository "
		    "%s: %s\n", repodir, strerror(errno));
		rv = -1;
		goto out;
	}
	repo = xbps_repo_public_open(xhp, repodir);
	if (repo == NULL && errno != ENOENT) {
		fprintf(stderr, "xbps-rindex: cannot open/lock repository "
		    "%s: %s\n", repodir, strerror(errno));
		rv = -1;
		goto out;
	}
	if (repo) {
		idx = xbps_dictionary_copy_mutable(repo->idx);
		idxmeta = xbps_dictionary_copy_mutable(repo->idxmeta);
	} else {
		idx = xbps_dictionary_create();
		idxmeta = NULL;
	}
	// TODO: load data from stage repository
	idxstage = xbps_dictionary_create();
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
		binpkgd = xbps_archive_fetch_plist(pkg, "/props.plist");
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
		curpkgd = xbps_dictionary_get(idxstage, pkgname);
		if(curpkgd == NULL)
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
		if (set_build_date(binpkgd, st.st_mtime) < 0) {
			free(pkgver);
			free(pkgname);
			rv = EINVAL;
			goto out;
		}
		/* Remove unneeded objects */
		xbps_dictionary_remove(binpkgd, "pkgname");
		xbps_dictionary_remove(binpkgd, "version");
		xbps_dictionary_remove(binpkgd, "packaged-with");

		/*
		 * Add new pkg dictionary into the index.
		 */
		if (!xbps_dictionary_set(idxstage, pkgname, binpkgd)) {
			free(pkgname);
			free(pkgver);
			rv = EINVAL;
			goto out;
		}
		printf("index: added `%s' (%s).\n", pkgver, arch);
		xbps_object_release(binpkgd);
		free(pkgname);
		free(pkgver);
	}
	/*
	 * Generate repository data files.
	 */
	if (!repodata_commit(xhp, repodir, idx, idxmeta, idxstage)) {
		fprintf(stderr, "%s: failed to write repodata: %s\n",
				_XBPS_RINDEX, strerror(errno));
		goto out;
	}
	printf("index: %u packages registered.\n", xbps_dictionary_count(idx));

out:
	if (repo)
		xbps_repo_close(repo);

	xbps_repo_unlock(rlockfd, rlockfname);

	if (tmprepodir)
		free(tmprepodir);

	return rv;
}
