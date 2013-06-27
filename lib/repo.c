/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

/**
 * @file lib/repo.c
 * @brief Repository functions
 * @defgroup repo Repository functions
 */
char *
xbps_repo_path(struct xbps_handle *xhp, const char *url)
{
	assert(xhp);
	assert(url);

	return xbps_xasprintf("%s/%s-repodata",
	    url, xhp->target_arch ? xhp->target_arch : xhp->native_arch);
}

struct xbps_repo *
xbps_repo_open(struct xbps_handle *xhp, const char *url)
{
	struct xbps_repo *repo;
	const char *arch;
	char *repofile;

	assert(xhp);
	assert(url);

	if (xhp->target_arch)
		arch = xhp->target_arch;
	else
		arch = xhp->native_arch;

	if (xbps_repository_is_remote(url)) {
		/* remote repository */
		char *rpath;

		if ((rpath = xbps_get_remote_repo_string(url)) == NULL)
			return NULL;
		repofile = xbps_xasprintf("%s/%s/%s-repodata", xhp->metadir, rpath, arch);
		free(rpath);
	} else {
		/* local repository */
		repofile = xbps_repo_path(xhp, url);
	}

	repo = calloc(1, sizeof(struct xbps_repo));
	assert(repo);

	repo->xhp = xhp;
	repo->uri = url;
	repo->ar = archive_read_new();
	archive_read_support_filter_gzip(repo->ar);
	archive_read_support_format_tar(repo->ar);

	if (archive_read_open_filename(repo->ar, repofile, ARCHIVE_READ_BLOCKSIZE)) {
		xbps_dbg_printf(xhp, "cannot open repository file %s: %s\n",
				repofile, strerror(archive_errno(repo->ar)));
		archive_read_free(repo->ar);
		free(repo);
		repo = NULL;
	}
	free(repofile);
	return repo;
}

xbps_dictionary_t
xbps_repo_get_plist(struct xbps_repo *repo, const char *file)
{
	xbps_dictionary_t d;
	struct archive_entry *entry;
	void *buf;
	size_t buflen;
	ssize_t nbytes = -1;
	int rv;

	assert(repo);
	assert(repo->ar);
	assert(file);

	for (;;) {
		rv = archive_read_next_header(repo->ar, &entry);
		if (rv == ARCHIVE_EOF || rv == ARCHIVE_FATAL)
			break;
		else if (rv == ARCHIVE_RETRY)
			continue;
		if (strcmp(archive_entry_pathname(entry), file) == 0) {
			buflen = (size_t)archive_entry_size(entry);
			buf = malloc(buflen);
			assert(buf);
			nbytes = archive_read_data(repo->ar, buf, buflen);
			if ((size_t)nbytes != buflen) {
				free(buf);
				return NULL;
			}
			d = xbps_dictionary_internalize(buf);
			free(buf);
			return d;
		}
		archive_read_data_skip(repo->ar);
	}
	return NULL;
}

void
xbps_repo_close(struct xbps_repo *repo)
{
	assert(repo);

	archive_read_free(repo->ar);
	if (xbps_object_type(repo->idx) == XBPS_TYPE_DICTIONARY)
		xbps_object_release(repo->idx);
	if (xbps_object_type(repo->idxfiles) == XBPS_TYPE_DICTIONARY)
		xbps_object_release(repo->idxfiles);
	free(repo);
}

xbps_dictionary_t
xbps_repo_get_virtualpkg(struct xbps_repo *repo, const char *pkg)
{
	xbps_dictionary_t pkgd;

	assert(repo);
	assert(repo->ar);
	assert(pkg);

	if (xbps_object_type(repo->idx) != XBPS_TYPE_DICTIONARY) {
		repo->idx = xbps_repo_get_plist(repo, XBPS_PKGINDEX);
		assert(repo->idx);
	}
	pkgd = xbps_find_virtualpkg_in_dict(repo->xhp, repo->idx, pkg);
	if (pkgd) {
		xbps_dictionary_set_cstring_nocopy(pkgd,
				"repository", repo->uri);
		return pkgd;
	}
	return NULL;
}

xbps_dictionary_t
xbps_repo_get_pkg(struct xbps_repo *repo, const char *pkg)
{
	xbps_dictionary_t pkgd;

	assert(repo);
	assert(repo->ar);
	assert(pkg);

	if (xbps_object_type(repo->idx) != XBPS_TYPE_DICTIONARY) {
		repo->idx = xbps_repo_get_plist(repo, XBPS_PKGINDEX);
		assert(repo->idx);
	}
	pkgd = xbps_find_pkg_in_dict(repo->idx, pkg);
	if (pkgd) {
		xbps_dictionary_set_cstring_nocopy(pkgd,
				"repository", repo->uri);
		return pkgd;
	}

	return NULL;
}

xbps_dictionary_t
xbps_repo_get_pkg_plist(struct xbps_handle *xhp, xbps_dictionary_t pkgd,
		const char *plist)
{
	xbps_dictionary_t bpkgd;
	char *url;

	url = xbps_repository_pkg_path(xhp, pkgd);
	if (url == NULL)
		return NULL;

	bpkgd = xbps_get_pkg_plist_from_binpkg(url, plist);
	free(url);
	return bpkgd;
}

static xbps_array_t
revdeps_match(struct xbps_repo *repo, xbps_dictionary_t tpkgd, const char *str)
{
	xbps_dictionary_t pkgd;
	xbps_array_t revdeps = NULL, pkgdeps, provides;
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *pkgver, *tpkgver, *arch, *vpkg;
	char *buf;
	unsigned int i;

	iter = xbps_dictionary_iterator(repo->idx);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		pkgd = xbps_dictionary_get_keysym(repo->idx, obj);
		if (xbps_dictionary_equals(pkgd, tpkgd))
			continue;

		pkgdeps = xbps_dictionary_get(pkgd, "run_depends");
		if (!xbps_array_count(pkgdeps))
			continue;
		/*
		 * Try to match passed in string.
		 */
		if (str) {
			if (!xbps_match_pkgdep_in_array(pkgdeps, str))
				continue;
			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (!xbps_pkg_arch_match(repo->xhp, arch, NULL))
				continue;

			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &tpkgver);
			/* match */
			if (revdeps == NULL)
				revdeps = xbps_array_create();

			if (!xbps_match_string_in_array(revdeps, tpkgver))
				xbps_array_add_cstring_nocopy(revdeps, tpkgver);

			continue;
		}
		/*
		 * Try to match any virtual package.
		 */
		provides = xbps_dictionary_get(tpkgd, "provides");
		for (i = 0; i < xbps_array_count(provides); i++) {
			xbps_array_get_cstring_nocopy(provides, i, &vpkg);
			if (strchr(vpkg, '_') == NULL)
				buf = xbps_xasprintf("%s_1", vpkg);
			else
				buf = strdup(vpkg);

			if (!xbps_match_pkgdep_in_array(pkgdeps, buf)) {
				free(buf);
				continue;
			}
			free(buf);
			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (!xbps_pkg_arch_match(repo->xhp, arch, NULL))
				continue;

			xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver",
			    &tpkgver);
			/* match */
			if (revdeps == NULL)
				revdeps = xbps_array_create();

			if (!xbps_match_string_in_array(revdeps, tpkgver))
				xbps_array_add_cstring_nocopy(revdeps, tpkgver);
		}
		/*
		 * Try to match by pkgver.
		 */
		xbps_dictionary_get_cstring_nocopy(tpkgd, "pkgver", &pkgver);
		if (!xbps_match_pkgdep_in_array(pkgdeps, pkgver))
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd,
		    "architecture", &arch);
		if (!xbps_pkg_arch_match(repo->xhp, arch, NULL))
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &tpkgver);
		/* match */
		if (revdeps == NULL)
			revdeps = xbps_array_create();

		if (!xbps_match_string_in_array(revdeps, tpkgver))
			xbps_array_add_cstring_nocopy(revdeps, tpkgver);
	}
	xbps_object_iterator_release(iter);
	return revdeps;
}

xbps_array_t
xbps_repo_get_pkg_revdeps(struct xbps_repo *repo, const char *pkg)
{
	xbps_array_t revdeps = NULL, vdeps = NULL;
	xbps_dictionary_t pkgd;
	const char *vpkg;
	char *buf = NULL;
	unsigned int i;
	bool match = false;

	if (((pkgd = xbps_rpool_get_pkg(repo->xhp, pkg)) == NULL) &&
	    ((pkgd = xbps_rpool_get_virtualpkg(repo->xhp, pkg)) == NULL)) {
		errno = ENOENT;
		return NULL;
	}
	/*
	 * If pkg is a virtual pkg let's match it instead of the real pkgver.
	 */
	if ((vdeps = xbps_dictionary_get(pkgd, "provides"))) {
		for (i = 0; i < xbps_array_count(vdeps); i++) {
			char *vpkgn;

			xbps_array_get_cstring_nocopy(vdeps, i, &vpkg);
			if (strchr(vpkg, '_') == NULL)
				buf = xbps_xasprintf("%s_1", vpkg);
			else
				buf = strdup(vpkg);

			vpkgn = xbps_pkg_name(buf);
			assert(vpkgn);
			if (strcmp(vpkgn, pkg) == 0) {
				free(vpkgn);
				break;
			}
			free(vpkgn);
			free(buf);
			buf = NULL;
		}
		if (buf) {
			match = true;
			revdeps = revdeps_match(repo, pkgd, buf);
			free(buf);
		}
	}
	if (!match)
		revdeps = revdeps_match(repo, pkgd, NULL);

	return revdeps;
}
