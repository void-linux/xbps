/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
 * Copyright (c) 2025 Duncan Overbruck <mail@duncano.de>.
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

static void __attribute__((__noreturn__))
unreachable_log_abort(const char *file, size_t line)
{
	xbps_error_printf("%s:%zu: code should not be reached\n", file, line);
	abort();
}

#define unreachable() unreachable_log_abort(__FILE__, __LINE__)

struct repo {
	char path[PATH_MAX];
	char tmp[PATH_MAX];
	int lockfd;
	bool changed;
	const char *repodir;
	const char *arch;
	xbps_dictionary_t index;
	xbps_dictionary_t stage;
	xbps_dictionary_t meta;
	xbps_array_t added;
};

struct shared_state {
	struct repo *repos;
	unsigned int nrepos;

	xbps_dictionary_t old_shlibs;
	xbps_dictionary_t used_shlibs;
};

static int
add_old_shlibs(struct shared_state *state, struct repo *repo)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(repo->stage);
	if (!iter)
		return xbps_error_oom();

	// record all shlibs from the old version of the staged package
	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t oldpkg;
		xbps_array_t oldshlibs;

		oldpkg = xbps_dictionary_get(repo->index, pkgname);
		if (!oldpkg)
			continue;

		oldshlibs = xbps_dictionary_get(oldpkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(oldshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(oldshlibs, i, &shlib))
				unreachable();
			if (!xbps_dictionary_set_cstring(state->old_shlibs, shlib, pkgname)) {
				xbps_object_iterator_release(iter);
				return xbps_error_oom();
			}
		}
	}
	xbps_object_iterator_reset(iter);

	// remove all shlibs that are still provided by a staged package
	while ((keysym = xbps_object_iterator_next(iter))) {
		// const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t newpkg;
		xbps_array_t newshlibs;

		newpkg = xbps_dictionary_get_keysym(repo->stage, keysym);
		if (!newpkg)
			unreachable();

		newshlibs = xbps_dictionary_get(newpkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(newshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(newshlibs, i, &shlib))
				unreachable();
			xbps_dictionary_remove(state->old_shlibs, shlib);
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

	assert(dict);

	array = xbps_dictionary_get(dict, key);
	if (!array) {
		array = xbps_array_create();
		if (!array)
			return xbps_error_oom();
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
find_used_staged_shlibs(struct shared_state *state, struct repo *repo)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;
	int r;

	iter = xbps_dictionary_iterator(repo->index);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
		xbps_dictionary_t pkg;
		xbps_array_t shlibreqs;

		pkg = xbps_dictionary_get(repo->stage, pkgname);
		if (!pkg)
			pkg = xbps_dictionary_get_keysym(repo->index, keysym);

		shlibreqs = xbps_dictionary_get(pkg, "shlib-requires");
		for (unsigned int i = 0; i < xbps_array_count(shlibreqs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(shlibreqs, i, &shlib))
				unreachable();
			if (!xbps_dictionary_get(state->old_shlibs, shlib))
				continue;
			r = dictionary_array_add_cstring(state->used_shlibs, shlib, pkgname);
			if (r < 0) {
				xbps_object_iterator_release(iter);
				return r;
			}
		}
	}
	xbps_object_iterator_release(iter);
	return 0;
}

static int UNUSED
purge_satisfied_by_index(struct shared_state *state, struct repo *repo)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(repo->index);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(repo->index, keysym);
		xbps_array_t pkgshlibs;
		const char *pkgname =  xbps_dictionary_keysym_cstring_nocopy(keysym);

		if (xbps_dictionary_get(repo->stage, pkgname))
			continue;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib))
				unreachable();
			xbps_dictionary_remove(state->used_shlibs, shlib);
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int UNUSED
purge_satisfied_by_stage(struct shared_state *state, struct repo *repo)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(repo->stage);
	if (!iter)
		return xbps_error_oom();

	while ((keysym = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkg = xbps_dictionary_get_keysym(repo->stage, keysym);
		xbps_array_t pkgshlibs;

		pkgshlibs = xbps_dictionary_get(pkg, "shlib-provides");
		for (unsigned int i = 0; i < xbps_array_count(pkgshlibs); i++) {
			const char *shlib = NULL;
			if (!xbps_array_get_cstring_nocopy(pkgshlibs, i, &shlib))
				unreachable();
			xbps_dictionary_remove(state->used_shlibs, shlib);
		}
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
repo_add_pkg(struct repo *repo, const char *file, bool force)
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

	if (strcmp(repo->arch, arch) != 0 && strcmp("noarch", arch) != 0) {
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
	curpkgd = xbps_dictionary_get(repo->stage, pkgname);
	if (!curpkgd)
		curpkgd = xbps_dictionary_get(repo->index, pkgname);

	if (curpkgd && !force) {
		const char *opkgver = NULL, *oarch = NULL;
		int cmp;

		if (!xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &opkgver))
			unreachable();
		if (!xbps_dictionary_get_cstring_nocopy(curpkgd, "architecture", &oarch))
			unreachable();

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
			xbps_warn_printf(
			    "%s: skipping `%s' (%s), already registered.\n",
			    repo->repodir, pkgver, arch);
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
	if (!xbps_dictionary_set(repo->stage, pkgname, binpkgd)) {
		r = xbps_error_oom();
		goto err;
	}

	repo->changed = true;
	if (!xbps_array_add_cstring(repo->added, pkgname)) {
		r = xbps_error_oom();
		goto err;
	}

	r = 0;
err:
	xbps_object_release(binpkgd);
	return r;
}

static const char *
safe_dirname(char *dst, size_t dstsz, const char *path)
{
	if (strlcpy(dst, path, dstsz) >= dstsz) {
		errno = ENOBUFS;
		return NULL;
	}
	return dirname(dst);
}

static int
find_repo(struct repo *repos, unsigned int nrepos, const char *file, struct repo **outp)
{
	char tmp[PATH_MAX];
	const char *dir;
	int r;

	dir = safe_dirname(tmp, sizeof(tmp), file);
	if (!dir) {
		r = -errno;
		xbps_error_printf("failed to get directory from path: %s: %s\n",
		    file, strerror(-r));
		return r;
	}

	for (unsigned i = 0; i < nrepos; i++) {
		if (strcmp(repos[i].repodir, dir) == 0) {
			*outp = &repos[i];
			return 1;
		}
	}

	*outp = NULL;
	xbps_error_printf("repository not found: %s\n", dir);
	return -ENOENT;
}

static int
repo_open(struct xbps_handle *xhp, struct repo *state, const char *repodir, const char *arch)
{
	struct xbps_repo *src;
	int r;

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

	src = xbps_repo_open(xhp, repodir);
	if (src) {
		state->index = xbps_dictionary_copy_mutable(src->index);
		if (!state->index) {
			r = xbps_error_oom();
			goto err;
		}
		state->stage = xbps_dictionary_copy_mutable(src->stage);
		if (!state->stage) {
			r = xbps_error_oom();
			goto err;
		}
		if (src->idxmeta) {
			state->meta = xbps_dictionary_copy_mutable(src->idxmeta);
			if (!state->meta) {
				r = xbps_error_oom();
				goto err;
			}
		}
		xbps_repo_release(src);
	} else {
		if (errno != ENOENT)
			return -errno;
		state->index = xbps_dictionary_create();
		if (!state->index)
			return xbps_error_oom();
		state->stage = xbps_dictionary_create();
		if (!state->stage)
			return xbps_error_oom();
		state->meta = NULL;
	}
	return 0;
err:
	xbps_repo_release(src);
	return r;
}

static void
repo_state_release(struct repo *repo)
{
	if (repo->index)
		xbps_object_release(repo->index);
	if (repo->stage)
		xbps_object_release(repo->stage);
	if (repo->meta)
		xbps_object_release(repo->meta);
	if (repo->added)
		xbps_object_release(repo->added);
	xbps_repo_unlock(repo->repodir, repo->arch, repo->lockfd);
}

static int
print_inconsistent_shlibs(struct shared_state *state)
{
	xbps_object_iterator_t iter;
	xbps_object_t keysym;

	iter = xbps_dictionary_iterator(state->used_shlibs);
	if (!iter)
		return xbps_error_oom();

	printf("Inconsistent shlibs:\n");
	while ((keysym = xbps_object_iterator_next(iter))) {
		const char *shlib = xbps_dictionary_keysym_cstring_nocopy(keysym);
		const char *provider = NULL;
		xbps_array_t users;

		if (!xbps_dictionary_get_cstring_nocopy(state->old_shlibs, shlib, &provider))
			unreachable();

		users = xbps_dictionary_get(state->used_shlibs, shlib);
		printf("  %s (provided by: %s; used by: ", shlib, provider);
		for (unsigned int i = 0; i < xbps_array_count(users); i++) {
			const char *user = NULL;
			if (!xbps_array_get_cstring_nocopy(users, i, &user))
				unreachable();
			printf("%s%s", i > 0 ? ", " : "", user);
		}
		printf(")\n");
	}

	xbps_object_iterator_release(iter);
	return 0;
}

static int
print_staged_packages(struct repo *repos, unsigned nrepos)
{
	for (unsigned int i = 0; i < nrepos; i++) {
		const struct repo *state = &repos[i];

		for (unsigned int j = 0; j < xbps_array_count(state->added); j++) {
			xbps_dictionary_t pkg;
			const char *pkgname = NULL, *pkgver = NULL, *arch = NULL;
			if (!xbps_array_get_cstring_nocopy(state->added, j, &pkgname))
				unreachable();
			if (!xbps_dictionary_get_dict(state->stage, pkgname, &pkg))
				unreachable();
			if (!xbps_dictionary_get_cstring_nocopy(pkg, "pkgver", &pkgver))
				unreachable();
			if (!xbps_dictionary_get_cstring_nocopy(pkg, "architecture", &arch))
				unreachable();
			printf("%s: stage: added `%s' (%s).\n", state->repodir, pkgver, arch);
		}
	}

	return 0;
}

static int
repo_unstage(struct repo *state)
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
			unreachable();
		if (!xbps_dictionary_get_cstring_nocopy(pkg, "architecture", &arch))
			unreachable();

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
repos_check_stage(struct repo *repos, unsigned nrepos)
{
	struct shared_state state = {
		.repos = repos,
		.nrepos = nrepos,
	};
	int r;

	state.old_shlibs = xbps_dictionary_create();
	if (!state.old_shlibs)
		return xbps_error_oom();

	state.used_shlibs = xbps_dictionary_create();
	if (!state.used_shlibs) {
		xbps_object_release(state.old_shlibs);
		return xbps_error_oom();
	}

	// collect all the used shared libraries in the stage
	for (unsigned i = 0; i < nrepos; i++) {
		r = add_old_shlibs(&state, &repos[i]);
		if (r < 0)
			goto err;
	}
	for (unsigned i = 0; i < nrepos; i++) {
		r = find_used_staged_shlibs(&state, &repos[i]);
		if (r < 0)
			goto err;
	}

	// throw out shared libraries that are already satisfied
	for (unsigned i = 0; i < nrepos; i++) {
		r = purge_satisfied_by_index(&state, &repos[i]);
		if (r < 0)
			goto err;
		r = purge_satisfied_by_stage(&state, &repos[i]);
		if (r < 0)
			goto err;
	}

	// ... now if there are libraries left, there is an inconsistency
	if (xbps_dictionary_count(state.used_shlibs) != 0) {
		r = print_inconsistent_shlibs(&state);
		if (r < 0)
			goto err;
		r = print_staged_packages(repos, nrepos);
		if (r < 0)
			goto err;
	} else {
		for (unsigned i = 0; i < nrepos; i++) {
			r = repo_unstage(&repos[i]);
			if (r < 0)
				goto err;
		}
	}

	for (unsigned i = 0; i < nrepos; i++) {
		struct repo *repo = &repos[i];
		if (!repo->changed)
			continue;
		printf("%s: index: %u packages registered.\n",
		    repo->repodir, xbps_dictionary_count(repo->index));
		if (xbps_dictionary_count(repo->stage) != 0) {
			printf("%s: stage: %u packages registered.\n",
			    repo->repodir, xbps_dictionary_count(repo->stage));
		}
	}

	r = 0;
err:
	xbps_object_release(state.old_shlibs);
	xbps_object_release(state.used_shlibs);
	return r;
}

static xbps_array_t
repos_from_argv(int argc, char **argv)
{
	char tmp[PATH_MAX];
	xbps_array_t res;

	res = xbps_array_create();
	if (!res) {
		errno = -xbps_error_oom();
		return NULL;
	}

	for (int i = 0; i < argc; i++) {
		const char *dir;
		int r;
		dir = safe_dirname(tmp, sizeof(tmp), argv[0]);
		if (!dir) {
			r = -errno;
			xbps_error_printf(
			    "failed to get dirname: %s: %s\n", argv[0], strerror(-r));
			xbps_object_release(res);
			errno = -r;
			return NULL;
		}
		if (xbps_match_string_in_array(res, dir))
			continue;
		if (!xbps_array_add_cstring(res, dir)) {
			xbps_object_release(res);
			errno = -xbps_error_oom();
			return NULL;
		}
	}

	return res;
}

int
index_add(struct xbps_handle *xhp, int argc, char **argv, bool force, const char *compression, xbps_array_t repo_args)
{
	const char *arch = xhp->target_arch ? xhp->target_arch : xhp->native_arch;
	struct repo *repos;
	unsigned nrepos;
	int r;

	// Backwards compatibiltiy if no repo args are given, in this case
	// add the repos based on the supplied packages...
	if (!repo_args) {
		repo_args = repos_from_argv(argc, argv);
		if (!repo_args)
			return EXIT_FAILURE;
	}

	nrepos = xbps_array_count(repo_args);
	repos = calloc(nrepos, sizeof(*repos));
	if (!repos) {
		xbps_error_oom();
		return EXIT_FAILURE;
	}

	for (unsigned i = 0; i < nrepos; i++) {
		const char *repodir = NULL;
		if (!xbps_array_get_cstring_nocopy(repo_args, i, &repodir))
			unreachable();
		r = repo_open(xhp, &repos[i], repodir, arch);
		if (r < 0)
			goto err;
	}

	for (int i = 0; i < argc; i++) {
		struct repo *repo = NULL;
		r = find_repo(repos, nrepos, argv[i], &repo);
		if (r < 0)
			goto err;
		r = repo_add_pkg(repo, argv[i], force);
		if (r < 0)
			goto err;
	}

	r = repos_check_stage(repos, nrepos);
	if (r < 0)
		goto err;

	// write all changed repodata's to tempfiles
	for (unsigned i = 0; i < nrepos; i++) {
		struct repo *repo = &repos[i];
		if (!repo->changed)
			continue;
		r = repodata_write_tmpfile(repo->path, sizeof(repo->path),
		    repo->tmp, sizeof(repo->tmp), repo->repodir, repo->arch,
		    repo->index, repo->stage, repo->meta, compression);
		if (r < 0)
			goto err;
	}

	// rename all changed repodata's tempfiles
	for (unsigned i = 0; i < nrepos; i++) {
		struct repo *repo = &repos[i];
		if (!repo->changed)
			continue;
		if (rename(repo->tmp, repo->path) == -2) {
			xbps_error_printf("failed to rename tempfile: %s: %s: %s\n",
			    repo->tmp, repo->path, strerror(-errno));
			// XXX: maybe better to abort here, but either way
			// we'll end up with inconsistent staging...
		}

	}

	for (unsigned i = 0; i < nrepos; i++)
		repo_state_release(&repos[i]);
	free(repos);

	return EXIT_SUCCESS;

err:
	for (unsigned i = 0; i < nrepos; i++) {
		if (repos[i].tmp[0] != 0)
			unlink(repos[i].tmp);
		repo_state_release(&repos[i]);
	}
	return EXIT_FAILURE;
}
