/*-
 * Copyright (c) 2008-2012 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

static int
store_dependency(struct xbps_handle *xhp,
		 prop_dictionary_t repo_pkgd,
		 pkg_state_t repo_pkg_state,
		 size_t *depth)
{
	prop_array_t unsorted;
	int rv;
	/*
	 * Overwrite package state in dictionary with same state than the
	 * package currently uses, otherwise not-installed.
	 */
	if ((rv = xbps_set_pkg_state_dictionary(repo_pkgd, repo_pkg_state)) != 0)
		return rv;
	/*
	 * Add required objects into package dep's dictionary.
	 */
	if (!prop_dictionary_set_bool(repo_pkgd, "automatic-install", true))
		return EINVAL;
	/*
	 * Add the dictionary into the array.
	 */
	unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");
	if (!prop_array_add(unsorted, repo_pkgd))
		return EINVAL;

	if (xhp->flags & XBPS_FLAG_DEBUG) {
		size_t x;
		const char *repo, *pkgver;

		prop_dictionary_get_cstring_nocopy(repo_pkgd,
		    "repository", &repo);
		prop_dictionary_get_cstring_nocopy(repo_pkgd,
		    "pkgver", &pkgver);
		xbps_dbg_printf(xhp, " ");
		for (x = 0; x < *depth; x++)
			xbps_dbg_printf_append(xhp, " ");

		xbps_dbg_printf_append(xhp, "%s: added into "
		    "the transaction (%s).\n", pkgver, repo);
	}
	return 0;
}

static int
add_missing_reqdep(struct xbps_handle *xhp, const char *reqpkg)
{
	prop_array_t mdeps;
	prop_string_t reqpkg_str;
	prop_object_iterator_t iter = NULL;
	prop_object_t obj;
	size_t idx = 0;
	bool add_pkgdep, pkgfound, update_pkgdep;
	int rv = 0;

	assert(reqpkg != NULL);

	add_pkgdep = update_pkgdep = pkgfound = false;
	mdeps = prop_dictionary_get(xhp->transd, "missing_deps");

	reqpkg_str = prop_string_create_cstring_nocopy(reqpkg);
	if (reqpkg_str == NULL)
		return errno;

	iter = prop_array_iterator(mdeps);
	if (iter == NULL)
		goto out;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		const char *curdep, *curver, *pkgver;
		char *curpkgnamedep = NULL, *pkgnamedep = NULL;

		assert(prop_object_type(obj) == PROP_TYPE_STRING);
		curdep = prop_string_cstring_nocopy(obj);
		curver = xbps_pkgpattern_version(curdep);
		pkgver = xbps_pkgpattern_version(reqpkg);
		if (curver == NULL || pkgver == NULL)
			goto out;
		curpkgnamedep = xbps_pkgpattern_name(curdep);
		if (curpkgnamedep == NULL)
			goto out;
		pkgnamedep = xbps_pkgpattern_name(reqpkg);
		if (pkgnamedep == NULL) {
			free(curpkgnamedep);
			goto out;
		}
		if (strcmp(pkgnamedep, curpkgnamedep) == 0) {
			pkgfound = true;
			if (strcmp(curver, pkgver) == 0) {
				free(curpkgnamedep);
				free(pkgnamedep);
				rv = EEXIST;
				goto out;
			}
			/*
			 * if new dependency version is greater than current
			 * one, store it.
			 */
			xbps_dbg_printf(xhp, "Missing pkgdep name matched, "
			    "curver: %s newver: %s\n", curver, pkgver);
			if (xbps_cmpver(curver, pkgver) <= 0) {
				add_pkgdep = false;
				free(curpkgnamedep);
				free(pkgnamedep);
				rv = EEXIST;
				goto out;
			}
			update_pkgdep = true;
		}
		free(curpkgnamedep);
		free(pkgnamedep);
		if (pkgfound)
			break;

		idx++;
	}
	add_pkgdep = true;
out:
	if (iter)
		prop_object_iterator_release(iter);
	if (update_pkgdep)
		prop_array_remove(mdeps, idx);
	if (add_pkgdep && !xbps_add_obj_to_array(mdeps, reqpkg_str)) {
		prop_object_release(reqpkg_str);
		return errno;
	}

	return rv;
}

#define MAX_DEPTH	512

static int
find_repo_deps(struct xbps_handle *xhp,
	       prop_array_t pkg_rdeps_array,	/* current pkg rundeps array  */
	       const char *curpkg,		/* current pkgver */
	       size_t *depth)			/* max recursion depth */
{
	prop_dictionary_t curpkgd, tmpd;
	prop_array_t curpkgrdeps, unsorted;
	pkg_state_t state;
	size_t i, x;
	const char *reqpkg, *pkgver_q, *reason = NULL;
	char *pkgname;
	int rv = 0;

	if (*depth >= MAX_DEPTH)
		return ELOOP;

	/*
	 * Iterate over the list of required run dependencies for
	 * current package.
	 */
	for (i = 0; i < prop_array_count(pkg_rdeps_array); i++) {
		prop_array_get_cstring_nocopy(pkg_rdeps_array, i, &reqpkg);
		if (xhp->flags & XBPS_FLAG_DEBUG) {
			xbps_dbg_printf(xhp, "");
			for (x = 0; x < *depth; x++)
				xbps_dbg_printf_append(xhp, " ");
			xbps_dbg_printf_append(xhp, "%s: requires dependency '%s': ",
			    curpkg != NULL ? curpkg : " ", reqpkg);
		}
		/*
		 * Pass 1: check if required dependency is already installed
		 * and its version is fully matched.
		 */
		if ((pkgname = xbps_pkgpattern_name(reqpkg)) == NULL) {
			rv = EINVAL;
			xbps_dbg_printf(xhp, "failed to get "
			    "pkgname from `%s'!", reqpkg);
			break;
		}
		/*
		 * Look for a real package installed...
		 */
		tmpd = xbps_find_pkg_dict_installed(xhp, pkgname, false);
		if (tmpd == NULL) {
			if (errno && errno != ENOENT) {
				/* error */
				rv = errno;
				xbps_dbg_printf(xhp, "failed to find "
				    "installed pkg for `%s': %s\n",
				    reqpkg, strerror(errno));
				break;
			}
			/*
			 * real package not installed, try looking for
			 * a virtual package instead.
			 */
			tmpd = xbps_find_virtualpkg_dict_installed(xhp,
					pkgname, false);
		}
		free(pkgname);
		if (tmpd == NULL) {
			if (errno && errno != ENOENT) {
				/* error */
				rv = errno;
				xbps_dbg_printf(xhp, "failed to find "
				    "installed virtual pkg for `%s': %s\n",
				    reqpkg, strerror(errno));
				break;
			}
			/* Required pkgdep not installed */
			xbps_dbg_printf_append(xhp, "not installed. ");
			reason = "install";
			state = XBPS_PKG_STATE_NOT_INSTALLED;
		} else {
			/*
			 * Check if installed version matches the
			 * required pkgdep version.
			 */
			prop_dictionary_get_cstring_nocopy(tmpd,
			    "pkgver", &pkgver_q);

			/* Check its state */
			rv = xbps_pkg_state_dictionary(tmpd, &state);
			if (rv != 0) {
				prop_object_release(tmpd);
				break;
			}
			if (xbps_match_virtual_pkg_in_dict(tmpd,reqpkg,true)) {
				/*
				 * Check if required dependency is a virtual
				 * package and is satisfied by an
				 * installed package.
				 */
				xbps_dbg_printf_append(xhp,
				    "[virtual] satisfied by "
				    "`%s'.\n", pkgver_q);
				prop_object_release(tmpd);
				continue;
			}
			rv = xbps_pkgpattern_match(pkgver_q, reqpkg);
			if (rv == 0) {
				/*
				 * Package is installed but does not match
				 * the dependency pattern, update pkg.
				 */
				xbps_dbg_printf_append(xhp,
				    "installed `%s', "
				    "must be updated.\n", pkgver_q);
				reason = "update";
			} else if (rv == 1) {
				rv = 0;
				if (state == XBPS_PKG_STATE_UNPACKED) {
					/*
					 * Package matches the dependency
					 * pattern but was only unpacked,
					 * configure pkg.
					 */
					xbps_dbg_printf_append(xhp,
					    "installed `%s'"
					    ", must be configured.\n",
					    pkgver_q);
					reason = "configure";
				} else {
					/*
					 * Package matches the dependency
					 * pattern and is fully installed,
					 * skip to next one.
					 */
					xbps_dbg_printf_append(xhp,
					    "installed "
					    "`%s'.\n", pkgver_q);
					prop_object_release(tmpd);
					continue;
				}
			} else {
				/* error matching pkgpattern */
				prop_object_release(tmpd);
				xbps_dbg_printf(xhp, "failed to match "
				    "pattern %s with %s\n", reqpkg, pkgver_q);
				break;
			}
		}
		/*
		 * Pass 2: check if required dependency has been already
		 * added in the transaction dictionary.
		 */
		unsorted = prop_dictionary_get(xhp->transd, "unsorted_deps");
		if (((curpkgd = xbps_find_pkg_in_array_by_pattern(unsorted, reqpkg, NULL)) == NULL) &&
		    ((curpkgd = xbps_find_virtualpkg_conf_in_array_by_pattern(xhp, unsorted, reqpkg)) == NULL) &&
		    ((curpkgd = xbps_find_virtualpkg_in_array_by_pattern(unsorted, reqpkg)) == NULL)) {
			/* error matching required pkgdep */
			if (errno && errno != ENOENT) {
				rv = errno;
				break;
			}
		} else {
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "pkgver", &pkgver_q);
			xbps_dbg_printf_append(xhp, " (%s queued "
			    "in transaction).\n", pkgver_q);
			continue;
		}
		/*
		 * Pass 3: find required dependency in repository pool.
		 * If dependency does not match add pkg into the missing
		 * deps array and pass to next one.
		 */
		if (((curpkgd = xbps_rpool_find_virtualpkg_conf(xhp, reqpkg, true)) == NULL) &&
		    ((curpkgd = xbps_rpool_find_pkg(xhp, reqpkg, true, true)) == NULL) &&
		    ((curpkgd = xbps_rpool_find_virtualpkg(xhp, reqpkg, true)) == NULL)) {
			/* pkg not found, there was some error */
			if (errno && errno != ENOENT) {
				xbps_dbg_printf(xhp, "failed to find pkg "
				    "for `%s' in rpool: %s\n",
				    reqpkg, strerror(errno));
				rv = errno;
				break;
			}
			rv = add_missing_reqdep(xhp, reqpkg);
			if (rv != 0 && rv != EEXIST) {
				xbps_dbg_printf_append(xhp, "`%s': "
				    "add_missing_reqdep failed %s\n",
				    reqpkg);
				break;
			} else if (rv == EEXIST) {
				xbps_dbg_printf_append(xhp, "`%s' missing "
				    "dep already added.\n", reqpkg);
				rv = 0;
				continue;
			} else {
				xbps_dbg_printf_append(xhp, "`%s' added "
				    "into the missing deps array.\n",
				    reqpkg);
				continue;
			}
		}
		prop_dictionary_get_cstring_nocopy(curpkgd,
		    "pkgver", &pkgver_q);
		/*
		 * Check if package has matched conflicts.
		 */
		xbps_pkg_find_conflicts(xhp, curpkgd);
		/*
		 * Package is on repo, add it into the transaction dictionary.
		 */
		prop_dictionary_set_cstring_nocopy(curpkgd, "transaction", reason);
		rv = store_dependency(xhp, curpkgd, state, depth);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "store_dependency failed for "
			    "`%s': %s\n", reqpkg, strerror(rv));
			prop_object_release(curpkgd);
			break;
		}
		/*
		 * If package doesn't have rundeps, pass to the next one.
		 */
		curpkgrdeps = prop_dictionary_get(curpkgd, "run_depends");
		if (curpkgrdeps == NULL) {
			prop_object_release(curpkgd);
			continue;
		}
		prop_object_release(curpkgd);
		if (xhp->flags & XBPS_FLAG_DEBUG) {
			xbps_dbg_printf(xhp, "");
			for (x = 0; x < *depth; x++)
				xbps_dbg_printf_append(xhp, " ");

			xbps_dbg_printf_append(xhp,
			    "%s: finding dependencies:\n", pkgver_q);
		}
		/*
		 * Recursively find rundeps for current pkg dictionary.
		 */
		(*depth)++;
		rv = find_repo_deps(xhp, curpkgrdeps, pkgver_q, depth);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "Error checking %s for rundeps: %s\n",
			    reqpkg, strerror(rv));
			break;
		}
	}
	(*depth)--;

	return rv;
}

int HIDDEN
xbps_repository_find_pkg_deps(struct xbps_handle *xhp,
			      prop_dictionary_t repo_pkgd)
{
	prop_array_t pkg_rdeps;
	const char *pkgver;
	size_t depth = 0;

	pkg_rdeps = prop_dictionary_get(repo_pkgd, "run_depends");
	if (prop_object_type(pkg_rdeps) != PROP_TYPE_ARRAY)
		return 0;

	prop_dictionary_get_cstring_nocopy(repo_pkgd, "pkgver", &pkgver);
	xbps_dbg_printf(xhp, "Finding required dependencies for '%s':\n", pkgver);
	/*
	 * This will find direct and indirect deps, if any of them is not
	 * there it will be added into the missing_deps array.
	 */
	return find_repo_deps(xhp, pkg_rdeps, pkgver, &depth);
}
