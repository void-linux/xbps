/*-
 * Copyright (c) 2009 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"

struct transaction {
	prop_dictionary_t dict;
	prop_object_iterator_t iter;
	const char *originpkgname;
	bool force;
};

static int	exec_transaction(struct transaction *);
static void	show_missing_deps(prop_dictionary_t, const char *);
static int	show_missing_dep_cb(prop_object_t, void *, bool *);
static void	show_package_list(prop_object_iterator_t, const char *);

static void
show_missing_deps(prop_dictionary_t d, const char *pkgname)
{
	printf("Unable to locate some required packages for %s:\n",
	    pkgname);
	(void)xbps_callback_array_iter_in_dict(d, "missing_deps",
	    show_missing_dep_cb, NULL);
}

static int
show_missing_dep_cb(prop_object_t obj, void *arg, bool *loop_done)
{
	const char *pkgname, *version;

        (void)arg;
        (void)loop_done;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(obj, "version", &version);
	if (pkgname && version) {
		printf("  * Missing binary package for: %s%s\n",
		    pkgname, version);
		return 0;
	}

	return EINVAL;
}

static int
check_pkg_hashes(prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgname, *repoloc, *filename;
	int rv = 0;
	pkg_state_t state = 0;

	printf("Checking binary package file(s) integrity...\n");
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname))
			return errno;

		state = 0;
		if (xbps_get_pkg_state_dictionary(obj, &state) != 0)
			return EINVAL;

		if (state == XBPS_PKG_STATE_UNPACKED)
			continue;

		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "repository", &repoloc))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "filename", &filename))
			return errno;
		rv = xbps_check_pkg_file_hash(obj, repoloc);
		if (rv != 0 && rv != ERANGE) {
			printf("Unexpected error while checking hash for "
			    "%s (%s)\n", filename, strerror(rv));
			return -1;
		} else if (rv != 0 && rv == ERANGE) {
			printf("Hash mismatch for %s, exiting.\n",
			    filename);
			return -1;
		}
	}
	prop_object_iterator_reset(iter);

	return 0;
}

static int
download_package_list(prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *repoloc, *filename, *arch;
	char *savedir, *binfile, *lbinfile, *repoloc_trans;
	int rv = 0;

	printf("Downloading binary package file(s)...\n");
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "repository", &repoloc))
			return errno;
		/*
		 * Skip packages in local repositories.
		 */
		if (!xbps_check_is_repo_string_remote(repoloc))
			continue;

		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &pkgver))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "filename", &filename))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "architecture", &arch))
			return errno;

		repoloc_trans = xbps_get_remote_repo_string(repoloc);
		if (repoloc_trans == NULL)
			return errno;

		savedir = xbps_xasprintf("%s/%s/repo/%s/%s",
		    xbps_get_rootdir(), XBPS_META_PATH, repoloc_trans, arch);
		if (savedir == NULL) {
			free(repoloc_trans);
			return errno;
		}
		lbinfile = xbps_xasprintf("%s/%s", savedir, filename);
		if (lbinfile == NULL) {
			free(repoloc_trans);
			free(savedir);
			return errno;
		}
		if (access(lbinfile, R_OK) == 0) {
			free(savedir);
			free(lbinfile);
			goto change_repodir;
		}
		free(lbinfile);

		binfile = xbps_xasprintf("%s/%s/%s", repoloc, arch, filename);
		if (binfile == NULL) {
			free(repoloc_trans);
			free(savedir);
			return errno;
		}
		printf("Downloading %s binary package ...\n", pkgver);
		rv = xbps_fetch_file(binfile, savedir, false, NULL);
		free(savedir);
		free(binfile);
		if (rv != 0) {
			printf("Couldn't download %s from %s (%s)\n",
			    filename, repoloc, strerror(rv));
			free(repoloc_trans);
			return errno;
		}

change_repodir:
		/*
		 * If it was downloaded successfully, override repository
		 * path in transaction dictionary.
		 */
		savedir = xbps_xasprintf("%s/%s/repo/%s",
		    xbps_get_rootdir(), XBPS_META_PATH, repoloc_trans);
		free(repoloc_trans);
		if (savedir == NULL)
		       return errno;

		if (!prop_dictionary_set_cstring(obj, "repository", savedir)) {
			free(savedir);
			return errno;
		}
		free(savedir);
	}
	prop_object_iterator_reset(iter);

	return 0;
}

static void
show_package_list(prop_object_iterator_t iter, const char *match)
{
	prop_object_t obj;
	size_t cols = 0;
	const char *pkgver, *tract;
	bool first = false;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &pkgver))
			return;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "trans-action", &tract))
			return;
		if (strcmp(match, tract))
			continue;

		cols += strlen(pkgver) + 4;
		if (cols <= 80) {
			if (first == false) {
				printf("  ");
				first = true;
			}
		} else {
			printf("\n  ");
			cols = strlen(pkgver) + 4;
		}
		printf("%s ", pkgver);
	}
	prop_object_iterator_reset(iter);
}

static int
show_transaction_sizes(prop_object_iterator_t iter)
{
	prop_object_t obj;
	uint64_t tsize = 0, dlsize = 0, instsize = 0;
	const char *tract;
	char size[64];
	bool trans_inst = false, trans_up = false;

	/*
	 * Iterate over the list of packages that are going to be
	 * installed and check the file hash.
	 */
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (!prop_dictionary_get_uint64(obj, "filename-size", &tsize))
			return errno;

		dlsize += tsize;
		tsize = 0;
		if (!prop_dictionary_get_uint64(obj, "installed_size", &tsize))
			return errno;

		instsize += tsize;
		tsize = 0;
	}
	prop_object_iterator_reset(iter);

	while ((obj = prop_object_iterator_next(iter))) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "trans-action", &tract))
			return errno;

		if (strcmp(tract, "install") == 0)
			trans_inst = true;
		else if (strcmp(tract, "update") == 0)
			trans_up = true;
	}
	prop_object_iterator_reset(iter);

	/*
	 * Show the list of packages that will be installed.
	 */
	if (trans_inst) {
		printf("The following packages will be installed:\n\n");
		show_package_list(iter, "install");
		printf("\n\n");
	}
	if (trans_up) {
		printf("The following packages will be updated:\n\n");
		show_package_list(iter, "update");
		printf("\n\n");
	}

	/*
	 * Show total download/installed size for all required packages.
	 */
	if (xbps_humanize_number(size, 5, (int64_t)dlsize,
	    "", HN_AUTOSCALE, HN_B|HN_DECIMAL|HN_NOSPACE) == -1) {
		printf("error: humanize_number returns %s\n",
		    strerror(errno));
		return -1;
	}
	printf("Total download size: %s\n", size);
	if (xbps_humanize_number(size, 5, (int64_t)instsize,
	    "", HN_AUTOSCALE, HN_B|HN_DECIMAL|HN_NOSPACE) == -1) {
		printf("error: humanize_number2 returns %s\n",
		    strerror(errno));
		return -1;
	}
	printf("Total installed size: %s\n\n", size);

	return 0;
}

int
xbps_exec_transaction(const char *pkgname, bool force, bool update)
{
	struct transaction *trans;
	prop_dictionary_t pkgd;
	prop_array_t array;
	int rv = 0;

	assert(pkgname != NULL);

	if (update && (strcasecmp(pkgname, "all") == 0)) {
		/*
		 * Update all currently installed packages, aka
		 * "xbps-bin autoupdate".
		 */
		printf("Finding new packages...\n");
		if ((rv = xbps_find_new_packages()) != 0) {
			if (rv == ENOENT) {
				printf("No packages currently registered.\n");
				return 0;
			} else if (rv == ENOPKG) {
				printf("All packages are up-to-date.\n");
				return 0;
			}
			goto out;
		}
	} else {
		pkgd = xbps_find_pkg_installed_from_plist(pkgname);
		if (update) {
			/*
			 * Update a single package, aka
			 * "xbps-bin update pkgname"
			 */
			printf("Finding new '%s' package...\n", pkgname);
			if (pkgd) {
				rv = xbps_find_new_pkg(pkgname, pkgd);
				if (rv == EEXIST) {
					printf("Package '%s' is up to date.\n",
					    pkgname);
					prop_object_release(pkgd);
					return rv;
				} else if (rv == ENOENT) {
					printf("Package '%s' not found in "
					    "repository pool.\n", pkgname);
					prop_object_release(pkgd);
					return rv;
				} else if (rv != 0) {
					prop_object_release(pkgd);
					return rv;
				}
				prop_object_release(pkgd);
			} else {
				printf("Package '%s' not installed.\n",
				    pkgname);
				return rv;
			}
		} else {
			/*
			 * Install a single package, aka
			 * "xbps-bin install pkgname"
			 */
			if (pkgd) {
				printf("Package '%s' is already installed.\n",
				    pkgname);
				prop_object_release(pkgd);
				return rv;
			}
			rv = xbps_prepare_pkg(pkgname);
			if (rv != 0 && rv == EAGAIN) {
				printf("Unable to locate '%s' in "
				    "repository pool.\n", pkgname);
				return rv;
			} else if (rv != 0 && rv != ENOENT) {
				printf("Unexpected error: %s", strerror(rv));
				return rv;
			}
		}
	}

	trans = calloc(1, sizeof(struct transaction));
	if (trans == NULL)
		goto out;

	trans->dict = xbps_get_pkg_props();
	if (trans->dict == NULL) {
		printf("error: unexistent props dictionary!\n");
		goto out1;
	}

	/*
	 * Bail out if there are unresolved deps.
	 */
	array = prop_dictionary_get(trans->dict, "missing_deps");
	if (prop_array_count(array) > 0) {
		show_missing_deps(trans->dict, pkgname);
		goto out2;
	}

	if (!prop_dictionary_get_cstring_nocopy(trans->dict,
	     "origin", &trans->originpkgname)) {
		rv = errno;
		goto out2;
	}

	if (update) {
		/*
		 * Sort the package transaction dictionary.
		 */
		if ((rv = xbps_sort_pkg_deps(trans->dict)) != 0) {
			printf("Error while sorting packages: %s\n",
		    	    strerror(rv));
			goto out2;
		}
	}

	/*
	 * It's time to run the transaction!
	 */
	trans->iter = xbps_get_array_iter_from_dict(trans->dict, "packages");
	if (trans->iter == NULL) {
		printf("error: allocating array mem! (%s)\n",
		    strerror(errno));
		goto out2;
	}

	trans->force = force;
	rv = exec_transaction(trans);

	prop_object_iterator_release(trans->iter);
out2:
	prop_object_release(trans->dict);
out1:
	free(trans);
out:
	return rv;
}

static int
replace_packages(prop_object_iterator_t iter, const char *pkgver)
{
	prop_dictionary_t instd;
	prop_object_t obj;
	const char *reppkgn;
	int rv = 0;

	/*
	 * This package replaces other package(s), so we remove
	 * them before upgrading or installing new one.
	 */
	while ((obj = prop_object_iterator_next(iter))) {
		reppkgn = prop_string_cstring_nocopy(obj);
		if (reppkgn == NULL)
			return errno;

		instd = xbps_find_pkg_installed_from_plist(reppkgn);
		if (instd == NULL)
			continue;

		printf("Replacing package '%s' with '%s' ...\n",
		    reppkgn, pkgver);
		if ((rv = xbps_remove_pkg(reppkgn, NULL, false)) != 0) {
			printf("Couldn't remove %s (%s)\n",
			    reppkgn, strerror(rv));
			return rv;
		}
		if ((rv = xbps_purge_pkg(reppkgn, false)) != 0) {
			printf("Couldn't purge %s (%s)\n",
			    reppkgn, strerror(rv));
			return rv;
		}
	}
	prop_object_iterator_release(iter);

	return 0;
}

static int
exec_transaction(struct transaction *trans)
{
	prop_dictionary_t instpkgd;
	prop_object_t obj;
	prop_object_iterator_t replaces_iter;
	const char *pkgname, *version, *pkgver, *instver, *filename, *tract;
	int rv = 0;
	bool essential, autoinst;
	pkg_state_t state = 0;

	assert(trans != NULL);
	assert(trans->dict != NULL);
	assert(trans->iter != NULL);

	essential = autoinst = false;
	/*
	 * Show download/installed size for the transaction.
	 */
	rv = show_transaction_sizes(trans->iter);
	if (rv != 0)
		return rv;

	/*
	 * Ask interactively (if -f not set).
	 */
	if (trans->force == false) {
		if (xbps_noyes("Do you want to continue?") == false) {
			printf("Aborting!\n");
			return 0;
		}
	}

	/*
	 * Download binary packages if they are in a remote repository.
	 */
	if ((rv = download_package_list(trans->iter)) != 0)
		return rv;

	/*
	 * Check the SHA256 hash for all required packages.
	 */
	if ((rv = check_pkg_hashes(trans->iter)) != 0)
		return rv;

	/*
	 * Iterate over the transaction dictionary.
	 */
	while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "version", &version))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &pkgver))
			return errno;
		prop_dictionary_get_bool(obj, "essential", &essential);
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "filename", &filename))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "trans-action", &tract))
			return errno;
		replaces_iter = xbps_get_array_iter_from_dict(obj, "replaces");

		/*
		 * Set automatic-install bool if we are updating all packages,
		 * and a new package is going to be installed, and
		 * if we updating a package required new updating dependent
		 * packages.
		 */
		if (trans->originpkgname &&
		    strcmp(trans->originpkgname, pkgname))
			autoinst = true;
		else if (!trans->originpkgname && strcmp(tract, "install") == 0)
			autoinst = true;

		/*
		 * If dependency is already unpacked skip this phase.
		 */
		state = 0;
		if (xbps_get_pkg_state_dictionary(obj, &state) != 0)
			return EINVAL;

		if (state == XBPS_PKG_STATE_UNPACKED)
			continue;

		/*
		 * Replace package(s) if necessary.
		 */
		if (replaces_iter != NULL) {
			rv = replace_packages(replaces_iter, pkgver);
			if (rv != 0) {
				printf("Couldn't replace some packages! "
				    "(%s)\n", strerror(rv));
				return rv;
			}
			replaces_iter = NULL;
		}

		if (strcmp(tract, "update") == 0) {
			instpkgd = xbps_find_pkg_installed_from_plist(pkgname);
			if (instpkgd == NULL) {
				printf("error: unable to find %s installed "
				    "dict!\n", pkgname);
				return EINVAL;
			}

			if (!prop_dictionary_get_cstring_nocopy(instpkgd,
			    "version", &instver)) {
				prop_object_release(instpkgd);
				return errno;
			}
			autoinst = false;
			if (!prop_dictionary_get_bool(instpkgd,
			    "automatic-install", &autoinst)) {
				prop_object_release(instpkgd);
				return errno;
			}
			prop_object_release(instpkgd);

			/*
			 * If package is marked as 'essential' remove old
			 * requiredby entries and overwrite pkg files; otherwise
			 * remove old package and install new one.
			 */
			if (essential) {
				rv = xbps_requiredby_pkg_remove(pkgname);
				if (rv != 0) {
					printf("error: couldn't remove reqby"
					    " entries for %s-%s (%s)\n",
					    pkgname, instver, strerror(rv));
					return rv;
				}
			} else {
				printf("Removing %s-%s ...\n",
				    pkgname, instver);
				rv = xbps_remove_pkg(pkgname, version, true);
				if (rv != 0) {
					printf("error: removing %s-%s (%s)\n",
					    pkgname, instver, strerror(rv));
					return rv;
				}
			}
		}
		/*
		 * Unpack binary package.
		 */
		printf("Unpacking %s (from .../%s) ...\n", pkgver, filename);
		if ((rv = xbps_unpack_binary_pkg(obj, essential)) != 0) {
			printf("error: unpacking %s (%s)\n", pkgver,
			    strerror(rv));
			return rv;
		}
		/*
		 * Register binary package.
		 */
		if ((rv = xbps_register_pkg(obj, autoinst)) != 0) {
			printf("error: registering %s! (%s)\n",
			    pkgver, strerror(rv));
			return rv;
		}
		autoinst = false;
		/*
		 * Set package state to unpacked in the transaction
		 * dictionary.
		 */
		if ((rv = xbps_set_pkg_state_dictionary(obj,
		    XBPS_PKG_STATE_UNPACKED)) != 0)
			return rv;
	}
	prop_object_iterator_reset(trans->iter);
	/*
	 * Configure all unpacked packages.
	 */
	while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "version", &version))
			return errno;
		if ((rv = xbps_configure_pkg(pkgname, version, false)) != 0) {
			printf("Error configuring package %s (%s)\n",
			    pkgname, strerror(rv));
			return rv;
		}
	}

	return 0;
}
