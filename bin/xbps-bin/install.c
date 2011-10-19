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
#include "compat.h"
#include "defs.h"
#include "../xbps-repo/defs.h"

struct transaction {
	prop_dictionary_t d;
	prop_object_iterator_t iter;
	uint32_t inst_pkgcnt;
	uint32_t up_pkgcnt;
	uint32_t cf_pkgcnt;
	uint32_t rm_pkgcnt;
};

static void
show_missing_deps(prop_array_t a)
{
	prop_object_t obj;
	size_t i;

	fprintf(stderr,
	    "xbps-bin: unable to locate some required packages:\n");

	for (i = 0; i < prop_array_count(a); i++) {
		obj = prop_array_get(a, i);
		fprintf(stderr, "  * Missing binary package for: %s\n",
		    prop_string_cstring_nocopy(obj));
	}
}

static int
show_binpkgs_url(prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *repoloc, *trans;
	char *binfile;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "repository", &repoloc))
			continue;

		/* ignore pkgs from local repositories */
		if (!xbps_check_is_repository_uri_remote(repoloc))
			continue;

		binfile = xbps_path_from_repository_uri(obj, repoloc);
		if (binfile == NULL)
			return errno;
		/*
		 * If downloaded package is in cachedir, ignore it.
		 */
		if (access(binfile, R_OK) == 0) {
			free(binfile);
			continue;
		}
		printf("%s\n", binfile);
		free(binfile);
	}
	prop_object_iterator_reset(iter);
	return 0;
}

static void
show_package_list(prop_object_iterator_t iter, const char *match)
{
	prop_object_t obj;
	const char *pkgver, *tract;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if (strcmp(match, tract))
			continue;
		print_package_line(pkgver, false);
	}
	prop_object_iterator_reset(iter);
	print_package_line(NULL, true);
}

static int
show_transaction_sizes(struct transaction *trans)
{
	uint64_t dlsize = 0, instsize = 0;
	char size[8];

	/*
	 * Show the list of packages that will be installed.
	 */
	if (prop_dictionary_get_uint32(trans->d, "total-install-pkgs",
	    &trans->inst_pkgcnt)) {
		printf("%u package%s will be installed:\n",
		    trans->inst_pkgcnt, trans->inst_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "install");
		printf("\n");
	}
	if (prop_dictionary_get_uint32(trans->d, "total-update-pkgs",
	    &trans->up_pkgcnt)) {
		printf("%u package%s will be updated:\n",
		    trans->up_pkgcnt, trans->up_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "update");
		printf("\n");
	}
	if (prop_dictionary_get_uint32(trans->d, "total-configure-pkgs",
	    &trans->cf_pkgcnt)) {
		printf("%u package%s will be configured:\n",
		    trans->cf_pkgcnt, trans->cf_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "configure");
		printf("\n");
	}
	if (prop_dictionary_get_uint32(trans->d, "total-remove-pkgs",
	    &trans->rm_pkgcnt)) {
		printf("%u package%s will be removed:\n",
		    trans->rm_pkgcnt, trans->rm_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "remove");
		printf("\n");
	}

	/*
	 * Show total download/installed size for all required packages.
	 */
	prop_dictionary_get_uint64(trans->d, "total-download-size", &dlsize);
	prop_dictionary_get_uint64(trans->d, "total-installed-size",
	    &instsize);
	if (xbps_humanize_number(size, (int64_t)dlsize) == -1) {
		xbps_error_printf("xbps-bin: error: humanize_number returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("\nTotal download size:\t%6s\n", size);
	if (xbps_humanize_number(size, (int64_t)instsize) == -1) {
		xbps_error_printf("xbps-bin: error: humanize_number2 returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("Total installed size:\t%6s\n\n", size);

	return 0;
}

int
autoupdate_pkgs(bool yes, bool show_download_pkglist_url)
{
	int rv = 0;

	/*
	 * Update all currently installed packages, aka
	 * "xbps-bin autoupdate".
	 */
	printf("Finding new packages...\n\n");
	if ((rv = xbps_repository_update_packages()) != 0) {
		if (rv == ENOENT) {
			printf("No packages currently registered.\n");
			return 0;
		} else if (rv == EEXIST) {
			printf("All packages are up-to-date.\n");
			return 0;
		} else if (rv == ENOTSUP) {
			xbps_error_printf("xbps-bin: no repositories currently "
			    "registered!\n");
			return -1;
		} else {
			xbps_error_printf("xbps-bin: unexpected error %s\n",
			    strerror(rv));
			return -1;
		}
	}

	return exec_transaction(yes, show_download_pkglist_url);
}

int
install_new_pkg(const char *pkg)
{
	prop_dictionary_t pkgd;
	char *pkgname = NULL, *pkgpatt = NULL;
	int rv = 0;
	bool pkgmatch = false;
	pkg_state_t state;

	if (xbps_pkgpattern_version(pkg)) {
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
	pkgname = xbps_pkgpattern_name(pkgpatt);
	if (pkgname == NULL)
		return -1;
	/*
	 * Find a package in a repository and prepare for installation.
	 */
	if ((pkgd = xbps_find_pkg_dict_installed(pkgname, false))) {
		if ((rv = xbps_pkg_state_dictionary(pkgd, &state)) != 0) {
			prop_object_release(pkgd);
			goto out;
		}
		prop_object_release(pkgd);
		if (state == XBPS_PKG_STATE_INSTALLED) {
			printf("Package '%s' is already installed.\n", pkgname);
			goto out;
		}
		printf("Package `%s' needs to be configured.\n", pkgname);
	}
	if ((rv = xbps_repository_install_pkg(pkgpatt)) != 0) {
		if (rv == ENOENT) {
			xbps_error_printf("xbps-bin: unable to locate '%s' in "
			    "repository pool.\n", pkg);
		} else if (rv == ENOTSUP) {
			xbps_error_printf("xbps-bin: no repositories  "
			    "currently registered!\n");
		} else {
			xbps_error_printf("xbps-bin: unexpected error: %s\n",
			    strerror(rv));
			rv = -1;
		}
	}
out:
	if (pkgmatch)
		free(pkgpatt);
	free(pkgname);

	return rv;
}

int
update_pkg(const char *pkgname)
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
	else if (rv == ENOTSUP)
		xbps_error_printf("xbps-bin: no repositories currently "
		    "registered!\n");
	else if (rv != 0) {
		xbps_error_printf("xbps-bin: unexpected error %s\n",
		    strerror(rv));
		return -1;
	}
	return 0;
}

int
exec_transaction(bool yes, bool show_download_urls)
{
	struct transaction *trans;
	prop_array_t array;
	int rv = 0;

	trans = calloc(1, sizeof(*trans));
	if (trans == NULL)
		return ENOMEM;

	if ((trans->d = xbps_transaction_prepare()) == NULL) {
		if (errno == ENODEV) {
			/* missing packages */
			array = xbps_transaction_missingdeps_get();
			show_missing_deps(array);
			rv = errno;
			goto out;
		}
		xbps_dbg_printf("Empty transaction dictionary: %s\n",
		    strerror(errno));
		return errno;
	}
	xbps_dbg_printf("Dictionary before transaction happens:\n");
	xbps_dbg_printf_append("%s", prop_dictionary_externalize(trans->d));

	trans->iter = xbps_array_iter_from_dict(trans->d, "packages");
	if (trans->iter == NULL) {
		rv = errno;
		xbps_error_printf("xbps-bin: error allocating array mem! (%s)\n",
		    strerror(errno));
		goto out;
	}
	/*
	 * Only show URLs to download binary packages.
	 */
	if (show_download_urls) {
		rv = show_binpkgs_url(trans->iter);
		goto out;
	}
	/*
	 * Show download/installed size for the transaction.
	 */
	if ((rv = show_transaction_sizes(trans)) != 0)
		goto out;
	/*
	 * Ask interactively (if -y not set).
	 */
	if (!yes && !noyes("Do you want to continue?")) {
		printf("Aborting!\n");
		goto out;
	}
	/*
	 * It's time to run the transaction!
	 */
	rv = xbps_transaction_commit(trans->d);
	if (rv == 0) {
		printf("\nxbps-bin: %u installed, %u updated, "
		    "%u configured, %u removed.\n", trans->inst_pkgcnt,
		    trans->up_pkgcnt, trans->cf_pkgcnt + trans->inst_pkgcnt,
		    trans->rm_pkgcnt);
	}
out:
	if (trans->iter)
		prop_object_iterator_release(trans->iter);
	if (trans->d)
		prop_object_release(trans->d);
	if (trans)
		free(trans);

	return rv;
}
