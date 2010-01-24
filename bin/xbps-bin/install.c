/*-
 * Copyright (c) 2009-2010 Juan Romero Pardines.
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
#include <ctype.h>

#include <xbps_api.h>
#include "defs.h"

struct transaction {
	prop_dictionary_t dict;
	prop_object_iterator_t iter;
	bool yes;
	size_t inst_pkgcnt;
	size_t up_pkgcnt;
	size_t cf_pkgcnt;
};

static int
show_missing_dep_cb(prop_object_t obj, void *arg, bool *loop_done)
{
	const char *reqpkg;

        (void)arg;
        (void)loop_done;

	reqpkg = prop_string_cstring_nocopy(obj);
	if (reqpkg == NULL)
		return EINVAL;

	fprintf(stderr, "  * Missing binary package for: %s\n", reqpkg);
	return 0;
}

static void
show_missing_deps(prop_dictionary_t d)
{
	fprintf(stderr,
	    "xbps-bin: unable to locate some required packages:\n");
	(void)xbps_callback_array_iter_in_dict(d, "missing_deps",
	    show_missing_dep_cb, NULL);
}

static bool
check_binpkg_hash(const char *path, const char *filename,
		  const char *sha256)
{
	int rv = 0;

	printf("Checking %s integrity... ", filename);
	rv = xbps_check_file_hash(path, sha256);
	errno = rv;
	if (rv != 0 && rv != ERANGE) {
		fprintf(stderr, "xbps-bin: unexpected error: %s\n",
		    strerror(rv));
		return false;
	} else if (rv == ERANGE) {
		fprintf(stderr, "xbps-bin: hash mismatch!.\n");
		return false;
	}
	printf("OK.\n");

	return true;
}

static int
download_package_list(prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *repoloc, *filename, *cachedir, *sha256;
	char *binfile, *lbinfile;
	int rv = 0;
	bool found_binpkg;

	cachedir = xbps_get_cachedir();
	if (cachedir == NULL)
		return EINVAL;

	/* Set default limit of global and per-host cached connections */
	xbps_fetch_set_cache_connection(0, 0);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		found_binpkg = false;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "repository", &repoloc))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgver", &pkgver))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "filename", &filename))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "filename-sha256", &sha256))
			return errno;

		lbinfile = xbps_get_binpkg_local_path(obj, repoloc);
		if (lbinfile == NULL)
			return errno;

		/*
		 * If package is in a local repository, check its hash
		 * and pass to next one.
		 */
		if (!xbps_check_is_repo_string_remote(repoloc)) {
			if (!check_binpkg_hash(lbinfile, filename, sha256)) {
				free(lbinfile);
				return errno;
			}
			free(lbinfile);
			continue;
		}
		/*
		 * If downloaded package is in cachedir, check its hash
		 * and restart it again if doesn't match.
		 */
		if (access(lbinfile, R_OK) == 0) {
			if (check_binpkg_hash(lbinfile, filename, sha256)) {
				free(lbinfile);
				continue;
			}
			if (errno && errno != ERANGE) {
				free(lbinfile);
				return errno;
			} else if (errno == ERANGE) {
				(void)remove(lbinfile);
				printf("Refetching %s again...\n",
				    filename);
				errno = 0;
			}
		}
		if (xbps_mkpath(__UNCONST(cachedir), 0755) == -1) {
			free(lbinfile);
			return errno;
		}
		binfile = xbps_repository_get_path_from_pkg_dict(obj, repoloc);
		if (binfile == NULL) {
			free(lbinfile);
			return errno;
		}
		printf("Downloading %s binary package ...\n", pkgver);
		rv = xbps_fetch_file(binfile, cachedir, false, NULL);
		free(binfile);
		if (rv == -1) {
			fprintf(stderr, "xbps-bin: couldn't download `%s'\n",
			    filename);
			fprintf(stderr, "xbps-bin: %s returned: `%s'\n",
			    repoloc, xbps_fetch_error_string());
			free(lbinfile);
			return -1;
		}
		if (!check_binpkg_hash(lbinfile, filename, sha256)) {
			fprintf(stderr, "W: removing wrong %s file ...\n",
			    filename);
			(void)remove(lbinfile);
			free(lbinfile);
			return errno;
		}
		free(lbinfile);
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
show_transaction_sizes(struct transaction *trans)
{
	prop_object_t obj;
	uint64_t dlsize = 0, instsize = 0;
	const char *tract;
	char size[64];
	bool trans_inst, trans_up, trans_conf;

	trans_inst = trans_up = trans_conf = false;

	while ((obj = prop_object_iterator_next(trans->iter))) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "trans-action", &tract))
			return errno;

		if (strcmp(tract, "install") == 0) {
			trans->inst_pkgcnt++;
			trans_inst = true;
		} else if (strcmp(tract, "update") == 0) {
			trans->up_pkgcnt++;
			trans_up = true;
		} else if (strcmp(tract, "configure") == 0) {
			trans->cf_pkgcnt++;
			trans_conf = true;
		}
	}
	prop_object_iterator_reset(trans->iter);

	/*
	 * Show the list of packages that will be installed.
	 */
	if (trans_inst) {
		printf("The following packages will be installed:\n\n");
		show_package_list(trans->iter, "install");
		printf("\n\n");
	}
	if (trans_up) {
		printf("The following packages will be updated:\n\n");
		show_package_list(trans->iter, "update");
		printf("\n\n");
	}
	if (trans_conf) {
		printf("The following packages will be configured:\n\n");
		show_package_list(trans->iter, "configure");
		printf("\n\n");
	}

	/*
	 * Show total download/installed size for all required packages.
	 */
	if (!prop_dictionary_get_uint64(trans->dict,
	    "total-download-size", &dlsize))
		return errno;
	if (!prop_dictionary_get_uint64(trans->dict,
	    "total-installed-size", &instsize))
		return errno;
	if (xbps_humanize_number(size, 5, (int64_t)dlsize,
	    "", HN_AUTOSCALE, HN_B|HN_DECIMAL|HN_NOSPACE) == -1) {
		fprintf(stderr, "xbps-bin: error: humanize_number returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("Total download size: %s\n", size);
	if (xbps_humanize_number(size, 5, (int64_t)instsize,
	    "", HN_AUTOSCALE, HN_B|HN_DECIMAL|HN_NOSPACE) == -1) {
		fprintf(stderr, "xbps-bin: error: humanize_number2 returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("Total installed size: %s\n\n", size);

	return 0;
}

int
xbps_autoupdate_pkgs(bool yes)
{
	int rv = 0;

	/*
	 * Update all currently installed packages, aka
	 * "xbps-bin autoupdate".
	 */
	printf("Finding new packages...\n");
	if ((rv = xbps_repository_update_allpkgs()) != 0) {
		if (rv == ENOENT) {
			printf("No packages currently registered.\n");
			return 0;
		} else if (rv == ENOPKG) {
			printf("All packages are up-to-date.\n");
			return 0;
		}
		return rv;
	}

	return xbps_exec_transaction(yes);
}

static char *
pkgname_from_pkgmatch(const char *pkg)
{
	const char *version;

	/*
	 * Check if 'pkg' string is a pkgmatch valid pattern or it
	 * is just a pkgname.
	 *
	 * XXX REALLY FIX THIS CRAP! I haven't found much easier ways...
	 */
	if ((version = xbps_get_pkgpattern_version(pkg))) {
		while (*version) {
			if (!isdigit((unsigned char)*version)) {
				version++;
				continue;
			}
			if (xbps_cmpver("0", version) <= 0)
				return xbps_get_pkgpattern_name(pkg);
		}
	}
	if ((version = xbps_get_pkg_version(pkg))) {
		while (*version) {
			if (!isdigit((unsigned char)*version)) {
				version++;
				continue;
			}

			const char *tmp = version;
			size_t ndigits = 0, tmplen = strlen(tmp);
			bool dot = false, digit = false;

			while (*tmp) {
				if (isdigit((unsigned char)*tmp)) {
					digit = true;
					ndigits++;
				}
				if (*tmp == '.')
					dot = true;
				else if (*tmp == '_')
					ndigits++;

				tmp++;
			}
			if ((!dot && !digit) || (!dot && tmplen != ndigits))
				break;

			if (xbps_cmpver("0", version) <= 0)
				return xbps_get_pkg_name(pkg);
		}
	}

	return NULL;
}

int
xbps_install_new_pkg(const char *pkg)
{
	prop_dictionary_t pkgd;
	char *pkgname = NULL;
	int rv = 0;
	bool pkgmatch = false;

	if ((pkgname = pkgname_from_pkgmatch(pkg)))
		pkgmatch = true;
	else
		pkgname = __UNCONST(pkg);

	/*
	 * Find a package in a repository and prepare for installation.
	 */
	if ((pkgd = xbps_find_pkg_dict_installed(pkgname, false))) {
		printf("Package '%s' is already installed.\n", pkgname);
		prop_object_release(pkgd);
		if (pkgmatch)
			free(pkgname);
		return 0;
	}
	rv = xbps_repository_install_pkg(pkg, pkgmatch);
	if (rv == EAGAIN) {
		fprintf(stderr, "xbps-bin: unable to locate '%s' in "
		    "repository pool.\n", pkg);
		rv = 1;
	} else if (rv != 0 && rv != ENOENT) {
		fprintf(stderr, "xbps-bin: unexpected error: %s",
		    strerror(errno));
		rv = -1;
	}
	
	if (pkgmatch)
		free(pkgname);

	return rv;
}

int
xbps_update_pkg(const char *pkgname)
{
	prop_dictionary_t pkgd;
	int rv = 0;

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	printf("Finding new '%s' package...\n", pkgname);
	if (pkgd) {
		rv = xbps_repository_update_pkg(pkgname, pkgd);
		if (rv == EEXIST) {
			printf("Package '%s' is up to date.\n", pkgname);
			rv = 0;
		} else if (rv == ENOENT) {
			fprintf(stderr, "Package '%s' not found in "
			    "repository pool.\n", pkgname);
			rv = 0;
		}
		prop_object_release(pkgd);
	} else {
		printf("Package '%s' not installed.\n", pkgname);
		return 0;
	}

	return rv;
}

static int
replace_packages(prop_object_iterator_t iter, const char *pkgver)
{
	prop_dictionary_t instd;
	prop_object_t obj;
	const char *pattern, *reppkgn, *reppkgver, *version;
	int rv = 0;

	/*
	 * This package replaces other package(s), so we remove
	 * them before upgrading or installing new one.
	 */
	while ((obj = prop_object_iterator_next(iter))) {
		pattern = prop_string_cstring_nocopy(obj);
		if (pattern == NULL)
			return errno;

		/*
		 * If pattern matches an installed package, replace it.
		 */
		instd = xbps_find_pkg_dict_installed(pattern, true);
		if (instd == NULL)
			continue;

		prop_dictionary_get_cstring_nocopy(instd, "pkgname", &reppkgn);
		prop_dictionary_get_cstring_nocopy(instd, "pkgver", &reppkgver);
		printf("Replacing package '%s' with '%s' "
		    "(matched by '%s')...\n", reppkgver, pkgver, pattern);
		prop_object_release(instd);

		version = xbps_get_pkg_version(pkgver);
		if ((rv = xbps_remove_pkg(reppkgn, version, false)) != 0) {
			fprintf(stderr, "xbps-bin: couldn't remove %s (%s)\n",
			    reppkgn, strerror(rv));
			return rv;
		}
		if ((rv = xbps_purge_pkg(reppkgn, false)) != 0) {
			fprintf(stderr, "xbps-bin: couldn't purge %s (%s)\n",
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
	bool update, essential, autoinst;
	pkg_state_t state = 0;

	assert(trans != NULL);
	assert(trans->dict != NULL);
	assert(trans->iter != NULL);

	update = essential = autoinst = false;
	/*
	 * Show download/installed size for the transaction.
	 */
	if ((rv = show_transaction_sizes(trans)) != 0)
		return rv;

	/*
	 * Ask interactively (if -y not set).
	 */
	if (trans->yes == false) {
		if (xbps_noyes("Do you want to continue?") == false) {
			printf("Aborting!\n");
			return 0;
		}
	}

	/*
	 * Download binary packages (if they come from a remote repository)
	 * and check its SHA256 hash.
	 */
	printf("[1/3] Downloading/integrity check\n");
	if ((rv = download_package_list(trans->iter)) != 0)
		return rv;

	/*
	 * Iterate over the transaction dictionary.
	 */
	printf("\n[2/3] Unpacking\n");
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
		prop_dictionary_get_bool(obj, "automatic-install", &autoinst);
		prop_dictionary_get_bool(obj, "essential", &essential);
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "filename", &filename))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "trans-action", &tract))
			return errno;
		replaces_iter = xbps_get_array_iter_from_dict(obj, "replaces");

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
				fprintf(stderr, 
				    "xbps-bin: couldn't replace some "
				    "packages! (%s)\n", strerror(rv));
				return rv;
			}
			replaces_iter = NULL;
		}

		if (strcmp(tract, "update") == 0) {
			instpkgd = xbps_find_pkg_dict_installed(pkgname, false);
			if (instpkgd == NULL) {
				fprintf(stderr, "xbps-bin: error: unable to "
				    "find %s installed dict!\n", pkgname);
				return EINVAL;
			}

			if (!prop_dictionary_get_cstring_nocopy(instpkgd,
			    "version", &instver)) {
				prop_object_release(instpkgd);
				return errno;
			}
			prop_object_release(instpkgd);

			/*
			 * If package is marked as 'essential' remove old
			 * requiredby entries and overwrite pkg files; otherwise
			 * remove old package and install new one. This
			 * is all handled internally in xbps_remove_pkg()
			 * and xbps_unpack_binary_pkg().
			 */
			if (essential)
				printf("Replacing %s-%s with %s-%s ...\n",
				    pkgname, instver, pkgname, version);
			else
				printf("Removing %s-%s before installing new "
				    "version ...\n", pkgname, instver);

			rv = xbps_remove_pkg(pkgname, version, true);
			if (rv != 0) {
				fprintf(stderr, "xbps-bin: error "
				    "%s %s-%s (%s)\n", essential ?
				    "replacing" : "removing", pkgname,
				    instver, strerror(rv));
				return rv;
			}
		}
		/*
		 * Unpack binary package.
		 */
		printf("Unpacking %s (from .../%s) ...\n", pkgver, filename);
		if ((rv = xbps_unpack_binary_pkg(obj)) != 0) {
			fprintf(stderr, "xbps-bin: error unpacking %s "
			    "(%s)\n", pkgver, strerror(rv));
			return rv;
		}
		/*
		 * Register binary package.
		 */
		if ((rv = xbps_register_pkg(obj, autoinst)) != 0) {
			fprintf(stderr, "xbps-bin: error registering %s "
			    "(%s)\n", pkgver, strerror(rv));
			return rv;
		}
		autoinst = false;
	}
	prop_object_iterator_reset(trans->iter);
	/*
	 * Configure all unpacked packages.
	 */
	printf("\n[3/3] Configuring\n");
	while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "pkgname", &pkgname))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "version", &version))
			return errno;
		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "trans-action", &tract))
			return errno;
		update = false;
		if (strcmp(tract, "update") == 0)
			update = true;
		rv = xbps_configure_pkg(pkgname, version, false, update);
		if (rv != 0) {
			fprintf(stderr, "xbps-bin: error configuring "
			    "package %s (%s)\n", pkgname, strerror(rv));
			return rv;
		}
		trans->cf_pkgcnt++;
	}
	printf("\nxbps-bin: %zu installed, %zu updated, "
	    "%zu configured.\n", trans->inst_pkgcnt, trans->up_pkgcnt,
	    trans->cf_pkgcnt);

	return 0;
}

int
xbps_exec_transaction(bool yes)
{
	struct transaction *trans;
	prop_array_t array;
	int rv = 0;

	trans = calloc(1, sizeof(struct transaction));
	if (trans == NULL)
		goto out;

	trans->dict = xbps_repository_get_transaction_dict();
	if (trans->dict == NULL)
		goto out;

	/*
	 * Bail out if there are unresolved deps.
	 */
	array = prop_dictionary_get(trans->dict, "missing_deps");
	if (prop_array_count(array) > 0) {
		show_missing_deps(trans->dict);
		goto out;
	}

	DPRINTF(("%s", prop_dictionary_externalize(trans->dict)));

	/*
	 * It's time to run the transaction!
	 */
	trans->iter = xbps_get_array_iter_from_dict(trans->dict, "packages");
	if (trans->iter == NULL) {
		fprintf(stderr, "xbps-bin: error allocating array mem! (%s)\n",
		    strerror(errno));
		goto out;
	}

	trans->yes = yes;
	rv = exec_transaction(trans);

out:
	if (trans->iter)
		prop_object_iterator_release(trans->iter);
	if (trans->dict)
		prop_object_release(trans->dict);
	if (trans)
		free(trans);
	return rv;
}
