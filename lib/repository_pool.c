/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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
 * @file lib/repository_pool.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */

struct repository_pool {
	SIMPLEQ_ENTRY(repository_pool) rp_entries;
	struct repository_pool_index *rpi;
};

static SIMPLEQ_HEAD(rpool_head, repository_pool) rpool_queue =
    SIMPLEQ_HEAD_INITIALIZER(rpool_queue);

static bool repolist_initialized;

#define FETCH_ERROR(x) ((x == FETCH_UNAVAIL) || \
			(x == FETCH_NETWORK) || \
			(x == FETCH_ABORT) || \
			(x == FETCH_TIMEOUT) || \
			(x == FETCH_DOWN))
static int
sync_remote_repo(const char *plist, const char *repourl)
{
	/* if file is there, continue */
	if (access(plist, R_OK) == 0)
		return 0;

	/* file not found, fetch it */
	if (xbps_repository_sync_pkg_index(repourl) == -1) {
		if (FETCH_ERROR(fetchLastErrCode))
			return -1;
	}

	return 0;
}
#undef FETCH_ERROR

int HIDDEN
xbps_repository_pool_init(void)
{
	struct xbps_handle *xhp;
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	struct repository_pool *rpool;
	size_t ntotal = 0, nmissing = 0;
	const char *repouri;
	char *plist;
	int rv = 0;
	bool duprepo;

	xhp = xbps_handle_get();
	if (xhp->conf_dictionary == NULL)
		return ENOTSUP;

	if (repolist_initialized)
		return 0;

	array = prop_dictionary_get(xhp->conf_dictionary, "repositories");
	if (array == NULL)
		return errno;

	if (prop_array_count(array) == 0)
		return ENOTSUP;

	iter = prop_array_iterator(array);
	if (iter == NULL) {
		rv = errno;
		goto out;
	}

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		/*
		 * Check that we do not register duplicate repositories.
		 */
		duprepo = false;
		repouri = prop_string_cstring_nocopy(obj);
		SIMPLEQ_FOREACH(rpool, &rpool_queue, rp_entries) {
			if (strcmp(rpool->rpi->rpi_uri, repouri) == 0) {
				duprepo = true;
				break;
			}
		}
		if (duprepo)
			continue;

		plist = xbps_pkg_index_plist(repouri);
		if (plist == NULL) {
			rv = errno;
			goto out;
		}
		ntotal++;
		if (sync_remote_repo(plist, repouri) == -1) {
			nmissing++;
			free(plist);
			continue;
		}
		/*
		 * Iterate over the repository pool and add the dictionary
		 * for current repository into the queue.
		 */

		rpool = malloc(sizeof(struct repository_pool));
		if (rpool == NULL) {
			rv = errno;
			free(plist);
			goto out;
		}

		rpool->rpi = malloc(sizeof(struct repository_pool_index));
		if (rpool->rpi == NULL) {
			rv = errno;
			free(plist);
			free(rpool);
			goto out;
		}

		rpool->rpi->rpi_uri = prop_string_cstring(obj);
		if (rpool->rpi->rpi_uri == NULL) {
			rv = errno;
			free(rpool->rpi);
			free(rpool);
			free(plist);
			goto out;
		}
		rpool->rpi->rpi_repod =
		    prop_dictionary_internalize_from_zfile(plist);
		if (rpool->rpi->rpi_repod == NULL) {
			free(rpool->rpi->rpi_uri);
			free(rpool->rpi);
			free(rpool);
			free(plist);
			if (errno == ENOENT) {
				errno = 0;
				xbps_dbg_printf("[rpool] missing pkg-index.plist "
				    "for '%s' repository.\n", repouri);
				nmissing++;
				continue;
			}
			rv = errno;
			xbps_dbg_printf("[rpool] cannot internalize plist %s: %s\n",
			    plist, strerror(rv));
			goto out;
		}
		free(plist);
		xbps_dbg_printf("Registered repository '%s'\n",
		    rpool->rpi->rpi_uri);
		SIMPLEQ_INSERT_TAIL(&rpool_queue, rpool, rp_entries);
	}

	if (ntotal - nmissing == 0) {
		/* no repositories available, error out */
		rv = ENOTSUP;
		goto out;
	}

	repolist_initialized = true;
	xbps_dbg_printf("[rpool] initialized ok.\n");
out:
	if (iter)
		prop_object_iterator_release(iter);
	if (rv != 0) 
		xbps_repository_pool_release();

	return rv;

}

void HIDDEN
xbps_repository_pool_release(void)
{
	struct repository_pool *rpool;

	if (!repolist_initialized)
		return;

	while ((rpool = SIMPLEQ_FIRST(&rpool_queue)) != NULL) {
		SIMPLEQ_REMOVE(&rpool_queue, rpool, repository_pool, rp_entries);
		xbps_dbg_printf("Unregistering repository '%s'...",
		    rpool->rpi->rpi_uri);
		prop_object_release(rpool->rpi->rpi_repod);
		free(rpool->rpi->rpi_uri);
		free(rpool->rpi);
		free(rpool);
		rpool = NULL;
		xbps_dbg_printf_append("done\n");

	}
	repolist_initialized = false;
	xbps_dbg_printf("[rpool] released ok.\n");
}

int
xbps_repository_pool_foreach(
		int (*fn)(struct repository_pool_index *, void *, bool *),
		void *arg)
{
	struct repository_pool *rpool, *rpool_new;
	int rv = 0;
	bool done = false;

	assert(fn != NULL);
	/*
	 * Initialize repository pool.
	 */
	if ((rv = xbps_repository_pool_init()) != 0) {
		if (rv == ENOTSUP) {
			xbps_dbg_printf("[rpool] empty repository list.\n");
		} else if (rv != ENOENT && rv != ENOTSUP) {
			xbps_dbg_printf("[rpool] couldn't initialize: %s\n",
			    strerror(rv));
		}
		return rv;
	}

	SIMPLEQ_FOREACH_SAFE(rpool, &rpool_queue, rp_entries, rpool_new) {
		rv = (*fn)(rpool->rpi, arg, &done);
		if (rv != 0 || done)
			break;
	}

	return rv;
}

struct repo_pool_fpkg {
	prop_dictionary_t pkgd;
	const char *pattern;
	bool bypattern;
	bool pkgfound;
};

static int
repo_find_virtualpkg_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repo_pool_fpkg *rpf = arg;

	if (rpf->bypattern) {
		rpf->pkgd =
		    xbps_find_virtualpkg_conf_in_dict_by_pattern(rpi->rpi_repod,
		    "packages", rpf->pattern);
	} else {
		rpf->pkgd =
		    xbps_find_virtualpkg_conf_in_dict_by_name(rpi->rpi_repod,
		    "packages", rpf->pattern);
	}
	if (rpf->pkgd) {
		prop_dictionary_set_cstring(rpf->pkgd, "repository",
		    rpi->rpi_uri);
		*done = true;
		rpf->pkgfound = true;
		return 0;
	}
	/* not found */
	return 0;
}

static int
repo_find_pkg_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repo_pool_fpkg *rpf = arg;

	if (rpf->bypattern) {
		rpf->pkgd = xbps_find_pkg_in_dict_by_pattern(rpi->rpi_repod,
		    "packages", rpf->pattern);
		/* If no pkg exists matching pattern, look for virtual packages */
		if (rpf->pkgd == NULL) {
			rpf->pkgd = xbps_find_virtualpkg_in_dict_by_pattern(
			    rpi->rpi_repod, "packages", rpf->pattern);
		}
	} else {
		rpf->pkgd = xbps_find_pkg_in_dict_by_name(rpi->rpi_repod,
		    "packages", rpf->pattern);
		/* If no pkg exists matching pattern, look for virtual packages */
		if (rpf->pkgd == NULL) {
			rpf->pkgd = xbps_find_virtualpkg_in_dict_by_name(
			    rpi->rpi_repod, "packages", rpf->pattern);
		}
	}
	if (rpf->pkgd) {
		/*
		 * Package dictionary found, add the "repository"
		 * object with the URI.
		 */
		prop_dictionary_set_cstring(rpf->pkgd, "repository",
		    rpi->rpi_uri);
		*done = true;
		rpf->pkgfound = true;
		return 0;
	}
	/* Not found */
	return 0;
}

static int
repo_find_best_pkg_cb(struct repository_pool_index *rpi,
		      void *arg,
		      bool *done)
{
	struct repo_pool_fpkg *rpf = arg;
	prop_dictionary_t instpkgd;
	const char *instver, *repover;

	rpf->pkgd = xbps_find_pkg_in_dict_by_name(rpi->rpi_repod,
	    "packages", rpf->pattern);
	if (rpf->pkgd == NULL) {
		if (errno && errno != ENOENT)
			return errno;

		xbps_dbg_printf("[rpool] Package '%s' not found in repository "
		    "'%s'.\n", rpf->pattern, rpi->rpi_uri);
	} else {
		/*
		 * Check if version in repository is greater than
		 * the version currently installed.
		 */
		instpkgd = xbps_find_pkg_dict_installed(rpf->pattern, false);
		if (instpkgd == NULL)
			return 0;
		prop_dictionary_get_cstring_nocopy(instpkgd,
		    "version", &instver);
		prop_dictionary_get_cstring_nocopy(rpf->pkgd,
		    "version", &repover);
		prop_object_release(instpkgd);

		if (xbps_cmpver(repover, instver) <= 0) {
			xbps_dbg_printf("[rpool] Skipping '%s-%s' "
			    "(installed: %s) from repository `%s'\n",
			    rpf->pattern, repover, instver,
			    rpi->rpi_uri);
			errno = EEXIST;
			return 0;
		}
		/*
		 * New package version found, exit from the loop.
		 */
		xbps_dbg_printf("[rpool] Found '%s-%s' (installed: %s) "
		    "in repository '%s'.\n", rpf->pattern, repover,
		    instver, rpi->rpi_uri);
		prop_dictionary_set_cstring(rpf->pkgd, "repository",
		    rpi->rpi_uri);
		*done = true;
		errno = 0;
		rpf->pkgfound = true;
	}
	return 0;
}

prop_dictionary_t
xbps_repository_pool_find_pkg(const char *pkg, bool bypattern, bool best)
{
	struct repo_pool_fpkg *rpf;
	prop_dictionary_t pkgd = NULL;
	int rv = 0;

	assert(pkg != NULL);

	rpf = calloc(1, sizeof(*rpf));
	if (rpf == NULL)
		return NULL;

	rpf->pattern = pkg;
	rpf->bypattern = bypattern;

	if (best) {
		/*
		 * Look for the best package version of a package name or
		 * pattern in all repositories.
		 */
		rv = xbps_repository_pool_foreach(repo_find_best_pkg_cb, rpf);
		if (rv != 0) {
			errno = rv;
			goto out;
		} else if (rpf->pkgfound == false) {
			goto out;
		}
	} else {
		/*
		 * Look for any virtual package set by the user matching
		 * the package name or pattern.
		 */
		rv = xbps_repository_pool_foreach(repo_find_virtualpkg_cb, rpf);
		if (rv != 0) {
			errno = rv;
			goto out;
		} else if (rpf->pkgfound == false) {
			/*
			 * No virtual package found. Look for real package
			 * names or patterns instead.
			 */
			rv = xbps_repository_pool_foreach(repo_find_pkg_cb, rpf);
			if (rv != 0) {
				errno = rv;
				goto out;
			} else if (rpf->pkgfound == false) {
				goto out;
			}
		}
	}
	pkgd = prop_dictionary_copy(rpf->pkgd);
out:
	free(rpf);

	return pkgd;
}

prop_dictionary_t
xbps_repository_pool_dictionary_metadata_plist(const char *pkgname,
					       const char *plistf)
{
	prop_dictionary_t pkgd = NULL, plistd = NULL;
	const char *repoloc;
	char *url;

	assert(pkgname != NULL);
	assert(plistf != NULL);

	/*
	 * Iterate over the the repository pool and search for a plist file
	 * in the binary package named 'pkgname'. The plist file will be
	 * internalized to a proplib dictionary.
	 *
	 * The first repository that has it wins and the loop is stopped.
	 * This will work locally and remotely, thanks to libarchive and
	 * libfetch!
	 */
	pkgd = xbps_repository_pool_find_pkg(pkgname, false, false);
	if (pkgd == NULL)
		goto out;

	prop_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc);
	url = xbps_path_from_repository_uri(pkgd, repoloc);
	if (url == NULL) {
		errno = EINVAL;
		goto out;
	}
	plistd = xbps_dictionary_metadata_plist_by_url(url, plistf);
	free(url);

out:
	if (plistd == NULL)
		errno = ENOENT;
	if (pkgd)
		prop_object_release(pkgd);

	return plistd;
}
