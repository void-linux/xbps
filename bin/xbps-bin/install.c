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
#include <ctype.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#include <xbps_api.h>
#include "strlcpy.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

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

static int
check_binpkg_hash(const char *path, const char *filename,
		  const char *sha256)
{
	int rv;

	printf("Checking %s integrity... ", filename);
	rv = xbps_check_file_hash(path, sha256);
	if (rv != 0 && rv != ERANGE) {
		fprintf(stderr, "\nxbps-bin: unexpected error: %s\n",
		    strerror(rv));
		return rv;
	} else if (rv == ERANGE) {
		printf("hash mismatch!\n");
		fprintf(stderr, "Package '%s' has wrong checksum, removing "
		    "and refetching it again...\n", filename);
		(void)remove(path);
		return rv;
	}
	printf("OK.\n");

	return 0;
}

static int
download_package_list(prop_object_iterator_t iter)
{
	prop_object_t obj;
	struct xbps_fetch_progress_data xfpd;
	const char *pkgver, *repoloc, *filename, *cachedir, *sha256;
	char *binfile;
	int rv = 0;
	bool cksum;

	cachedir = xbps_get_cachedir();
	if (cachedir == NULL)
		return EINVAL;

again:
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		cksum = false;
		prop_dictionary_get_bool(obj, "checksum_ok", &cksum);
		if (cksum == true)
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filename);
		prop_dictionary_get_cstring_nocopy(obj,
		    "filename-sha256", &sha256);

		binfile = xbps_get_binpkg_repo_uri(obj, repoloc);
		if (binfile == NULL)
			return errno;
		/*
		 * If downloaded package is in cachedir, check its hash
		 * and refetch the binpkg again if didn't match.
		 */
		if (access(binfile, R_OK) == 0) {
			rv = check_binpkg_hash(binfile, filename, sha256);
			free(binfile);
			if (rv != 0 && rv != ERANGE) {
				return rv;
			} else if (rv == ERANGE) {
				break;
			}
			prop_dictionary_set_bool(obj, "checksum_ok", true);
			continue;
		}
		if (xbps_mkpath(__UNCONST(cachedir), 0755) == -1) {
			free(binfile);
			return errno;
		}
		printf("Downloading %s binary package ...\n", pkgver);
		rv = xbps_fetch_file(binfile, cachedir, false, NULL,
		    fetch_file_progress_cb, &xfpd);
		if (rv == -1) {
			fprintf(stderr, "xbps-bin: couldn't download `%s'\n",
			    filename);
			fprintf(stderr, "xbps-bin: %s returned: `%s'\n",
			    repoloc, xbps_fetch_error_string());
			free(binfile);
			return -1;
		}
		free(binfile);
		binfile = xbps_get_binpkg_repo_uri(obj, repoloc);
		if (binfile == NULL)
			return errno;

		rv = check_binpkg_hash(binfile, filename, sha256);
		free(binfile);
		if (rv != 0 && rv != ERANGE) {
			return rv;
		} else if (rv == ERANGE) {
			break;
		}
		prop_dictionary_set_bool(obj, "checksum_ok", true);
	}
	prop_object_iterator_reset(iter);
	if (rv == ERANGE)
		goto again;

	return 0;
}

static void
show_package_list(prop_object_iterator_t iter, const char *match)
{
	prop_object_t obj;
	const char *pkgver, *tract;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "trans-action", &tract);
		if (strcmp(match, tract))
			continue;
		print_package_line(pkgver);
	}
	prop_object_iterator_reset(iter);
}

static int
show_transaction_sizes(struct transaction *trans)
{
	prop_object_t obj;
	uint64_t dlsize = 0, instsize = 0;
	const char *tract;
	char size[8];
	bool trans_inst, trans_up, trans_conf;

	trans_inst = trans_up = trans_conf = false;

	while ((obj = prop_object_iterator_next(trans->iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "trans-action", &tract);
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
	prop_dictionary_get_uint64(trans->dict, "total-download-size", &dlsize);
	prop_dictionary_get_uint64(trans->dict, "total-installed-size",
	    &instsize);
	if (xbps_humanize_number(size, (int64_t)dlsize) == -1) {
		fprintf(stderr, "xbps-bin: error: humanize_number returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("Total download size: %s\n", size);
	if (xbps_humanize_number(size, (int64_t)instsize) == -1) {
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
		} else if (rv == ENXIO) {
			printf("All packages are up-to-date.\n");
			return 0;
		} else {
			fprintf(stderr, "xbps-bin: unexpected error %s\n",
			    strerror(rv));
			return -1;
		}
	}

	return xbps_exec_transaction(yes);
}

int
xbps_install_new_pkg(const char *pkg)
{
	prop_dictionary_t pkgd;
	char *pkgname = NULL, *pkgpatt = NULL;
	int rv = 0;
	bool pkgmatch = false;

	if (xbps_get_pkgpattern_version(pkg)) {
		pkgpatt = __UNCONST(pkg);
	} else {
		/* 
		 * If only pkgname has been specified, always append
		 * '>=0' at the end, will be easier to parse.
		 */
		pkgmatch = true;
		pkgpatt = xbps_xasprintf("%s%s", pkg, ">=0");
		if (pkgpatt == NULL)
			return -1;
	}
	pkgname = xbps_get_pkgpattern_name(pkgpatt);
	if (pkgname == NULL)
		return -1;

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
	if ((rv = xbps_repository_install_pkg(pkgpatt)) != 0) {
		if (rv == EAGAIN) {
			fprintf(stderr, "xbps-bin: unable to locate '%s' in "
			    "repository pool.\n", pkg);
			rv = -1;
		} else {
			fprintf(stderr, "xbps-bin: unexpected error: %s\n",
			    strerror(rv));
			rv = -1;
		}
	}

	if (pkgmatch)
		free(pkgpatt);
	free(pkgname);

	return rv;
}

int
xbps_update_pkg(const char *pkgname)
{
	int rv = 0;

	rv = xbps_repository_update_pkg(pkgname);
	if (rv == EEXIST)
		printf("Package '%s' is up to date.\n", pkgname);
	else if (rv == ENOENT)
		fprintf(stderr, "Package '%s' not found in "
		    "repository pool.\n", pkgname);
	else if (rv == ENODEV)
		printf("Package '%s' not installed.\n", pkgname);
	else if (rv != 0) {
		fprintf(stderr, "xbps-bin: unexpected error %s\n",
		    strerror(rv));
		return -1;
	}
	return 0;
}

static int
replace_packages(prop_dictionary_t trans_dict, prop_dictionary_t pkgd,
		 prop_object_iterator_t replaces_iter, const char *pkgver)
{
	prop_dictionary_t instd = NULL, transd = NULL;
	prop_object_t obj;
	const char *pattern, *reppkgn, *reppkgver, *version;
	int rv = 0;

	/*
	 * This package replaces other package(s), so we remove
	 * them before upgrading or installing new one. If the package
	 * to be replaced is in the transaction and to be updated,
	 * the new package will overwrite its files.
	 */
	while ((obj = prop_object_iterator_next(replaces_iter))) {
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
		/*
		 * If the package to be replaced is in the transaction due to
		 * an update, do not remove it; just overwrite its files.
		 */
		transd = xbps_find_pkg_in_dict_by_name(trans_dict,
		    "packages", reppkgn);
		if (transd) {
			/*
			 * Set the bool property 'replace-files-in-pkg-update'.
			 */
			prop_dictionary_set_bool(pkgd,
			    "replace-files-in-pkg-update", true);
			printf("Replacing some files from '%s (will be "
			    "updated)' with '%s' (matched by '%s')...\n",
			    reppkgver, pkgver, pattern);
			prop_object_release(instd);
			continue;
		}

		printf("Replacing package '%s' with '%s' "
		    "(matched by '%s')...\n", reppkgver, pkgver, pattern);
		prop_object_release(instd);

		version = xbps_get_pkg_version(pkgver);
		if ((rv = xbps_remove_pkg(reppkgn, version, false)) != 0) {
			fprintf(stderr, "xbps-bin: couldn't remove %s (%s)\n",
			    reppkgn, strerror(rv));
			return -1;
		}
	}
	prop_object_iterator_release(replaces_iter);

	return 0;
}

static void
unpack_progress_cb_verbose(void *data)
{
	struct xbps_unpack_progress_data *xpd = data;

	if (xpd->entry == NULL || xpd->entry_is_metadata)
		return;
	else if (xpd->entry_size <= 0)
		return;

	fprintf(stderr, "Extracted %sfile `%s' (%" PRIi64 " bytes)\n",
	    xpd->entry_is_conf ? "configuration " : "", xpd->entry,
	    xpd->entry_size);
}

static void
unpack_progress_cb_percentage(void *data)
{
	struct xbps_unpack_progress_data *xpd = data;
	double percent = 0;

	if (xpd->entry_is_metadata)
		return;

	percent =
	    (double)((xpd->entry_extract_count * 100.0) / xpd->entry_total_count);
	if (percent > 100.0 ||
	    xpd->entry_extract_count >= xpd->entry_total_count)
		percent = 100.0;

	printf("\033[s(%3.2f%%)\033[u", percent);
}

static int
exec_transaction(struct transaction *trans)
{
	prop_dictionary_t instpkgd;
	prop_object_t obj;
	prop_object_iterator_t replaces_iter;
	struct xbps_unpack_progress_data xpd;
	const char *pkgname, *version, *pkgver, *instver, *filen, *tract;
	int flags = xbps_get_flags(), rv = 0;
	bool update, preserve, autoinst;
	pkg_state_t state = 0;

	assert(trans != NULL);
	assert(trans->dict != NULL);
	assert(trans->iter != NULL);

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
		autoinst = preserve = false;

		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filen);
		prop_dictionary_get_cstring_nocopy(obj, "trans-action", &tract);

		assert(pkgname != NULL);
		assert(version != NULL);
		assert(pkgver != NULL);
		assert(filen != NULL);
		assert(tract != NULL);

		prop_dictionary_get_bool(obj, "automatic-install", &autoinst);
		prop_dictionary_get_bool(obj, "preserve",  &preserve);
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
			rv = replace_packages(trans->dict, obj, replaces_iter, pkgver);
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

			prop_dictionary_get_cstring_nocopy(instpkgd,
			    "version", &instver);
			prop_object_release(instpkgd);

			if (preserve)
				printf("Conserving %s-%s files, installing new "
				    "version ...\n", pkgname, instver);
			else
				printf("Replacing %s files (%s -> %s) ...\n",
				    pkgname, instver, version);

			if ((rv = xbps_remove_pkg(pkgname, version, true)) != 0) {
				fprintf(stderr, "xbps-bin: error "
				    "replacing %s-%s (%s)\n", pkgname,
				    instver, strerror(rv));
				return rv;
			}
		}
		/*
		 * Unpack binary package.
		 */
		printf("Unpacking `%s' (from ../%s) ... ", pkgver, filen);

		if (flags & XBPS_FLAG_VERBOSE) {
			rv = xbps_unpack_binary_pkg(obj,
			    unpack_progress_cb_verbose, &xpd);
			printf("\n");
		} else {
			rv = xbps_unpack_binary_pkg(obj,
			    unpack_progress_cb_percentage, &xpd);
		}
		if (rv != 0) {
			fprintf(stderr, "xbps-bin: error unpacking %s "
			    "(%s)\n", pkgver, strerror(rv));
			return rv;
		}
		if ((flags & XBPS_FLAG_VERBOSE) == 0)
			printf("\n");

		/*
		 * Register binary package.
		 */
		if ((rv = xbps_register_pkg(obj, autoinst)) != 0) {
			fprintf(stderr, "xbps-bin: error registering %s "
			    "(%s)\n", pkgver, strerror(rv));
			return rv;
		}
	}
	prop_object_iterator_reset(trans->iter);
	/*
	 * Configure all unpacked packages.
	 */
	printf("\n[3/3] Configuring\n");
	while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "trans-action", &tract);
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
		return rv;

	trans->dict = xbps_transaction_prepare();
	if (trans->dict == NULL) {
		if (errno == ENODEV) {
			/* missing packages */
			array = xbps_transaction_missingdeps_get();
			show_missing_deps(trans->dict);
			goto out;
		}
		xbps_dbg_printf("Empty transaction dictionary: %s\n",
		    strerror(errno));
		goto out;
	}
	xbps_dbg_printf("Dictionary before transaction happens:\n");
	xbps_dbg_printf_append("%s", prop_dictionary_externalize(trans->dict));

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
