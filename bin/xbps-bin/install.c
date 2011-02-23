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
	bool only_show;
	size_t inst_pkgcnt;
	size_t up_pkgcnt;
	size_t cf_pkgcnt;
	size_t rm_pkgcnt;
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
check_binpkg_hash(const char *path,
		  const char *filename,
		  const char *sha256)
{
	int rv;

	printf("Checking %s integrity... ", filename);
	rv = xbps_check_file_hash(path, sha256);
	if (rv != 0 && rv != ERANGE) {
		xbps_error_printf("\nxbps-bin: unexpected error: %s\n",
		    strerror(rv));
		return rv;
	} else if (rv == ERANGE) {
		printf("hash mismatch!\n");
		xbps_warn_printf("Package '%s' has wrong checksum, removing "
		    "and refetching it again...\n", filename);
		(void)remove(path);
		return rv;
	}
	printf("OK.\n");

	return 0;
}

static int
download_package_list(prop_object_iterator_t iter, bool only_show)
{
	const struct xbps_handle *xhp;
	prop_object_t obj;
	const char *pkgver, *repoloc, *filename, *sha256, *trans;
	char *binfile;
	int rv = 0;
	bool cksum;

	xhp = xbps_handle_get();
again:
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		cksum = false;
		prop_dictionary_get_bool(obj, "checksum_ok", &cksum);
		if (cksum == true)
			continue;

		if (!prop_dictionary_get_cstring_nocopy(obj,
		    "repository", &repoloc))
			continue;
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
		if (only_show) {
			printf("%s\n", binfile);
			free(binfile);
			continue;
		}
		if (xbps_mkpath(xhp->cachedir, 0755) == -1) {
			free(binfile);
			return errno;
		}
		printf("Downloading %s binary package ...\n", pkgver);
		rv = xbps_fetch_file(binfile, xhp->cachedir, false, NULL);
		if (rv == -1) {
			xbps_error_printf("xbps-bin: couldn't download `%s'\n",
			    filename);
			xbps_error_printf("xbps-bin: %s returned: `%s'\n",
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
	prop_object_t obj;
	uint64_t dlsize = 0, instsize = 0;
	const char *tract;
	char size[8];
	bool trans_inst, trans_up, trans_conf, trans_rm;

	trans_inst = trans_up = trans_conf = trans_rm = false;

	while ((obj = prop_object_iterator_next(trans->iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if (strcmp(tract, "install") == 0) {
			trans->inst_pkgcnt++;
			trans_inst = true;
		} else if (strcmp(tract, "update") == 0) {
			trans->up_pkgcnt++;
			trans_up = true;
		} else if (strcmp(tract, "configure") == 0) {
			trans->cf_pkgcnt++;
			trans_conf = true;
		} else if (strcmp(tract, "remove") == 0) {
			trans->rm_pkgcnt++;
			trans_rm = true;
		}
	}
	prop_object_iterator_reset(trans->iter);

	/*
	 * Show the list of packages that will be installed.
	 */
	if (trans_inst) {
		printf("%zu package%s will be installed:\n\n",
		    trans->inst_pkgcnt, trans->inst_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "install");
		printf("\n\n");
	}
	if (trans_up) {
		printf("%zu package%s will be updated:\n\n",
		    trans->up_pkgcnt, trans->up_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "update");
		printf("\n\n");
	}
	if (trans_conf) {
		printf("%zu package%s will be configured:\n\n",
		    trans->cf_pkgcnt, trans->cf_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "configure");
		printf("\n\n");
	}
	if (trans_rm) {
		printf("%zu package%s will be removed:\n\n",
		    trans->rm_pkgcnt, trans->rm_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "remove");
		printf("\n\n");
	}

	/*
	 * Show total download/installed size for all required packages.
	 */
	prop_dictionary_get_uint64(trans->dict, "total-download-size", &dlsize);
	prop_dictionary_get_uint64(trans->dict, "total-installed-size",
	    &instsize);
	if (xbps_humanize_number(size, (int64_t)dlsize) == -1) {
		xbps_error_printf("xbps-bin: error: humanize_number returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("Total download size:\t%6s\n", size);
	if (xbps_humanize_number(size, (int64_t)instsize) == -1) {
		xbps_error_printf("xbps-bin: error: humanize_number2 returns "
		    "%s\n", strerror(errno));
		return -1;
	}
	printf("Total installed size:\t%6s\n\n", size);

	return 0;
}

int
xbps_autoupdate_pkgs(bool yes, bool show_download_pkglist_url)
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
			xbps_error_printf("xbps-bin: unexpected error %s\n",
			    strerror(rv));
			return -1;
		}
	}

	return xbps_exec_transaction(yes, show_download_pkglist_url);
}

int
xbps_install_new_pkg(const char *pkg)
{
	prop_dictionary_t pkgd;
	char *pkgname = NULL, *pkgpatt = NULL;
	int rv = 0;
	bool pkgmatch = false;
	pkg_state_t state;

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
		if ((rv = xbps_get_pkg_state_dictionary(pkgd, &state)) != 0) {
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
			fprintf(stderr, "xbps-bin: unable to locate '%s' in "
			    "repository pool.\n", pkg);
			rv = -1;
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
		xbps_error_printf("xbps-bin: unexpected error %s\n",
		    strerror(rv));
		return -1;
	}
	return 0;
}

static int
exec_transaction(struct transaction *trans)
{
	const struct xbps_handle *xhp;
	prop_dictionary_t instpkgd;
	prop_object_t obj;
	const char *pkgname, *version, *pkgver, *instver, *filen, *tract;
	int rv = 0;
	bool update, preserve, autoinst;
	pkg_state_t state;

	xhp = xbps_handle_get();
	/*
	 * Only show the URLs to download the binary packages.
	 */
	if (trans->only_show)
		return download_package_list(trans->iter, true);
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
	printf("[*] Downloading/integrity check ...\n");
	if ((rv = download_package_list(trans->iter, false)) != 0)
		return rv;
	/*
	 * Remove packages to be replaced.
	 */
	if (trans->rm_pkgcnt > 0) {
		printf("\n[*] Removing packages to be replaced ...\n");
		while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
			prop_dictionary_get_cstring_nocopy(obj, "transaction",
			    &tract);
			if (strcmp(tract, "remove"))
				continue;

			prop_dictionary_get_cstring_nocopy(obj, "pkgname",
			    &pkgname);
			prop_dictionary_get_cstring_nocopy(obj, "version",
			    &version);
			prop_dictionary_get_cstring_nocopy(obj, "pkgver",
			    &pkgver);
			update = false;
			prop_dictionary_get_bool(obj, "remove-and-update",
			    &update);

			/* Remove a package */
			printf("Removing `%s' package ...\n", pkgver);
			rv = xbps_remove_pkg(pkgname, version, update);
			if (rv != 0) {
				xbps_error_printf("xbps-bin: failed to "
				    "remove `%s': %s\n", pkgver,
				    strerror(rv));
				return rv;
			}
		}
		prop_object_iterator_reset(trans->iter);
	}
	/*
	 * Configure pending packages.
	 */
	if (trans->cf_pkgcnt > 0) {
		printf("\n[*] Reconfigure unpacked packages ...\n");
		while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
			prop_dictionary_get_cstring_nocopy(obj, "transaction",
			    &tract);
			if (strcmp(tract, "configure"))
				continue;
			prop_dictionary_get_cstring_nocopy(obj, "pkgname",
			    &pkgname);
			prop_dictionary_get_cstring_nocopy(obj, "version",
			    &version);
			prop_dictionary_get_cstring_nocopy(obj, "pkgver",
			    &pkgver);
			rv = xbps_configure_pkg(pkgname, version, false, false);
			if (rv != 0) {
				xbps_error_printf("xbps-bin: failed to "
				    "configure `%s': %s\n", pkgver, strerror(rv));
				return rv;
			}
		}
		prop_object_iterator_reset(trans->iter);
	}
	/*
	 * Install or update packages in transaction.
	 */
	printf("\n[*] Unpacking packages to be installed/updated ...\n");
	while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		/* Match only packages to be installed or updated */
		if ((strcmp(tract, "remove") == 0) ||
		    (strcmp(tract, "configure") == 0))
			continue;
		autoinst = preserve = false;
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filen);
		prop_dictionary_get_bool(obj, "automatic-install", &autoinst);
		prop_dictionary_get_bool(obj, "preserve",  &preserve);
		/*
		 * If dependency is already unpacked skip this phase.
		 */
		state = 0;
		if (xbps_get_pkg_state_dictionary(obj, &state) != 0)
			return EINVAL;
		if (state == XBPS_PKG_STATE_UNPACKED)
			continue;

		if (strcmp(tract, "update") == 0) {
			/* Update a package */
			instpkgd = xbps_find_pkg_dict_installed(pkgname, false);
			if (instpkgd == NULL) {
				xbps_error_printf("xbps-bin: error: unable to "
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
				xbps_error_printf("xbps-bin: error "
				    "replacing %s-%s (%s)\n", pkgname,
				    instver, strerror(rv));
				return rv;
			}
		}
		/*
		 * Unpack binary package.
		 */
		printf("Unpacking `%s' (from ../%s) ...\n", pkgver, filen);
		if ((rv = xbps_unpack_binary_pkg(obj)) != 0) {
			xbps_error_printf("xbps-bin: error unpacking %s "
			    "(%s)\n", pkgver, strerror(rv));
			return rv;
		}
		/*
		 * Register binary package.
		 */
		if ((rv = xbps_register_pkg(obj, autoinst)) != 0) {
			xbps_error_printf("xbps-bin: error registering %s "
			    "(%s)\n", pkgver, strerror(rv));
			return rv;
		}
	}
	prop_object_iterator_reset(trans->iter);
	/*
	 * Configure all unpacked packages.
	 */
	printf("\n[*] Configuring packages installed/updated ...\n");
	while ((obj = prop_object_iterator_next(trans->iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if ((strcmp(tract, "remove") == 0) ||
		    (strcmp(tract, "configure") == 0))
			continue;
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		update = false;
		if (strcmp(tract, "update") == 0)
			update = true;
		rv = xbps_configure_pkg(pkgname, version, false, update);
		if (rv != 0) {
			xbps_error_printf("xbps-bin: error configuring "
			    "package %s (%s)\n", pkgname, strerror(rv));
			return rv;
		}
		trans->cf_pkgcnt++;
	}
	printf("\nxbps-bin: %zu installed, %zu updated, "
	    "%zu configured, %zu removed.\n", trans->inst_pkgcnt,
	    trans->up_pkgcnt, trans->cf_pkgcnt, trans->rm_pkgcnt);

	return 0;
}

int
xbps_exec_transaction(bool yes, bool show_download_pkglist_url)
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
			show_missing_deps(array);
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
		xbps_error_printf("xbps-bin: error allocating array mem! (%s)\n",
		    strerror(errno));
		goto out;
	}

	trans->yes = yes;
	trans->only_show = show_download_pkglist_url;
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
