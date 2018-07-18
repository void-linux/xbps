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

#include <sys/utsname.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

struct rpool_fpkg {
	xbps_array_t revdeps;
	xbps_dictionary_t pkgd;
	const char *pattern;
	const char *bestpkgver;
	bool best;
};

typedef enum {
	BEST_PKG = 1,
	VIRTUAL_PKG,
	REAL_PKG,
	REVDEPS_PKG
} pkg_repo_type_t;

static SIMPLEQ_HEAD(rpool_head, xbps_repo) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

/**
 * @file lib/rpool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

int
xbps_rpool_sync(struct xbps_handle *xhp, const char *uri)
{
	const char *repouri;

	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		/* If argument was set just process that repository */
		if (uri && strcmp(repouri, uri))
			continue;

		if (xbps_repo_sync(xhp, repouri) == -1) {
			xbps_dbg_printf(xhp,
			    "[rpool] `%s' failed to fetch repository data: %s\n",
			    repouri, fetchLastErrCode == 0 ? strerror(errno) :
			    xbps_fetch_error_string());
			continue;
		}
	}
	return 0;
}

struct xbps_repo HIDDEN *
xbps_regget_repo(struct xbps_handle *xhp, const char *url)
{
	struct xbps_repo *repo;
	const char *repouri;

	if (SIMPLEQ_EMPTY(&rpool_queue)) {
		/* iterate until we have a match */
		for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
			xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
			if (strcmp(repouri, url))
				continue;

			repo = xbps_repo_open(xhp, repouri);
			if (!repo)
				return NULL;

			SIMPLEQ_INSERT_TAIL(&rpool_queue, repo, entries);
			xbps_dbg_printf(xhp, "[rpool] `%s' registered.\n", repouri);
		}
	}
	SIMPLEQ_FOREACH(repo, &rpool_queue, entries)
		if (strcmp(url, repo->uri) == 0)
			return repo;

	return NULL;
}

struct xbps_repo *
xbps_rpool_get_repo(const char *url)
{
	struct xbps_repo *repo;

	SIMPLEQ_FOREACH(repo, &rpool_queue, entries)
		if (strcmp(url, repo->uri) == 0)
			return repo;

	return NULL;
}

void
xbps_rpool_release(struct xbps_handle *xhp UNUSED)
{
	struct xbps_repo *repo;

	while ((repo = SIMPLEQ_FIRST(&rpool_queue))) {
	       SIMPLEQ_REMOVE(&rpool_queue, repo, xbps_repo, entries);
	       xbps_repo_close(repo);
	}
	if (xhp->repositories)
		xbps_object_release(xhp->repositories);
}

int
xbps_rpool_foreach(struct xbps_handle *xhp,
	int (*fn)(struct xbps_repo *, void *, bool *),
	void *arg)
{
	struct xbps_repo *repo;
	const char *repouri;
	int rv = 0;
	bool foundrepo = false, done = false;

	assert(fn != NULL);

	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		xbps_array_get_cstring_nocopy(xhp->repositories, i, &repouri);
		if ((repo = xbps_rpool_get_repo(repouri)) == NULL) {
			repo = xbps_repo_open(xhp, repouri);
			if (!repo)
				continue;
			SIMPLEQ_INSERT_TAIL(&rpool_queue, repo, entries);
			xbps_dbg_printf(xhp, "[rpool] `%s' registered.\n", repouri);
		}
		foundrepo = true;
		rv = (*fn)(repo, arg, &done);
		if (rv != 0 || done)
			break;
	}
	if (!foundrepo)
		rv = ENOTSUP;

	return rv;
}

static int
find_virtualpkg_cb(struct xbps_repo *repo, void *arg, bool *done)
{
	struct rpool_fpkg *rpf = arg;

	rpf->pkgd = xbps_repo_get_virtualpkg(repo, rpf->pattern);
	if (rpf->pkgd) {
		/* found */
		*done = true;
		return 0;
	}
	/* not found */
	return 0;
}

static int
find_pkg_cb(struct xbps_repo *repo, void *arg, bool *done)
{
	struct rpool_fpkg *rpf = arg;

	rpf->pkgd = xbps_repo_get_pkg(repo, rpf->pattern);
	if (rpf->pkgd) {
		/* found */
		*done = true;
		return 0;
	}
	/* Not found */
	return 0;
}

static int
find_pkg_revdeps_cb(struct xbps_repo *repo, void *arg, bool *done UNUSED)
{
	struct rpool_fpkg *rpf = arg;
	xbps_array_t revdeps = NULL;
	const char *pkgver;

	revdeps = xbps_repo_get_pkg_revdeps(repo, rpf->pattern);
	if (xbps_array_count(revdeps)) {
		/* found */
		if (rpf->revdeps == NULL)
			rpf->revdeps = xbps_array_create();
		for (unsigned int i = 0; i < xbps_array_count(revdeps); i++) {
			xbps_array_get_cstring_nocopy(revdeps, i, &pkgver);
			xbps_array_add_cstring_nocopy(rpf->revdeps, pkgver);
		}
		xbps_object_release(revdeps);
	}
	return 0;
}

static int
find_best_pkg_cb(struct xbps_repo *repo, void *arg, bool *done UNUSED)
{
	struct rpool_fpkg *rpf = arg;
	xbps_dictionary_t pkgd;
	const char *repopkgver;

	pkgd = xbps_repo_get_pkg(repo, rpf->pattern);
	if (pkgd == NULL) {
		if (errno && errno != ENOENT)
			return errno;

		xbps_dbg_printf(repo->xhp,
		    "[rpool] Package '%s' not found in repository "
		    "'%s'.\n", rpf->pattern, repo->uri);
		return 0;
	}
	xbps_dictionary_get_cstring_nocopy(pkgd,
	    "pkgver", &repopkgver);
	if (rpf->bestpkgver == NULL) {
		xbps_dbg_printf(repo->xhp,
		    "[rpool] Found match '%s' (%s).\n",
		    repopkgver, repo->uri);
		rpf->pkgd = pkgd;
		rpf->bestpkgver = repopkgver;
		return 0;
	}
	/*
	 * Compare current stored version against new
	 * version from current package in repository.
	 */
	if (xbps_cmpver(repopkgver, rpf->bestpkgver) == 1) {
		xbps_dbg_printf(repo->xhp,
		    "[rpool] Found best match '%s' (%s).\n",
		    repopkgver, repo->uri);
		rpf->pkgd = pkgd;
		rpf->bestpkgver = repopkgver;
	}
	return 0;
}

static xbps_object_t
repo_find_pkg(struct xbps_handle *xhp,
	      const char *pkg,
	      pkg_repo_type_t type)
{
	struct rpool_fpkg rpf;
	int rv = 0;

	assert(xhp);
	assert(pkg);

	rpf.pattern = pkg;
	rpf.pkgd = NULL;
	rpf.revdeps = NULL;
	rpf.bestpkgver = NULL;

	switch (type) {
	case BEST_PKG:
		/*
		 * Find best pkg version.
		 */
		rv = xbps_rpool_foreach(xhp, find_best_pkg_cb, &rpf);
		break;
	case VIRTUAL_PKG:
		/*
		 * Find virtual pkg.
		 */
		rv = xbps_rpool_foreach(xhp, find_virtualpkg_cb, &rpf);
		break;
	case REAL_PKG:
		/*
		 * Find real pkg.
		 */
		rv = xbps_rpool_foreach(xhp, find_pkg_cb, &rpf);
		break;
	case REVDEPS_PKG:
		/*
		 * Find revdeps for pkg.
		 */
		rv = xbps_rpool_foreach(xhp, find_pkg_revdeps_cb, &rpf);
		break;
	}
	if (rv != 0) {
		errno = rv;
		return NULL;
	}
	if (type == REVDEPS_PKG) {
		if (rpf.revdeps == NULL)
			errno = ENOENT;

		return rpf.revdeps;
	} else {
		if (rpf.pkgd == NULL)
			errno = ENOENT;
	}
	return rpf.pkgd;
}

xbps_dictionary_t
xbps_rpool_get_virtualpkg(struct xbps_handle *xhp, const char *pkg)
{
	return repo_find_pkg(xhp, pkg, VIRTUAL_PKG);
}

xbps_dictionary_t
xbps_rpool_get_pkg(struct xbps_handle *xhp, const char *pkg)
{
	if (xhp->flags & XBPS_FLAG_BESTMATCH)
		return repo_find_pkg(xhp, pkg, BEST_PKG);

	return repo_find_pkg(xhp, pkg, REAL_PKG);
}

xbps_array_t
xbps_rpool_get_pkg_revdeps(struct xbps_handle *xhp, const char *pkg)
{
	return repo_find_pkg(xhp, pkg, REVDEPS_PKG);
}

xbps_array_t
xbps_rpool_get_pkg_fulldeptree(struct xbps_handle *xhp, const char *pkg)
{
	return xbps_get_pkg_fulldeptree(xhp, pkg, true);
}

xbps_dictionary_t
xbps_rpool_get_pkg_plist(struct xbps_handle *xhp,
			 const char *pkg,
			 const char *plistf)
{
	xbps_dictionary_t pkgd = NULL, plistd = NULL;
	char *url;

	assert(pkg != NULL);
	assert(plistf != NULL);
	/*
	 * Iterate over the the repository pool and search for a plist file
	 * in the binary package matching `pattern'. The plist file will be
	 * internalized to a proplib dictionary.
	 *
	 * The first repository that has it wins and the loop is stopped.
	 * This will work locally and remotely, thanks to libarchive and
	 * libfetch!
	 */
	if (((pkgd = xbps_rpool_get_pkg(xhp, pkg)) == NULL) &&
	    ((pkgd = xbps_rpool_get_virtualpkg(xhp, pkg)) == NULL))
		goto out;

	url = xbps_repository_pkg_path(xhp, pkgd);
	if (url == NULL) {
		errno = EINVAL;
		goto out;
	}
	plistd = xbps_archive_fetch_plist(url, plistf);
	free(url);

out:
	if (plistd == NULL)
		errno = ENOENT;

	return plistd;
}
