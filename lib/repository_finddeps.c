/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "xbps_api_impl.h"

static int
store_dependency(prop_dictionary_t transd, prop_dictionary_t repo_pkgd)
{
	prop_dictionary_t dict;
	prop_array_t array;
	const char *pkgname, *pkgver, *repoloc;
	int rv = 0;
	pkg_state_t state = 0;

	assert(transd != NULL);
	assert(repo_pkgd != NULL);
	/*
	 * Get some info about dependencies and current repository.
	 */
	prop_dictionary_get_cstring_nocopy(repo_pkgd, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(repo_pkgd, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(repo_pkgd, "repository", &repoloc);

	dict = prop_dictionary_copy(repo_pkgd);
	if (dict == NULL)
		return errno;

	array = prop_dictionary_get(transd, "unsorted_deps");
	if (array == NULL) {
		prop_object_release(dict);
		return errno;
	}
	/*
	 * Always set "not-installed" package state. Will be overwritten
	 * to its correct state later.
	 */
	rv = xbps_set_pkg_state_dictionary(dict, XBPS_PKG_STATE_NOT_INSTALLED);
	if (rv != 0) {
		prop_object_release(dict);
		return rv;
	}
	/*
	 * Overwrite package state in dictionary if it was unpacked
	 * previously.
	 */
	if ((rv = xbps_get_pkg_state_installed(pkgname, &state)) == 0) {
		if ((rv = xbps_set_pkg_state_dictionary(dict, state)) != 0) {
			prop_object_release(dict);
			return rv;
		}
	}
	/*
	 * Add required objects into package dep's dictionary.
	 */
	if (!prop_dictionary_set_bool(dict, "automatic-install", true)) {
		prop_object_release(dict);
		return errno;
	}
	/*
	 * Add the dictionary into the array.
	 */
	if (!xbps_add_obj_to_array(array, dict)) {
		prop_object_release(dict);
		return EINVAL;
	}
	xbps_dbg_printf("Added package '%s' into "
	    "the transaction (%s).\n", pkgver, repoloc);

	return 0;
}

static int
add_missing_reqdep(prop_array_t missing_rdeps, const char *reqpkg)
{
	prop_string_t reqpkg_str;
	prop_object_iterator_t iter = NULL;
	prop_object_t obj;
	size_t idx = 0;
	bool add_pkgdep, pkgfound, update_pkgdep;
	int rv = 0;

	assert(missing_rdeps != NULL);
	assert(reqpkg != NULL);

	add_pkgdep = update_pkgdep = pkgfound = false;

	reqpkg_str = prop_string_create_cstring_nocopy(reqpkg);
	if (reqpkg_str == NULL)
		return errno;

	iter = prop_array_iterator(missing_rdeps);
	if (iter == NULL)
		goto out;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		const char *curdep, *curver, *pkgver;
		char *curpkgnamedep = NULL, *pkgnamedep = NULL;

		assert(prop_object_type(obj) == PROP_TYPE_STRING);
		curdep = prop_string_cstring_nocopy(obj);
		curver = xbps_get_pkgpattern_version(curdep);
		pkgver = xbps_get_pkgpattern_version(reqpkg);
		if (curver == NULL || pkgver == NULL)
			goto out;
		curpkgnamedep = xbps_get_pkgpattern_name(curdep);
		if (curpkgnamedep == NULL)
			goto out;
		pkgnamedep = xbps_get_pkgpattern_name(reqpkg);
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
			xbps_dbg_printf("Missing pkgdep name matched, "
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
		prop_array_remove(missing_rdeps, idx);
	if (add_pkgdep && !xbps_add_obj_to_array(missing_rdeps, reqpkg_str)) {
		prop_object_release(reqpkg_str);
		return errno;
	}

	return rv;
}

static int
find_repo_deps(prop_dictionary_t transd,	/* transaction dictionary */
	       prop_array_t mrdeps,		/* missing rundeps array */
	       const char *originpkgn,		/* origin pkgname */
	       prop_array_t pkg_rdeps_array)	/* current pkg rundeps array  */
{
	prop_dictionary_t curpkgd, tmpd;
	prop_array_t curpkg_rdeps;
	prop_object_t obj;
	prop_object_iterator_t iter;
	pkg_state_t state = 0;
	const char *reqpkg, *pkgver_q, *repopkgver;
	char *pkgname;
	int rv = 0;

	iter = prop_array_iterator(pkg_rdeps_array);
	if (iter == NULL)
		return ENOMEM;

	/*
	 * Iterate over the list of required run dependencies for
	 * current package.
	 */
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		tmpd = curpkgd = NULL;
		reqpkg = prop_string_cstring_nocopy(obj);
		if (reqpkg == NULL) {
			rv = EINVAL;
			break;
		}
		if (originpkgn)
			xbps_dbg_printf("'%s' requires dependency '%s' "
			    "[direct]: ", originpkgn, reqpkg);
		else
			xbps_dbg_printf("    Requires dependency '%s' "
			    "[indirect]: ", reqpkg);

		/*
		 * Check if package is already added in the
		 * array of unsorted deps, and check if current required
		 * dependency pattern is matched.
		 */
		curpkgd = xbps_find_pkg_in_dict_by_pattern(transd,
		    "unsorted_deps", reqpkg);
		if (curpkgd != NULL) {
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "pkgver", &pkgver_q);
			xbps_dbg_printf_append("`%s' queued "
			    "in the transaction.\n", pkgver_q);
			continue;
		} else {
			/* error matching required pkgdep */
			if (errno && errno != ENOENT) {
				rv = errno;
				break;
			}
		}
		/*
		 * If required pkgdep is not in repo, add it into the
		 * missing deps array and pass to the next one.
		 */
		curpkgd = xbps_repository_pool_find_pkg(reqpkg, true, false);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				rv = errno;
				break;
			}

			rv = add_missing_reqdep(mrdeps, reqpkg);
			if (rv != 0 && rv != EEXIST) {
				xbps_dbg_printf_append("`%s': "
				    "add_missing_reqdep failed %s\n", reqpkg);
				break;
			} else if (rv == EEXIST) {
				xbps_dbg_printf_append("`%s' missing dep "
				    "already added.\n", reqpkg);
				rv = 0;
				continue;
			} else {
				xbps_dbg_printf_append("`%s' added "
				    "into the missing deps array.\n", reqpkg);
				continue;
			}
		}
		pkgname = xbps_get_pkgpattern_name(reqpkg);
		if (pkgname == NULL) {
			prop_object_release(curpkgd);
			rv = EINVAL;
			break;
		}
		/*
		 * Check if required pkgdep is installed and matches
		 * the required version.
		 */
		tmpd = xbps_find_pkg_dict_installed(pkgname, false);
		if (tmpd == NULL) {
			free(pkgname);
			if (errno && errno != ENOENT) {
				/* error */
				rv = errno;
				prop_object_release(curpkgd);
				break;
			}
			/* Required pkgdep not installed */
			prop_dictionary_set_cstring_nocopy(curpkgd,
			    "trans-action", "install");
			xbps_dbg_printf_append("not installed.\n");
		} else {
			/*
			 * Check if installed version matches the
			 * required pkgdep version.
			 */
			prop_dictionary_get_cstring_nocopy(tmpd,
			    "pkgver", &pkgver_q);

			/* Check its state */
			rv = xbps_get_pkg_state_installed(pkgname, &state);
			if (rv != 0) {
				free(pkgname);
				prop_object_release(tmpd);
				prop_object_release(curpkgd);
				break;
			}
			free(pkgname);
			rv = xbps_pkgpattern_match(pkgver_q, __UNCONST(reqpkg));
			if (rv == 0) {
				/*
				 * Package is installed but does not match
				 * the dependency pattern, an update
				 * needs to be installed.
				 */
				prop_dictionary_get_cstring_nocopy(curpkgd,
				    "version", &repopkgver);
				xbps_dbg_printf_append("installed `%s', "
				    "updating to `%s'...\n",
				    pkgver_q, repopkgver);
				prop_dictionary_set_cstring_nocopy(curpkgd,
				    "trans-action", "update");
			} else if (rv == 1) {
				rv = 0;
				if (state == XBPS_PKG_STATE_UNPACKED) {
					/*
					 * Package matches the dependency
					 * pattern but was only unpacked,
					 * mark pkg to be configured.
					 */
					prop_dictionary_set_cstring_nocopy(
					    curpkgd, "trans-action",
					    "configure");
					xbps_dbg_printf_append("installed `%s'"
					    ", but needs to be configured...\n",
					    pkgver_q);
				} else {
					/*
					 * Package matches the dependency
					 * pattern and is fully installed,
					 * skip and pass to next one.
					 */
					xbps_dbg_printf_append("installed "
					    "`%s'.\n", pkgver_q);
					prop_object_release(tmpd);
					prop_object_release(curpkgd);
					continue;
				}
			} else {
				/* error matching pkgpattern */
				prop_object_release(tmpd);
				prop_object_release(curpkgd);
				break;
			}
		}
		/*
		 * Package is on repo, add it into the dictionary.
		 */
		if ((rv = store_dependency(transd, curpkgd)) != 0) {
			xbps_dbg_printf("store_dependency failed %s",
			    reqpkg);
			prop_object_release(curpkgd);
			break;
		}
		/*
		 * If package doesn't have rundeps, pass to the next one.
		 */
		curpkg_rdeps = prop_dictionary_get(curpkgd, "run_depends");
		if (curpkg_rdeps == NULL) {
			prop_object_release(curpkgd);
			continue;
		}
		prop_object_release(curpkgd);
		/*
		 * Iterate on required pkg to find more deps.
		 */
		xbps_dbg_printf("%sFinding dependencies for '%s' [%s]:\n",
		    originpkgn ? "" : "  ", reqpkg,
		    originpkgn ? "direct" : "indirect");
		rv = find_repo_deps(transd, mrdeps, NULL, curpkg_rdeps);
		if (rv != 0) {
			xbps_dbg_printf("Error checking %s for rundeps: %s\n",
			    reqpkg, strerror(rv));
			break;
		}
	}
	prop_object_iterator_release(iter);

	return rv;
}

int HIDDEN
xbps_repository_find_pkg_deps(prop_dictionary_t transd,
			      prop_array_t mdeps,
			      prop_dictionary_t repo_pkgd)
{
	prop_array_t pkg_rdeps;
	const char *pkgname, *pkgver;
	int rv = 0;

	assert(transd != NULL);
	assert(mdeps != NULL);
	assert(repo_pkgd != NULL);

	pkg_rdeps = prop_dictionary_get(repo_pkgd, "run_depends");
	if (pkg_rdeps == NULL)
		return 0;

	prop_dictionary_get_cstring_nocopy(repo_pkgd, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(repo_pkgd, "pkgver", &pkgver);
	xbps_dbg_printf("Finding required dependencies for '%s':\n", pkgver);
	/*
	 * This will find direct and indirect deps, if any of them is not
	 * there it will be added into the missing_deps array.
	 */
	if ((rv = find_repo_deps(transd, mdeps, pkgname, pkg_rdeps)) != 0) {
		xbps_dbg_printf("Error '%s' while checking rundeps!\n",
		    strerror(rv));
	}

	return rv;
}
