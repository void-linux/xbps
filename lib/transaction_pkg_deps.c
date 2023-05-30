/*-
 * Copyright (c) 2008-2020 Juan Romero Pardines.
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
add_missing_reqdep(struct xbps_handle *xhp, const char *reqpkg)
{
	xbps_array_t mdeps;
	xbps_object_iterator_t iter = NULL;
	xbps_object_t obj;
	unsigned int idx = 0;
	bool add_pkgdep, pkgfound, update_pkgdep;
	int rv = 0;

	assert(reqpkg != NULL);

	add_pkgdep = update_pkgdep = pkgfound = false;
	mdeps = xbps_dictionary_get(xhp->transd, "missing_deps");

	iter = xbps_array_iterator(mdeps);
	if (iter == NULL)
		goto out;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		const char *curdep, *curver, *pkgver;
		char curpkgnamedep[XBPS_NAME_SIZE];
		char pkgnamedep[XBPS_NAME_SIZE];

		assert(xbps_object_type(obj) == XBPS_TYPE_STRING);
		curdep = xbps_string_cstring_nocopy(obj);
		curver = xbps_pkgpattern_version(curdep);
		pkgver = xbps_pkgpattern_version(reqpkg);
		if (curver == NULL || pkgver == NULL)
			goto out;
		if (!xbps_pkgpattern_name(curpkgnamedep, XBPS_NAME_SIZE, curdep)) {
			goto out;
		}
		if (!xbps_pkgpattern_name(pkgnamedep, XBPS_NAME_SIZE, reqpkg)) {
			goto out;
		}
		if (strcmp(pkgnamedep, curpkgnamedep) == 0) {
			pkgfound = true;
			if (strcmp(curver, pkgver) == 0) {
				rv = EEXIST;
				goto out;
			}
			/*
			 * if new dependency version is greater than current
			 * one, store it.
			 */
			xbps_dbg_printf("Missing pkgdep name matched, curver: %s newver: %s\n", curver, pkgver);
			if (xbps_cmpver(curver, pkgver) <= 0) {
				add_pkgdep = false;
				rv = EEXIST;
				goto out;
			}
			update_pkgdep = true;
		}
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
	if (add_pkgdep) {
		char *str;

		str = xbps_xasprintf("MISSING: %s", reqpkg);
		xbps_array_add_cstring(mdeps, str);
		free(str);
	}

	return rv;
}

#define MAX_DEPTH	512

static int
repo_deps(struct xbps_handle *xhp,
	  xbps_array_t pkgs,		/* array of pkgs */
	  xbps_dictionary_t pkg_repod,	/* pkg repo dictionary */
	  unsigned short *depth)	/* max recursion depth */
{
	xbps_array_t pkg_rdeps = NULL, pkg_provides = NULL;
	xbps_dictionary_t curpkgd = NULL, repopkgd = NULL;
	xbps_trans_type_t ttype;
	pkg_state_t state;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *curpkg = NULL, *reqpkg = NULL, *pkgver_q = NULL;
	char pkgname[XBPS_NAME_SIZE], reqpkgname[XBPS_NAME_SIZE];
	int rv = 0;

	assert(xhp);
	assert(pkgs);
	assert(pkg_repod);

	if (*depth >= MAX_DEPTH)
		return ELOOP;

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &curpkg);
	pkg_provides = xbps_dictionary_get(pkg_repod, "provides");
	/*
	 * Iterate over the list of required run dependencies for
	 * current package.
	 */
	pkg_rdeps = xbps_dictionary_get(pkg_repod, "run_depends");
	if (xbps_array_count(pkg_rdeps) == 0)
		goto out;

	iter = xbps_array_iterator(pkg_rdeps);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		bool error = false, foundvpkg = false;
		bool autoinst = true;

		ttype = XBPS_TRANS_UNKNOWN;
		reqpkg = xbps_string_cstring_nocopy(obj);

		if (xhp->flags & XBPS_FLAG_DEBUG) {
			xbps_dbg_printf("%s", "");
			for (unsigned short x = 0; x < *depth; x++) {
				xbps_dbg_printf_append(" ");
			}
			xbps_dbg_printf_append("%s: requires dependency '%s': ", curpkg ? curpkg : " ", reqpkg);
		}
		if ((!xbps_pkgpattern_name(pkgname, sizeof(pkgname), reqpkg)) &&
		    (!xbps_pkg_name(pkgname, sizeof(pkgname), reqpkg))) {
			xbps_dbg_printf("%s: can't guess pkgname for dependency: %s\n", curpkg, reqpkg);
			xbps_set_cb_state(xhp, XBPS_STATE_INVALID_DEP, ENXIO, NULL,
			    "%s: can't guess pkgname for dependency '%s'", curpkg, reqpkg);
			rv = ENXIO;
			break;
		}
		/*
		 * Pass 0: check if required dependency is ignored.
		 */
		if (xbps_pkg_is_ignored(xhp, pkgname)) {
			xbps_dbg_printf_append("%s ignored.\n", pkgname);
			continue;
		}
		/*
		 * Pass 1: check if required dependency is provided as virtual
		 * package via "provides", if true ignore dependency.
		 */
		if (pkg_provides && xbps_match_virtual_pkg_in_array(pkg_provides, reqpkg)) {
			xbps_dbg_printf_append("%s is a vpkg provided by %s, ignored.\n", pkgname, curpkg);
			continue;
		}
		/*
		 * Pass 2: check if required dependency has been already
		 * added in the transaction dictionary.
		 */
		if ((curpkgd = xbps_find_pkg_in_array(pkgs, reqpkg, 0)) ||
		    (curpkgd = xbps_find_virtualpkg_in_array(xhp, pkgs, reqpkg, 0))) {
			xbps_trans_type_t ttype_q = xbps_transaction_pkg_type(curpkgd);
			xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &pkgver_q);
			if (ttype_q != XBPS_TRANS_REMOVE && ttype_q != XBPS_TRANS_HOLD) {
				xbps_dbg_printf_append(" (%s queued %d)\n", pkgver_q, ttype_q);
				continue;
			}
		}
		/*
		 * Pass 3: check if required dependency is already installed
		 * and its version is fully matched.
		 */
		if ((curpkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
			if ((curpkgd = xbps_pkgdb_get_virtualpkg(xhp, pkgname))) {
				foundvpkg = true;
			}
		}
		if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
			/*
			 * if XBPS_FLAG_DOWNLOAD_ONLY always assume
			 * all deps are not installed. This way one can download
			 * the whole set of binary packages to perform an
			 * off-line installation later on.
			 */
			curpkgd = NULL;
		}

		if (curpkgd == NULL) {
			if (errno && errno != ENOENT) {
				/* error */
				rv = errno;
				xbps_dbg_printf("failed to find installed pkg for `%s': %s\n", reqpkg, strerror(rv));
				break;
			}
			/* Required dependency not installed */
			xbps_dbg_printf_append("not installed.\n");
			ttype = XBPS_TRANS_INSTALL;
			state = XBPS_PKG_STATE_NOT_INSTALLED;
		} else {
			/*
			 * Required dependency is installed, check if its version can
			 * satisfy the requirements.
			 */
			xbps_dictionary_get_cstring_nocopy(curpkgd, "pkgver", &pkgver_q);

			/* Check its state */
			if ((rv = xbps_pkg_state_dictionary(curpkgd, &state)) != 0) {
				break;
			}

			if (foundvpkg && xbps_match_virtual_pkg_in_dict(curpkgd, reqpkg)) {
				/*
				 * Check if required dependency is a virtual package and is satisfied
				 * by an installed package.
				 */
				xbps_dbg_printf_append("[virtual] satisfied by `%s'.\n", pkgver_q);
				continue;
			}
			rv = xbps_pkgpattern_match(pkgver_q, reqpkg);
			if (rv == 0) {
				char curpkgname[XBPS_NAME_SIZE];
				/*
				 * The version requirement is not satisfied.
				 */
				if (!xbps_pkg_name(curpkgname, sizeof(curpkgname), pkgver_q)) {
					abort();
				}

				if (strcmp(pkgname, curpkgname)) {
					xbps_dbg_printf_append("not installed `%s (vpkg)'", pkgver_q);
					if (xbps_dictionary_get(curpkgd, "hold")) {
						ttype = XBPS_TRANS_HOLD;
						xbps_dbg_printf_append(" on hold state! ignoring package.\n");
						rv = ENODEV;
					} else {
						xbps_dbg_printf_append("\n");
						ttype = XBPS_TRANS_INSTALL;
					}
				} else {
					xbps_dbg_printf_append("installed `%s', must be updated", pkgver_q);
					if (xbps_dictionary_get(curpkgd, "hold")) {
						xbps_dbg_printf_append(" on hold state! ignoring package.\n");
						ttype = XBPS_TRANS_HOLD;
						rv = ENODEV;
					} else {
						xbps_dbg_printf_append("\n");
						ttype = XBPS_TRANS_UPDATE;
					}
				}
				/*
				 * Not satisfied and package on hold.
				 */
				if (rv == ENODEV) {
					rv = add_missing_reqdep(xhp, reqpkg);
					if (rv != 0 && rv != EEXIST) {
						xbps_dbg_printf("`%s': add_missing_reqdep failed\n", reqpkg);
						break;
					} else if (rv == EEXIST) {
						xbps_dbg_printf("`%s' missing dep already added.\n", reqpkg);
						rv = 0;
						continue;
					} else {
						xbps_dbg_printf("`%s' added into the missing deps array.\n", reqpkg);
						continue;
					}
				}
			} else if (rv == 1) {
				/*
				 * The version requirement is satisfied.
				 */
				rv = 0;
				if (state == XBPS_PKG_STATE_UNPACKED) {
					/*
					 * Package matches the dependency pattern but was only unpacked,
					 * configure pkg.
					 */
					xbps_dbg_printf_append("installed `%s', must be configured.\n", pkgver_q);
					ttype = XBPS_TRANS_CONFIGURE;
				} else if (state == XBPS_PKG_STATE_INSTALLED) {
					/*
					 * Package matches the dependency pattern and is fully installed,
					 * skip to next one.
					 */
					xbps_dbg_printf_append("installed `%s'.\n", pkgver_q);
					continue;
				}
			} else {
				/* error matching pkgpattern */
				xbps_dbg_printf("failed to match pattern %s with %s\n", reqpkg, pkgver_q);
				break;
			}
		}
		/*
		 * Pass 4: find required dependency in repository pool.
		 * If dependency does not match add pkg into the missing
		 * deps array and pass to next one.
		 */
		if (xbps_dictionary_get(curpkgd, "repolock")) {
			const char *repourl = NULL;
			struct xbps_repo *repo = NULL;
			xbps_dbg_printf("`%s' is repolocked, looking at single repository.\n", reqpkg);
			xbps_dictionary_get_cstring_nocopy(curpkgd, "repository", &repourl);
			if (repourl && (repo = xbps_regget_repo(xhp, repourl))) {
				repopkgd = xbps_repo_get_pkg(repo, reqpkg);
			} else {
				repopkgd = NULL;
			}
		} else {
			repopkgd = xbps_rpool_get_pkg(xhp, reqpkg);
			if (!repopkgd) {
				repopkgd = xbps_rpool_get_virtualpkg(xhp, reqpkg);
			}
		}
		if (repopkgd == NULL) {
			/* pkg not found, there was some error */
			if (errno && errno != ENOENT) {
				xbps_dbg_printf("failed to find pkg for `%s' in rpool: %s\n", reqpkg, strerror(errno));
				rv = errno;
				break;
			}
			rv = add_missing_reqdep(xhp, reqpkg);
			if (rv != 0 && rv != EEXIST) {
				xbps_dbg_printf("`%s': add_missing_reqdep failed\n", reqpkg);
				break;
			} else if (rv == EEXIST) {
				xbps_dbg_printf("`%s' missing dep already added.\n", reqpkg);
				rv = 0;
				continue;
			} else {
				xbps_dbg_printf("`%s' added into the missing deps array.\n", reqpkg);
				continue;
			}
		}


		xbps_dictionary_get_cstring_nocopy(repopkgd, "pkgver", &pkgver_q);
		if (!xbps_pkg_name(reqpkgname, sizeof(reqpkgname), pkgver_q)) {
			rv = EINVAL;
			break;
		}
		/*
		 * Check dependency validity.
		 */
		if (!xbps_pkg_name(pkgname, sizeof(pkgname), curpkg)) {
			rv = EINVAL;
			break;
		}
		if (strcmp(pkgname, reqpkgname) == 0) {
			xbps_dbg_printf_append("[ignoring wrong dependency %s (depends on itself)]\n", reqpkg);
			xbps_remove_string_from_array(pkg_rdeps, reqpkg);
			continue;
		}
		/*
		 * Installed package must be updated, check if dependency is
		 * satisfied.
		 */
		if (ttype == XBPS_TRANS_UPDATE) {
			switch (xbps_pkgpattern_match(pkgver_q, reqpkg)) {
				case 0: /* nomatch */
					break;
				case 1: /* match */
					if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver_q)) {
						abort();
					}
					/*
					 * If there's an update in transaction,
					 * it's assumed version is greater.
					 * So dependency pattern matching didn't
					 * succeed... return ENODEV.
					 */
					if (xbps_find_pkg_in_array(pkgs, pkgname, XBPS_TRANS_UPDATE)) {
						error = true;
						rv = ENODEV;
					}
					break;
				default:
					error = true;
					rv = EINVAL;
					break;
			}
			if (error)
				break;
		}
		pkg_rdeps = xbps_dictionary_get(repopkgd, "run_depends");
		if (xbps_array_count(pkg_rdeps)) {
			/*
			 * Process rundeps for current pkg found in rpool.
			 */
			if (xhp->flags & XBPS_FLAG_DEBUG) {
				xbps_dbg_printf("%s", "");
				for (unsigned short x = 0; x < *depth; x++) {
					xbps_dbg_printf_append(" ");
				}
				xbps_dbg_printf_append("%s: finding dependencies:\n", pkgver_q);
			}
			(*depth)++;
			rv = repo_deps(xhp, pkgs, repopkgd, depth);
			if (rv != 0) {
				xbps_dbg_printf("Error checking %s for rundeps: %s\n", reqpkg, strerror(rv));
				break;
			}
		}
		if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
			ttype = XBPS_TRANS_DOWNLOAD;
		} else if (xbps_dictionary_get(curpkgd, "hold")) {
			ttype = XBPS_TRANS_HOLD;
		}
		if (ttype == XBPS_TRANS_UPDATE || ttype == XBPS_TRANS_CONFIGURE) {
			/*
			 * If the package is already installed preserve the installation mode,
			 * which is not automatic if automatic-install is not set.
			 */
			bool pkgd_auto = false;
			xbps_dictionary_get_bool(curpkgd, "automatic-install", &pkgd_auto);
			autoinst = pkgd_auto;
		}
		/*
		 * All deps were processed, store pkg in transaction.
		 */
		if (!xbps_transaction_pkg_type_set(repopkgd, ttype)) {
			rv = EINVAL;
			xbps_dbg_printf("xbps_transaction_pkg_type_set failed for `%s': %s\n", reqpkg, strerror(rv));
			break;
		}
		if (!xbps_transaction_store(xhp, pkgs, repopkgd, autoinst)) {
			rv = EINVAL;
			xbps_dbg_printf("xbps_transaction_store failed for `%s': %s\n", reqpkg, strerror(rv));
			break;
		}
	}
	xbps_object_iterator_release(iter);
out:
	(*depth)--;

	return rv;
}

int HIDDEN
xbps_transaction_pkg_deps(struct xbps_handle *xhp,
			  xbps_array_t pkgs,
			  xbps_dictionary_t pkg_repod)
{
	const char *pkgver;
	unsigned short depth = 0;

	assert(xhp);
	assert(pkgs);
	assert(pkg_repod);

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	xbps_dbg_printf("Finding required dependencies for '%s':\n", pkgver);
	/*
	 * This will find direct and indirect deps, if any of them is not
	 * there it will be added into the missing_deps array.
	 */
	return repo_deps(xhp, pkgs, pkg_repod, &depth);
}
