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
 * @file lib/repository_pool_find.c
 * @brief Repository pool routines
 * @defgroup repopool Repository pool functions
 */
struct repo_pool_fpkg {
	prop_dictionary_t pkgd;
	const char *pattern;
	bool bypattern;
};

static int
repo_find_virtualpkg_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repo_pool_fpkg *rpf = arg;

	assert(rpi != NULL);

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
#ifdef DEBUG
		xbps_dbg_printf("%s: found pkg in repository\n", __func__);
		xbps_dbg_printf_append("%s", prop_dictionary_externalize(rpf->pkgd));
#endif
		prop_dictionary_set_cstring(rpf->pkgd, "repository",
		    rpi->rpi_uri);
		*done = true;
		return 0;
	}
	/* not found */
	return 0;
}

static int
repo_find_pkg_cb(struct repository_pool_index *rpi, void *arg, bool *done)
{
	struct repo_pool_fpkg *rpf = arg;

	assert(rpi != NULL);

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
#ifdef DEBUG
		xbps_dbg_printf("%s: found pkg in repository\n", __func__);
		xbps_dbg_printf_append("%s", prop_dictionary_externalize(rpf->pkgd));
#endif
		prop_dictionary_set_cstring(rpf->pkgd, "repository",
		    rpi->rpi_uri);
		*done = true;
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

	assert(rpi != NULL);

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
		if (instpkgd == NULL) {
			xbps_dbg_printf("[rpool] `%s' not installed, "
			    "ignoring...\n", rpf->pattern);
			rpf->pkgd = NULL;
			return ENODEV;
		}
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
			rpf->pkgd = NULL;
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
	}
	return 0;
}

static struct repo_pool_fpkg *
repo_find_pkg(const char *pkg, bool bypattern, bool best, bool virtual)
{
	struct repo_pool_fpkg *rpf;
	int rv = 0;

	assert(pkg != NULL);

	rpf = malloc(sizeof(*rpf));
	if (rpf == NULL)
		return NULL;

	rpf->pattern = pkg;
	rpf->bypattern = bypattern;
	rpf->pkgd = NULL;

	if (best) {
		/*
		 * Look for the best package version of a package name or
		 * pattern in all repositories.
		 */
		rv = xbps_repository_pool_foreach(repo_find_best_pkg_cb, rpf);
		if (rv != 0)
			errno = rv;
	} else {
		if (virtual) {
			/*
			 * No package found. Look for virtual package
			 * set by the user or any virtual pkg available.
			 */
			rv = xbps_repository_pool_foreach(repo_find_virtualpkg_cb, rpf);
			if (rv != 0)
				errno = rv;
		} else {
			/*
			 * Look for a package (non virtual) in repositories.
			 */
			rv = xbps_repository_pool_foreach(repo_find_pkg_cb, rpf);
			if (rv != 0)
				errno = rv;
		}
	}

	return rpf;
}

prop_dictionary_t
xbps_repository_pool_find_virtualpkg(const char *pkg, bool bypattern, bool best)
{
	struct repo_pool_fpkg *rpf;
	prop_dictionary_t pkgd = NULL;

	assert(pkg != NULL);

	rpf = repo_find_pkg(pkg, bypattern, best, true);
	if (prop_object_type(rpf->pkgd) == PROP_TYPE_DICTIONARY)
		pkgd = prop_dictionary_copy(rpf->pkgd);
	free(rpf);

	return pkgd;
}

prop_dictionary_t
xbps_repository_pool_find_pkg(const char *pkg, bool bypattern, bool best)
{
	struct repo_pool_fpkg *rpf;
	prop_dictionary_t pkgd = NULL;

	assert(pkg != NULL);

	rpf = repo_find_pkg(pkg, bypattern, best, false);
	if (prop_object_type(rpf->pkgd) == PROP_TYPE_DICTIONARY)
		pkgd = prop_dictionary_copy(rpf->pkgd);
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
	if (prop_object_type(pkgd) == PROP_TYPE_DICTIONARY)
		prop_object_release(pkgd);

	return plistd;
}
