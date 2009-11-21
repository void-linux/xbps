/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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

static int add_missing_reqdep(prop_dictionary_t, const char *, const char *);
static int find_repo_deps(prop_dictionary_t, prop_dictionary_t,
			  const char *, prop_array_t);

static int
store_dependency(prop_dictionary_t master, prop_dictionary_t depd,
		 const char *repoloc)
{
	prop_dictionary_t dict;
	prop_array_t array;
	const char *pkgname;
	int rv = 0;
	pkg_state_t state = 0;

	assert(master != NULL);
	assert(depd != NULL);
	assert(repoloc != NULL);
	/*
	 * Get some info about dependencies and current repository.
	 */
	prop_dictionary_get_cstring_nocopy(depd, "pkgname", &pkgname);

	dict = prop_dictionary_copy(depd);
	if (dict == NULL)
		return errno;

	array = prop_dictionary_get(master, "unsorted_deps");
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
	rv = xbps_get_pkg_state_installed(pkgname, &state);
	if (rv == 0) {
		if ((rv = xbps_set_pkg_state_dictionary(dict, state)) != 0) {
			prop_object_release(dict);
			return rv;
		}
	}
	/*
	 * Add required objects into package dep's dictionary.
	 */
	prop_dictionary_set_cstring(dict, "repository", repoloc);
	/*
	 * Remove some unneeded objects.
	 */
	prop_dictionary_remove(dict, "conf_files");
	prop_dictionary_remove(dict, "maintainer");
	prop_dictionary_remove(dict, "long_desc");

	/*
	 * Add the dictionary into the array.
	 */
	if (!xbps_add_obj_to_array(array, dict)) {
		prop_object_release(dict);
		return EINVAL;
	}

	return 0;
}

static int
add_missing_reqdep(prop_dictionary_t master, const char *pkgname,
		   const char *version)
{
	prop_array_t missing_rdeps;
	prop_dictionary_t mdepd;

	assert(array != NULL);
	assert(reqdep != NULL);

	if (xbps_find_pkg_in_dict(master, "missing_deps", pkgname))
		return EEXIST;

	mdepd = prop_dictionary_create();
	if (mdepd == NULL)
		return errno;

	missing_rdeps = prop_dictionary_get(master, "missing_deps");
	prop_dictionary_set_cstring(mdepd, "pkgname", pkgname);
	prop_dictionary_set_cstring(mdepd, "version", version);

	if (!xbps_add_obj_to_array(missing_rdeps, mdepd)) {
		prop_object_release(mdepd);
		return EINVAL;
	}

	return 0;
}

int SYMEXPORT
xbps_find_deps_in_pkg(prop_dictionary_t master, prop_dictionary_t pkg)
{
	prop_array_t pkg_rdeps, missing_rdeps;
	struct repository_data *rdata;
	const char *pkgname;
	int rv = 0;

	assert(pkg != NULL);
	assert(iter != NULL);

	pkg_rdeps = prop_dictionary_get(pkg, "run_depends");
	if (pkg_rdeps == NULL)
		return 0;

	prop_dictionary_get_cstring_nocopy(pkg, "pkgname", &pkgname);
	DPRINTF(("Checking rundeps for %s.\n", pkgname));
	/*
	 * Iterate over the repository pool and find out if we have
	 * all available binary packages.
	 */
	SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
		/*
		 * This will find direct and indirect deps,
		 * if any of them is not there it will be added
		 * into the missing_deps array.
		 */
		rv = find_repo_deps(master, rdata->rd_repod,
		    rdata->rd_uri, pkg_rdeps);
		if (rv != 0)
			break;
	}

	/*
	 * If there are no missing deps, there's nothing to do.
	 */
	missing_rdeps = prop_dictionary_get(master, "missing_deps");
	if (prop_array_count(missing_rdeps) == 0)
		return 0;

	/*
	 * Iterate one more time, but this time with missing deps
	 * that were found in previous pass.
	 */
	DPRINTF(("Checking for missing deps in %s.\n", pkgname)); 
	SIMPLEQ_FOREACH(rdata, &repodata_queue, chain) {
		rv = find_repo_deps(master, rdata->rd_repod,
		    rdata->rd_uri, missing_rdeps);
		if (rv != 0)
			break;
	}

	return rv;
}

static int
find_repo_deps(prop_dictionary_t master, prop_dictionary_t repo,
	       const char *repoloc, prop_array_t array)
{
	prop_dictionary_t curpkgd, tmpd = NULL;
	prop_array_t curpkg_rdeps;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *reqpkg, *reqvers;
	char *pkgname;
	int rv = 0;

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return ENOMEM;

	/*
	 * Iterate over the list of required run dependencies for
	 * current package.
	 */
	while ((obj = prop_object_iterator_next(iter))) {
		reqpkg = prop_string_cstring_nocopy(obj);
		/*
		 * Check if required dep is satisfied and installed.
		 */
		rv = xbps_check_is_installed_pkg(reqpkg);
		if (rv == -1) {
			/* There was an error checking it... */
			break;
		} else if (rv == 1) {
			/* pkgdep is satisfied */
			DPRINTF(("Dependency %s satisfied.\n", reqpkg));
			rv = 0;
			continue;
		}
		DPRINTF(("Dependency %s not mached.\n", reqpkg));
		pkgname = xbps_get_pkgdep_name(reqpkg);
		if (pkgname == NULL) {
			rv = EINVAL;
			break;
		}
		reqvers = xbps_get_pkgdep_version(reqpkg);
		if (reqvers == NULL) {
			free(pkgname);
			rv = EINVAL;
			break;
		}
		/*
		 * Check if package is already added in the
		 * array of unsorted deps.
		 */
		if (xbps_find_pkg_in_dict(master, "unsorted_deps", pkgname)) {
			DPRINTF(("Dependency %s already queued.\n", pkgname));
			free(pkgname);
			continue;
		}

		/*
		 * If required package is not in repo, add it into the
		 * missing deps array and pass to the next one.
		 */
		curpkgd = xbps_find_pkg_in_dict(repo, "packages", pkgname);
		if (curpkgd == NULL) {
			rv = add_missing_reqdep(master, pkgname, reqvers);
			if (rv != 0 && rv != EEXIST) {
				free(pkgname);
				break;
			} else if (rv == EEXIST) {
				DPRINTF(("Missing dep %s already added.\n",
				    reqpkg));
				rv = 0;
				free(pkgname);
				continue;
			} else {
				DPRINTF(("Added missing dep %s (repo: %s).\n",
				    pkgname, repoloc));
				free(pkgname);
				continue;
			}
		}
		/*
		 * If package is installed but version doesn't satisfy
		 * the dependency mark it as an update, otherwise as
		 * an install.
		 */
		tmpd = xbps_find_pkg_installed_from_plist(pkgname);
		if (tmpd != NULL) {
			prop_dictionary_set_cstring_nocopy(curpkgd,
			    "trans-action", "update");
			prop_object_release(tmpd);
		} else {
			prop_dictionary_set_cstring_nocopy(curpkgd,
			    "trans-action", "install");
		}
		/*
		 * Package is on repo, add it into the dictionary.
		 */
		if ((rv = store_dependency(master, curpkgd, repoloc)) != 0) {
			free(pkgname);
			break;
		}
		DPRINTF(("Added reqdep %s (repo: %s)\n", pkgname, repoloc));

		/*
		 * If package was added in the missing_deps array, we
		 * can remove it now it has been found in current repository.
		 */
		rv = xbps_remove_pkg_from_dict(master, "missing_deps", pkgname);
		if (rv == 0)
			DPRINTF(("Removed missing dep %s.\n", pkgname));
		else if (rv != 0 && rv != ENOENT) {
			free(pkgname);
			break;
		}
		free(pkgname);

		/*
		 * If package doesn't have rundeps, pass to the next one.
		 */
		curpkg_rdeps = prop_dictionary_get(curpkgd, "run_depends");
		if (curpkg_rdeps == NULL)
			continue;

		/*
		 * Iterate on required pkg to find more deps.
		 */
		DPRINTF(("Looking for rundeps on %s.\n", reqpkg));
		if (!find_repo_deps(master, repo, repoloc, curpkg_rdeps))
			continue;
	}
	prop_object_iterator_release(iter);

	return rv;
}
