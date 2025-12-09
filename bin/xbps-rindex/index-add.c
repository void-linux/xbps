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
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbps.h>
#include "defs.h"

struct repo_state {
	int lockfd;
	bool changed;
	const char *repodir;
	const char *arch;
	struct xbps_repo *repo;
	xbps_dictionary_t index;
	xbps_dictionary_t stage;
	xbps_dictionary_t meta;
	xbps_array_t added;
};

struct shared_state {
	xbps_dictionary_t staged_shlibs;
	xbps_dictionary_t used_shlibs;
};

static int
add_staged_shlib_providers(struct shared_state *shared, struct repo_state *state)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(state->stage);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg = xbps_dictionary_get(state->index, pkgname);
		xbps_array_t pkgshlibs;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib))
				abort();
			// fprintf(stderr, ">> %s = %s (%s)\n", shlib, pkgname, state->repodir);
			if (!xbps_dictionary_set_cstring(shared->staged_shlibs, shlib, pkgname)) {
				xbps_object_iterator_release(iter);
				return xbps_error_oom();
			}
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
dictionary_array_add_cstring(xbps_dictionary_t dict, const char *key, const char *value)
{
	xbps_array_t array;
	bool alloc = false;
	int r = 0;

	array = xbps_dictionary_get(dict, key);
	if (!array) {
		array = xbps_array_create();
		if (!xbps_dictionary_set(dict, key, array)) {
			xbps_object_release(array);
			return xbps_error_oom();
		}
		alloc = true;
	}
	if (!xbps_array_add_cstring(array, value))
		r = xbps_error_oom();
	if (alloc)
		xbps_object_release(array);
	return r;
}

static int
find_used_staged_shlibs(struct shared_state *shared, struct repo_state *state)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;
	int r;

	iter = xbps_dictionary_iterator(state->index);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg;
		xbps_array_t pkgshlibs;

		pkg = xbps_dictionary_get(state->stage, pkgname);
		if (!pkg)
			pkg = xbps_dictionary_get_keysym(state->index, keysym);

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-requires");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib))
				abort();
			if (!xbps_dictionary_get(shared->staged_shlibs, shlib))
				continue;
			r = dictionary_array_add_cstring(shared->used_shlibs, shlib, pkgname);
			if (r < 0) {
				xbps_object_iterator_release(iter);
				return r;
			}
		}
	}
	xbps_object_iterator_release(iter);
	return 0;
}

static int
purge_satisfied_by_index(struct shared_state *shared, struct repo_state *state)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(state->index);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(state->index, keysym);
		xbps_array_t pkgshlibs;
		const char *pkgname =  xbps_dictionary_keysym_cstring_nocopy(keysym);

		if (xbps_dictionary_get(state->stage, pkgname))
			continue;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib))
				abort();
			xbps_dictionary_remove(shared->used_shlibs, shlib);
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
purge_satisfied_by_stage(struct shared_state *shared, struct repo_state *state)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(state->stage);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(state->stage, keysym);
		xbps_array_t pkgshlibs;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib))
				abort();
			xbps_dictionary_remove(shared->used_shlibs, shlib);
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
index_add_pkg(struct repo_state *state, const char *file, bool force)
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

	if (!xbps_dictionary_get_cstring_nocopy(binpkgd, "architecture", &arch)) {
		xbps_error_printf(
		    "invalid binary packages: %s: missing property: "
		    "architecture\n",
		    file);
		return -EINVAL;
	}
	if (!xbps_dictionary_get_cstring_nocopy(binpkgd, "pkgver", &pkgver)) {
		xbps_error_printf(
		    "invalid binary packages: %s: missing property: pkgver\n",
		    file);
		return -EINVAL;
	}

	if (strcmp(state->arch, arch) != 0 && strcmp("noarch", arch) != 0) {
		xbps_warn_printf("ignoring %s, unmatched arch (%s)\n", pkgver, arch);
		xbps_object_release(binpkgd);
		return 0;
	}

	if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
		xbps_error_printf(
		    "invalid binary package: %s: invalid property: pkgver: "
		    "%s\n", file, pkgver);
		xbps_object_release(binpkgd);
		return -EINVAL;
	}

	/*
	 * Check if this package exists already in the index, but first
	 * checking the version. If current package version is greater
	 * than current registered package, update the index; otherwise
	 * pass to the next one.
	 */
	curpkgd = xbps_dictionary_get(state->stage, pkgname);
	if (!curpkgd)
		curpkgd = xbps_dictionary_get(state->index, pkgname);

	if (curpkgd && !force) {
		const char *opkgver = NULL, *oarch = NULL;
		int cmp;

		if (!xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &opkgver))
			abort();
		if (!xbps_dictionary_get_cstring_nocopy(curpkgd, "architecture", &oarch))
			abort();

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
			xbps_object_release(binpkgd);
			return 0;
		}
	}

	if (!xbps_file_sha256(sha256, sizeof(sha256), file)) {
		r = -errno;
		xbps_error_printf("failed to checksum binary package: %s: %s\n", file, strerror(-r));
		goto err;
	}
	if (!xbps_dictionary_set_cstring(binpkgd, "filename-sha256", sha256)) {
		r = xbps_error_oom();
		goto err;
	}
	if (stat(file, &st) == -1) {
		r = -errno;
		xbps_error_printf("failed to stat binary package: %s: %s\n", file, strerror(-r));
		goto err;
	}
	if (!xbps_dictionary_set_uint64(binpkgd, "filename-size", (uint64_t)st.st_size)) {
		r = xbps_error_oom();
		goto err;
	}

	xbps_dictionary_remove(binpkgd, "pkgname");
	xbps_dictionary_remove(binpkgd, "version");
	xbps_dictionary_remove(binpkgd, "packaged-with");

	/*
	 * Add new pkg dictionary into the stage index
	 */
	if (!xbps_dictionary_set(state->stage, pkgname, binpkgd)) {
		r = xbps_error_oom();
		goto err;
	}

	state->changed = true;
	if (!xbps_array_add_cstring(state->added, pkgname)) {
		r = xbps_error_oom();
		goto err;
	}

	r = 0;
err:
	xbps_object_release(binpkgd);
	return r;
}

static int
repo_add_package(
    struct repo_state *states, unsigned nstates, const char *file, bool force)
{
	char tmp[PATH_MAX];
	const char *dir;
	int r;

	if (strlcpy(tmp, file, sizeof(tmp)) >= sizeof(tmp)) {
		xbps_error_printf(
		    "failed to copy path: %s: %s\n", file, strerror(ENOBUFS));
		return -ENOBUFS;
	}

	dir = dirname(tmp);
	if (!dir) {
		r = -errno;
		xbps_error_printf("failed to get directory from path: %s: %s\n",
		    file, strerror(-r));
		return r;
	}

	for (unsigned i = 0; i < nstates; i++) {
		struct repo_state *state = &states[i];
		if (strcmp(state->repodir, dir) != 0)
			continue;
		r = index_add_pkg(state, file, force);
		if (r < 0)
			return r;
		return 0;
	}

	xbps_error_printf("repository not in repository list: %s\n", dir);
	return -ENODEV;
}

static int
repo_state_open(struct xbps_handle *xhp, struct repo_state *state, const char *repodir, const char *arch)
{
	state->changed = false;
	state->repodir = repodir;
	state->arch = arch;
	state->lockfd = xbps_repo_lock(state->repodir, arch);
	if (state->lockfd < 0) {
		xbps_error_printf(
		    "failed to lock repository: %s: %s\n",
		    state->repodir, strerror(-state->lockfd));
		return -state->lockfd;
	}

	state->added = xbps_array_create();
	if (!state->added)
		return xbps_error_oom();

	state->repo = xbps_repo_open(xhp, repodir);
	if (!state->repo) {
		if (errno != ENOENT)
			return -errno;
		state->index = xbps_dictionary_create();
		if (!state->index)
			return xbps_error_oom();
		state->stage = xbps_dictionary_create();
		if (!state->stage)
			return xbps_error_oom();
		state->meta = NULL;
	} else {
		state->index = xbps_dictionary_copy_mutable(state->repo->index);
		if (!state->index)
			return xbps_error_oom();
		state->stage = xbps_dictionary_copy_mutable(state->repo->stage);
		if (!state->stage)
			return xbps_error_oom();
		if (state->repo->idxmeta) {
			state->meta = xbps_dictionary_copy_mutable(state->repo->idxmeta);
			if (!state->meta)
				return xbps_error_oom();
		}
	}
	return 0;
}

static void
repo_state_release(struct repo_state *state)
{
	if (state->index)
		xbps_object_release(state->index);
	if (state->stage)
		xbps_object_release(state->stage);
	if (state->meta)
		xbps_object_release(state->meta);
	xbps_repo_release(state->repo);
	xbps_repo_unlock(state->repodir, state->arch, state->lockfd);
}

static int
print_inconsistent_shlibs(struct shared_state *shared)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(shared->used_shlibs);
	if (!iter)
		return xbps_error_oom();

	printf("Inconsistent shlibs:\n");
	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *shlib = xbps_dictionary_keysym_cstring_nocopy(keysym);
		const char *provider = NULL;
		xbps_array_t users;

		if (!xbps_dictionary_get_cstring_nocopy(shared->staged_shlibs, shlib, &provider))
			abort();

		users = xbps_dictionary_get(shared->used_shlibs, shlib);
		printf("  %s (provided by: %s; used by: ", shlib, provider);
		for (unsigned int i = 0; i < xbps_array_count(users); i++) {
			const char *user = NULL;
			if (!xbps_array_get_cstring_nocopy(users, i, &user))
				abort();
			printf("%s%s", i > 0 ? ", " : "", user);
		}
		printf(")\n");
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
print_staged_packages(struct repo_state *states, unsigned nstates)
{
	for (unsigned int i = 0; i < nstates; i++) {
		const struct repo_state *state = &states[i];

		for (unsigned int j = 0; j < xbps_array_count(state->added); j++) {
			xbps_dictionary_t pkg;
			const char *pkgname = NULL, *pkgver = NULL, *arch = NULL;
			if (!xbps_array_get_cstring_nocopy(state->added, j, &pkgname))
				abort();
			if (!xbps_dictionary_get_dict(state->stage, pkgname, &pkg))
				abort();
			if (!xbps_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver))
				abort();
			if (!xbps_dictionary_get_cstring_nocopy(pkg, "architecture", &arch))
				abort();
			printf("%s: stage: added `%s' (%s)\n", state->repodir, pkgver, arch);
		}
	}

	return 0;
}

static int
unstage(struct repo_state *state)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	if (xbps_dictionary_count(state->stage) == 0)
		return 0;

	iter = xbps_dictionary_iterator(state->stage);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgver = NULL, *arch = NULL;
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(state->stage, keysym);

		if (!xbps_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver))
			abort();
		if (!xbps_dictionary_get_cstring_nocopy(pkg, "architecture", &arch))
			abort();

		printf("%s: index: added `%s' (%s).\n", state->repodir,
		    pkgver, arch);

		if (!xbps_dictionary_set(state->index, pkgname, pkg)) {
			xbps_object_iterator_release(iter);
			return xbps_error_oom();
		}
	}
	xbps_object_iterator_release(iter);

	xbps_object_release(state->stage);
	state->stage = NULL;
	state->changed = true;

	return 0;
}

static int
commit(struct repo_state *states, unsigned nstates)
{
	struct shared_state shared;
	int r;

	shared.staged_shlibs = xbps_dictionary_create();
	if (!shared.staged_shlibs)
		return xbps_error_oom();

	shared.used_shlibs = xbps_dictionary_create();
	if (!shared.used_shlibs) {
		xbps_object_release(shared.staged_shlibs);
		return xbps_error_oom();
	}

	// collect all the used shared libraries in the stage
	for (unsigned i = 0; i < nstates; i++) {
		r = add_staged_shlib_providers(&shared, &states[i]);
		if (r < 0)
			goto err;
		r = find_used_staged_shlibs(&shared, &states[i]);
		if (r < 0)
			goto err;
	}

	// throw out shared libraries that are already satisfied
	for (unsigned i = 0; i < nstates; i++) {
		r = purge_satisfied_by_index(&shared, &states[i]);
		if (r < 0)
			goto err;
		r = purge_satisfied_by_stage(&shared, &states[i]);
		if (r < 0)
			goto err;
	}

	// ... now if there are libraries left, there is an inconsistency
	if (xbps_dictionary_count(shared.used_shlibs) != 0) {
		r = print_inconsistent_shlibs(&shared);
		if (r < 0)
			goto err;
		r = print_staged_packages(states, nstates);
		if (r < 0)
			goto err;
	} else {
		for (unsigned i = 0; i < nstates; i++) {
			r = unstage(&states[i]);
			if (r < 0)
				goto err;
		}
	}

	for (unsigned i = 0; i < nstates; i++) {
		struct repo_state *state = &states[i];
		if (!state->changed)
			continue;
		printf("%s: index: %u packages registered.\n",
		    state->repodir, xbps_dictionary_count(state->index));
		if (xbps_dictionary_count(state->stage) != 0) {
			printf("%s: stage: %u packages registered.\n",
			    state->repodir,
			    xbps_dictionary_count(state->stage));
		}
	}

	r = 0;
err:
	xbps_object_release(shared.staged_shlibs);
	xbps_object_release(shared.used_shlibs);
	return r;
}

static int
repo_write(struct repo_state *state, const char *compression)
{
	int r;

	r = repodata_flush(state->repodir, state->arch, state->index, state->stage, state->meta, compression);
	if (r < 0)
		return r;
	return 0;
}

int
index_add(struct xbps_handle *xhp, int argc, char **argv, bool force, const char *compression, xbps_array_t repos)
{
	const char *arch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;
	struct repo_state *states;
	unsigned nstates;
	int r;

	if (!repos) {
		char tmp[PATH_MAX];
		const char *repodir;
		repos = xbps_array_create();
		if (!repos) {
			xbps_error_oom();
			return EXIT_FAILURE;
		}
		if (strlcpy(tmp, argv[0], sizeof(tmp)) >= sizeof(tmp)) {
			xbps_error_printf("failed to copy path: %s: %s\n", argv[0],
			    strerror(ENOBUFS));
			return EXIT_FAILURE;
		}
		repodir = dirname(tmp);
		if (!repodir) {
			xbps_error_printf("failed to get dirname: %s: %s\n", tmp, strerror(errno));
			xbps_object_release(repos);
			return EXIT_FAILURE;
		}
		if (!xbps_array_add_cstring(repos, repodir)) {
			xbps_error_oom();
			xbps_object_release(repos);
			return EXIT_FAILURE;
		}
	}

	nstates = xbps_array_count(repos);
	states = calloc(nstates, sizeof(*states));
	if (!states) {
		xbps_error_oom();
		return EXIT_FAILURE;
	}

	for (unsigned i = 0; i < nstates; i++) {
		struct repo_state *state = &states[i];
		const char *repodir = NULL;
		if (!xbps_array_get_cstring_nocopy(repos, i, &repodir))
			abort();
		r = repo_state_open(xhp, state, repodir, arch);
		if (r < 0)
			goto err;
	}

	for (int i = 0; i < argc; i++) {
		r = repo_add_package(states, nstates, argv[i], force);
		if (r < 0)
			goto err;
	}

	r = commit(states, nstates);
	if (r < 0)
		goto err;

	for (unsigned i = 0; i < nstates; i++) {
		r = repo_write(&states[i], compression);
		if (r < 0)
			return r;
	}

	for (unsigned i = 0; i < xbps_array_count(repos); i++)
		repo_state_release(&states[i]);
	free(states);

	return EXIT_SUCCESS;

err:
	for (unsigned i = 0; i < xbps_array_count(repos); i++)
		repo_state_release(&states[i]);
	return EXIT_FAILURE;
}
