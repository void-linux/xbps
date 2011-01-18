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
store_dependency(prop_dictionary_t trans_dict, prop_dictionary_t repo_pkgd)
{
	prop_dictionary_t dict;
	prop_array_t array;
	const char *pkgname, *pkgver, *repoloc;
	int rv = 0;
	pkg_state_t state = 0;

	assert(trans_dict != NULL);
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

	array = prop_dictionary_get(trans_dict, "unsorted_deps");
	if (array == NULL) {
		prop_object_release(dict);
		return errno;
	}
	/*
	 * Always set "not-installed" package state. Will be overwritten
	 * to its correct state later.
	 */
	xbps_dbg_printf_append("\n");
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
remove_missing_reqdep(prop_array_t missing_rdeps, const char *reqpkg)
{
	prop_object_iterator_t iter = NULL;
	prop_object_t obj;
	size_t idx = 0;
	int rv = 0;
	bool found = false;

	assert(missing_rdeps != NULL);
	assert(reqpkg != NULL);

	iter = prop_array_iterator(missing_rdeps);
	if (iter == NULL)
		return errno;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		const char *curdep;
		char *curpkgnamedep, *reqpkgname;

		curdep = prop_string_cstring_nocopy(obj);
		curpkgnamedep = xbps_get_pkgpattern_name(curdep);
		if (curpkgnamedep == NULL) {
			rv = errno;
			goto out;
		}
		reqpkgname = xbps_get_pkgpattern_name(reqpkg);
		if (reqpkgname == NULL) {
			free(curpkgnamedep);
			rv = errno;
			goto out;
		}
		if (strcmp(reqpkgname, curpkgnamedep) == 0)
			found = true;

		free(curpkgnamedep);
		free(reqpkgname);
		if (found)
			break;
		idx++;
	}
out:
	prop_object_iterator_release(iter);
	if (found) {
		prop_array_remove(missing_rdeps, idx);
		return 0;
	}
	if (rv == 0)
		rv = ENOENT;

	return rv;
}

static int
find_repo_deps(prop_dictionary_t trans_dict,	/* transaction dictionary */
	       prop_array_t mrdeps,		/* missing rundeps array */
	       const char *originpkgn,		/* origin pkgname */
	       prop_array_t pkg_rdeps_array)	/* current pkg rundeps array  */
{
	prop_dictionary_t curpkgd, tmpd;
	prop_array_t curpkg_rdeps;
	prop_object_t obj;
	prop_object_iterator_t iter;
	pkg_state_t state = 0;
	const char *reqpkg, *reqvers, *pkg_queued;
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
		curpkgd = NULL;
		reqpkg = prop_string_cstring_nocopy(obj);
		if (reqpkg == NULL) {
			rv = EINVAL;
			break;
		}
		if (originpkgn)
			xbps_dbg_printf("  %s requires dependency '%s' "
			    "[direct]: ", originpkgn, reqpkg);
		else
			xbps_dbg_printf("    requires dependency '%s' "
			    "[indirect]: ", reqpkg);

		/*
		 * Check if required dep is satisfied and installed.
		 */
		rv = xbps_check_is_installed_pkg(reqpkg);
		if (rv == -1) {
			/* There was an error checking it... */
			xbps_dbg_printf_append("error matching reqdep %s\n",
			    reqpkg);
			break;
		} else if (rv == 1) {
			/* Required pkg dependency is satisfied */
			xbps_dbg_printf_append("satisfied and installed.\n");
			rv = 0;
			continue;
		}
		pkgname = xbps_get_pkgpattern_name(reqpkg);
		if (pkgname == NULL) {
			rv = EINVAL;
			break;
		}
		reqvers = xbps_get_pkgpattern_version(reqpkg);
		if (reqvers == NULL) {
			free(pkgname);
			rv = EINVAL;
			break;
		}
		/*
		 * Check if package is already added in the
		 * array of unsorted deps, and check if current required
		 * dependency pattern is matched.
		 */
		curpkgd = xbps_find_pkg_in_dict_by_name(trans_dict,
		    "unsorted_deps", pkgname);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				free(pkgname);
				rv = errno;
				break;
			}
		} else {
			prop_dictionary_get_cstring_nocopy(curpkgd,
			    "pkgver", &pkg_queued);
			if (xbps_pkgpattern_match(pkg_queued,
			    __UNCONST(reqpkg))) {
				xbps_dbg_printf_append(
				    "queued in the transaction.\n");
				free(pkgname);
				continue;
			}
			curpkgd = NULL;
		}

		/*
		 * If required pkgdep is not in repo, add it into the
		 * missing deps array and pass to the next one.
		 */
		curpkgd = xbps_repository_pool_find_pkg(reqpkg, true, false);
		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				free(pkgname);
				rv = errno;
				break;
			}

			rv = add_missing_reqdep(mrdeps, reqpkg);
			if (rv != 0 && rv != EEXIST) {
				xbps_dbg_printf_append("add missing reqdep "
				    "failed %s\n", reqpkg);
				free(pkgname);
				break;
			} else if (rv == EEXIST) {
				xbps_dbg_printf_append("missing dep %s "
				    "already added.\n", reqpkg);
				rv = 0;
				free(pkgname);
				continue;
			} else {
				xbps_dbg_printf_append(
				    "missing dep '%s' in repository!\n",
				    reqpkg);
				free(pkgname);
				continue;
			}
		}
		/*
		 * If package is installed but version doesn't satisfy
		 * the dependency mark it as an update, otherwise as
		 * an install. Packages that were unpacked previously
		 * will be marked as pending to be configured.
		 */
		tmpd = xbps_find_pkg_dict_installed(reqpkg, true);
		if (tmpd == NULL) {
			if (errno && errno != ENOENT) {
				free(pkgname);
				prop_object_release(curpkgd);
				rv = errno;
				break;
			}
			prop_dictionary_set_cstring_nocopy(curpkgd,
			    "trans-action", "install");
		} else if (tmpd) {
			rv = xbps_get_pkg_state_installed(pkgname, &state);
			if (rv != 0) {
				free(pkgname);
				prop_object_release(tmpd);
				prop_object_release(curpkgd);
				break;
			}
			if (state == XBPS_PKG_STATE_INSTALLED)
				prop_dictionary_set_cstring_nocopy(curpkgd,
				    "trans-action", "update");
			else
				prop_dictionary_set_cstring_nocopy(curpkgd,
				    "trans-action", "configure");

			prop_object_release(tmpd);
		}
		/*
		 * Package is on repo, add it into the dictionary.
		 */
		if ((rv = store_dependency(trans_dict, curpkgd)) != 0) {
			xbps_dbg_printf("store_dependency failed %s",
			    reqpkg);
			free(pkgname);
			prop_object_release(curpkgd);
			break;
		}

		/*
		 * If package was added in the missing_deps array, we
		 * can remove it now it has been found in current repository.
		 */
		rv = remove_missing_reqdep(mrdeps, reqpkg);
		if (rv == ENOENT) {
			rv = 0;
		} else if (rv == 0) {
			xbps_dbg_printf("Removed missing dep %s.\n", reqpkg);
		} else {
			xbps_dbg_printf("Removing missing dep %s "
			    "returned %s\n", reqpkg, strerror(rv));
			free(pkgname);
			prop_object_release(curpkgd);
			break;
		}

		/*
		 * If package doesn't have rundeps, pass to the next one.
		 */
		curpkg_rdeps = prop_dictionary_get(curpkgd, "run_depends");
		if (curpkg_rdeps == NULL) {
			free(pkgname);
			prop_object_release(curpkgd);
			continue;
		}
		prop_object_release(curpkgd);

		/*
		 * Iterate on required pkg to find more deps.
		 */
		xbps_dbg_printf_append("\n");
		xbps_dbg_printf("Finding dependencies for '%s-%s' [%s]:\n",
			    pkgname, reqvers, originpkgn ? "direct" : "indirect");

		free(pkgname);
		rv = find_repo_deps(trans_dict, mrdeps, NULL, curpkg_rdeps);
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
xbps_repository_find_pkg_deps(prop_dictionary_t trans_dict,
			      prop_dictionary_t repo_pkgd)
{
	prop_array_t pkg_rdeps, missing_rdeps;
	const char *pkgname, *pkgver;
	int rv = 0;

	assert(trans_dict != NULL);
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
	missing_rdeps = prop_dictionary_get(trans_dict, "missing_deps");
	rv = find_repo_deps(trans_dict, missing_rdeps, pkgname, pkg_rdeps);
	if (rv != 0) {
		xbps_dbg_printf("Error '%s' while checking rundeps!\n",
		    strerror(rv));
	}

	return rv;
}
