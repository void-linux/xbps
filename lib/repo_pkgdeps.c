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
		 xbps_array_t unsorted,
		 xbps_dictionary_t repo_pkgd,
		 pkg_state_t repo_pkg_state)
{
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
	if (!xbps_dictionary_set_bool(repo_pkgd, "automatic-install", true))
		return EINVAL;
	/*
	 * Add the dictionary into the unsorted queue.
	 */
	xbps_array_add(unsorted, repo_pkgd);
	xbps_dbg_printf_append(xhp, "(added)\n");

	return 0;
}

static int
add_missing_reqdep(struct xbps_handle *xhp, const char *reqpkg)
{
	xbps_array_t mdeps;
	xbps_string_t reqpkg_str;
	xbps_object_iterator_t iter = NULL;
	xbps_object_t obj;
	unsigned int idx = 0;
	bool add_pkgdep, pkgfound, update_pkgdep;
	int rv = 0;

	assert(reqpkg != NULL);

	add_pkgdep = update_pkgdep = pkgfound = false;
	mdeps = xbps_dictionary_get(xhp->transd, "missing_deps");

	reqpkg_str = xbps_string_create_cstring_nocopy(reqpkg);
	if (reqpkg_str == NULL)
		return errno;

	iter = xbps_array_iterator(mdeps);
	if (iter == NULL)
		goto out;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		const char *curdep, *curver, *pkgver;
		char *curpkgnamedep = NULL, *pkgnamedep = NULL;

		assert(xbps_object_type(obj) == XBPS_TYPE_STRING);
		curdep = xbps_string_cstring_nocopy(obj);
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
		xbps_object_iterator_release(iter);
	if (update_pkgdep)
		xbps_array_remove(mdeps, idx);
	if (add_pkgdep && !xbps_add_obj_to_array(mdeps, reqpkg_str)) {
		xbps_object_release(reqpkg_str);
		return errno;
	}

	return rv;
}

#define MAX_DEPTH	512

static int
find_repo_deps(struct xbps_handle *xhp,
	       xbps_array_t unsorted,		/* array of unsorted deps */
	       xbps_array_t pkg_rdeps_array,	/* current pkg rundeps array  */
	       const char *curpkg,		/* current pkgver */
	       unsigned short *depth)		/* max recursion depth */
{
	xbps_dictionary_t curpkgd, tmpd;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_array_t curpkgrdeps;
	pkg_state_t state;
	unsigned int x;
	const char *reqpkg, *pkgver_q, *reason = NULL;
	char *pkgname, *reqpkgname;
	int rv = 0;

	if (*depth >= MAX_DEPTH)
		return ELOOP;

	/*
	 * Iterate over the list of required run dependencies for
	 * current package.
	 */
	iter = xbps_array_iterator(pkg_rdeps_array);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		reqpkg = xbps_string_cstring_nocopy(obj);
		if (xhp->flags & XBPS_FLAG_DEBUG) {
			xbps_dbg_printf(xhp, "");
			for (x = 0; x < *depth; x++)
				xbps_dbg_printf_append(xhp, " ");
			xbps_dbg_printf_append(xhp, "%s: requires dependency '%s': ",
			    curpkg != NULL ? curpkg : " ", reqpkg);
		}
		if (((pkgname = xbps_pkgpattern_name(reqpkg)) == NULL) &&
		    ((pkgname = xbps_pkg_name(reqpkg)) == NULL)) {
			xbps_dbg_printf(xhp, "can't guess pkgname for %s\n",
			    reqpkg);
			rv = EINVAL;
			break;
		}
		/*
		 * Pass 1: check if required dependency is already installed
		 * and its version is fully matched.
		 */
		if (((tmpd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) &&
		    ((tmpd = xbps_pkgdb_get_virtualpkg(xhp, pkgname)) == NULL)) {
			free(pkgname);
			if (errno && errno != ENOENT) {
				/* error */
				rv = errno;
				xbps_dbg_printf(xhp, "failed to find "
				    "installed pkg for `%s': %s\n",
				    reqpkg, strerror(errno));
				break;
			}
			/* Required pkgdep not installed */
			xbps_dbg_printf_append(xhp, "not installed ");
			reason = "install";
			state = XBPS_PKG_STATE_NOT_INSTALLED;
		} else {
			free(pkgname);
			/*
			 * Check if installed version matches the
			 * required pkgdep version.
			 */
			xbps_dictionary_get_cstring_nocopy(tmpd,
			    "pkgver", &pkgver_q);

			/* Check its state */
			if ((rv = xbps_pkg_state_dictionary(tmpd, &state)) != 0)
				break;
			if (xbps_match_virtual_pkg_in_dict(tmpd,reqpkg,true)) {
				/*
				 * Check if required dependency is a virtual
				 * package and is satisfied by an
				 * installed package.
				 */
				xbps_dbg_printf_append(xhp,
				    "[virtual] satisfied by "
				    "`%s'.\n", pkgver_q);
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
				} else if (state == XBPS_PKG_STATE_INSTALLED) {
					/*
					 * Package matches the dependency
					 * pattern and is fully installed,
					 * skip to next one.
					 */
					xbps_dbg_printf_append(xhp,
					    "installed "
					    "`%s'.\n", pkgver_q);
					continue;
				}
			} else {
				/* error matching pkgpattern */
				xbps_dbg_printf(xhp, "failed to match "
				    "pattern %s with %s\n", reqpkg, pkgver_q);
				break;
			}
		}
		/*
		 * Pass 2: check if required dependency has been already
		 * added in the transaction dictionary.
		 */
		if ((curpkgd = xbps_find_pkg_in_array(unsorted, reqpkg)) ||
		    (curpkgd = xbps_find_virtualpkg_in_array(xhp, unsorted, reqpkg))) {
			xbps_dictionary_get_cstring_nocopy(curpkgd,
			    "pkgver", &pkgver_q);
			xbps_dbg_printf_append(xhp, " (%s queued)\n", pkgver_q);
			continue;
		}
		/*
		 * Pass 3: find required dependency in repository pool.
		 * If dependency does not match add pkg into the missing
		 * deps array and pass to next one.
		 */
		if (((curpkgd = xbps_rpool_get_pkg(xhp, reqpkg)) == NULL) &&
		    ((curpkgd = xbps_rpool_get_virtualpkg(xhp, reqpkg)) == NULL)) {
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
		xbps_dictionary_get_cstring_nocopy(curpkgd,
		    "pkgver", &pkgver_q);
		reqpkgname = xbps_pkg_name(pkgver_q);
		assert(reqpkgname);
		/*
		 * Check dependency validity.
		 */
		pkgname = xbps_pkg_name(curpkg);
		assert(pkgname);
		if (strcmp(pkgname, reqpkgname) == 0) {
			xbps_dbg_printf_append(xhp, "[ignoring wrong dependency "
			    "%s (depends on itself)]\n",
			    reqpkg);
			free(pkgname);
			free(reqpkgname);
			continue;
		}
		free(pkgname);
		free(reqpkgname);

		/*
		 * Check if package has matched conflicts.
		 */
		xbps_pkg_find_conflicts(xhp, unsorted, curpkgd);
		/*
		 * Package is on repo, add it into the transaction dictionary.
		 */
		xbps_dictionary_set_cstring_nocopy(curpkgd, "transaction", reason);
		rv = store_dependency(xhp, unsorted, curpkgd, state);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "store_dependency failed for "
			    "`%s': %s\n", reqpkg, strerror(rv));
			break;
		}
		/*
		 * If package doesn't have rundeps, pass to the next one.
		 */
		curpkgrdeps = xbps_dictionary_get(curpkgd, "run_depends");
		if (curpkgrdeps == NULL)
			continue;

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
		rv = find_repo_deps(xhp, unsorted, curpkgrdeps,
				pkgver_q, depth);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "Error checking %s for rundeps: %s\n",
			    reqpkg, strerror(rv));
			break;
		}
	}
	xbps_object_iterator_release(iter);
	(*depth)--;

	return rv;
}

int HIDDEN
xbps_repository_find_deps(struct xbps_handle *xhp,
			  xbps_array_t unsorted,
			  xbps_dictionary_t repo_pkgd)
{
	xbps_array_t pkg_rdeps;
	const char *pkgver;
	unsigned short depth = 0;

	pkg_rdeps = xbps_dictionary_get(repo_pkgd, "run_depends");
	if (xbps_array_count(pkg_rdeps) == 0)
		return 0;

	xbps_dictionary_get_cstring_nocopy(repo_pkgd, "pkgver", &pkgver);
	xbps_dbg_printf(xhp, "Finding required dependencies for '%s':\n", pkgver);
	/*
	 * This will find direct and indirect deps, if any of them is not
	 * there it will be added into the missing_deps array.
	 */
	return find_repo_deps(xhp, unsorted, pkg_rdeps, pkgver, &depth);
}
