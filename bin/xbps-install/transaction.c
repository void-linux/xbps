/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
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

#include <xbps.h>
#include "defs.h"

struct transaction {
	xbps_dictionary_t d;
	xbps_object_iterator_t iter;
	uint32_t inst_pkgcnt;
	uint32_t up_pkgcnt;
	uint32_t cf_pkgcnt;
	uint32_t rm_pkgcnt;
	uint32_t dl_pkgcnt;
};

static void
print_array(xbps_array_t a)
{
	const char *str;

	for (unsigned int i = 0; i < xbps_array_count(a); i++) {
		xbps_array_get_cstring_nocopy(a, i, &str);
		fprintf(stderr, "%s\n", str);
	}
}

static void
show_actions(xbps_object_iterator_t iter)
{
	xbps_object_t obj;
	const char *repoloc, *trans, *pkgver, *arch;

	repoloc = trans = pkgver = arch = NULL;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		printf("%s %s", pkgver, trans);
		xbps_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		xbps_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
		if (repoloc && arch)
			printf(" %s %s", arch, repoloc);

		printf("\n");
	}
}

static void
show_package_list(xbps_object_iterator_t iter, const char *match, int cols)
{
	xbps_object_t obj;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		const char *pkgver, *tract;
		bool dload = false;

		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		xbps_dictionary_get_bool(obj, "download", &dload);

		if ((match && (strcmp(match, tract) == 0)) || (!match && dload))
			print_package_line(pkgver, cols, false);
	}
	xbps_object_iterator_reset(iter);
	print_package_line(NULL, cols, true);
}

static int
show_transaction_sizes(struct transaction *trans, int cols)
{
	uint64_t dlsize = 0, instsize = 0, rmsize = 0, disk_free_size = 0;
	char size[8];

	/*
	 * Show the list of packages that will be downloaded, installed, updated,
	 * removed or configured.
	 */
	xbps_dictionary_get_uint32(trans->d, "total-download-pkgs",
	    &trans->dl_pkgcnt);
	if (trans->dl_pkgcnt) {
		printf("%u package%s will be downloaded:\n",
		    trans->dl_pkgcnt, trans->dl_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, NULL, cols);
		printf("\n");
	}
	xbps_dictionary_get_uint32(trans->d, "total-install-pkgs",
	    &trans->inst_pkgcnt);
	if (trans->inst_pkgcnt) {
		printf("%u package%s will be installed:\n",
		    trans->inst_pkgcnt, trans->inst_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "install", cols);
		printf("\n");
	}
	xbps_dictionary_get_uint32(trans->d, "total-update-pkgs",
	    &trans->up_pkgcnt);
	if (trans->up_pkgcnt) {
		printf("%u package%s will be updated:\n",
		    trans->up_pkgcnt, trans->up_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "update", cols);
		printf("\n");
	}
	xbps_dictionary_get_uint32(trans->d, "total-configure-pkgs",
	    &trans->cf_pkgcnt);
	if (trans->cf_pkgcnt) {
		printf("%u package%s will be configured:\n",
		    trans->cf_pkgcnt, trans->cf_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "configure", cols);
		printf("\n");
	}
	xbps_dictionary_get_uint32(trans->d, "total-remove-pkgs",
	    &trans->rm_pkgcnt);
	if (trans->rm_pkgcnt) {
		printf("%u package%s will be removed:\n",
		    trans->rm_pkgcnt, trans->rm_pkgcnt == 1 ? "" : "s");
		show_package_list(trans->iter, "remove", cols);
		printf("\n");
	}
	/*
	 * Show total download/installed/removed size for all required packages.
	 */
	xbps_dictionary_get_uint64(trans->d, "total-download-size", &dlsize);
	xbps_dictionary_get_uint64(trans->d, "total-installed-size", &instsize);
	xbps_dictionary_get_uint64(trans->d, "total-removed-size", &rmsize);
	xbps_dictionary_get_uint64(trans->d, "disk-free-size", &disk_free_size);
	if (dlsize || instsize || rmsize || disk_free_size)
		printf("\n");

	if (dlsize) {
		if (xbps_humanize_number(size, (int64_t)dlsize) == -1) {
			xbps_error_printf("humanize_number returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Size to download:             %6s\n", size);
	}
	if (instsize) {
		if (xbps_humanize_number(size, (int64_t)instsize) == -1) {
			xbps_error_printf("humanize_number2 returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Size required on disk:        %6s\n", size);
	}
	if (rmsize) {
		if (xbps_humanize_number(size, (int64_t)rmsize) == -1) {
			xbps_error_printf("humanize_number3 returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Size freed on disk:           %6s\n", size);
	}
	if (disk_free_size) {
		if (xbps_humanize_number(size, (int64_t)disk_free_size) == -1) {
			xbps_error_printf("humanize_number3 returns "
			    "%s\n", strerror(errno));
			return -1;
		}
		printf("Free space on disk:           %6s\n", size);
	}
	printf("\n");

	return 0;
}

int
dist_upgrade(struct xbps_handle *xhp, int cols, bool yes, bool drun)
{
	int rv = 0;

	if ((rv = xbps_transaction_update_packages(xhp)) != 0) {
		if (rv == ENOENT) {
			printf("No packages currently registered.\n");
			return 0;
		} else if (rv == EEXIST) {
			return 0;
		} else if (rv == ENOTSUP) {
			fprintf(stderr, "No repositories currently "
			    "registered!\n");
			return -1;
		} else {
			fprintf(stderr, "Unexpected error %s\n",
			    strerror(rv));
			return -1;
		}
	}

	return exec_transaction(xhp, cols, yes, drun);
}

int
install_new_pkg(struct xbps_handle *xhp, const char *pkg, bool reinstall)
{
	int rv;

	if ((rv = xbps_transaction_install_pkg(xhp, pkg, reinstall)) != 0) {
		if (rv == EEXIST) {
			printf("Package `%s' already installed.\n", pkg);
		} else if (rv == ENOENT) {
			fprintf(stderr, "Unable to locate '%s' in "
			    "repository pool.\n", pkg);
		} else if (rv == ENOTSUP) {
			fprintf(stderr, "No repositories  "
			    "currently registered!\n");
		} else if (rv == ENXIO) {
			fprintf(stderr, "Package `%s' contains invalid dependencies, exiting.\n", pkg);
		} else {
			fprintf(stderr, "Unexpected error: %s\n",
			    strerror(rv));
			rv = -1;
		}
	}
	return rv;
}

int
update_pkg(struct xbps_handle *xhp, const char *pkgname)
{
	int rv;

	rv = xbps_transaction_update_pkg(xhp, pkgname);
	if (rv == EEXIST)
		printf("Package '%s' is up to date.\n", pkgname);
	else if (rv == ENOENT)
		fprintf(stderr, "Package '%s' not found in "
		    "repository pool.\n", pkgname);
	else if (rv == ENODEV)
		printf("Package '%s' not installed.\n", pkgname);
	else if (rv == ENOTSUP)
		fprintf(stderr, "No repositories currently registered!\n");
	else if (rv != 0) {
		fprintf(stderr, "Unexpected error: %s\n", strerror(rv));
		return -1;
	}
	return rv;
}

int
exec_transaction(struct xbps_handle *xhp, int maxcols, bool yes, bool drun)
{
	xbps_array_t array;
	struct transaction *trans;
	int rv = 0;

	trans = calloc(1, sizeof(*trans));
	if (trans == NULL)
		return ENOMEM;

	if ((rv = xbps_transaction_prepare(xhp)) != 0) {
		if (rv == ENODEV) {
			array = xbps_dictionary_get(xhp->transd, "missing_deps");
			/* missing packages */
			print_array(array);
			fprintf(stderr, "Transaction aborted due to missing packages.\n");
			goto out;
		} else if (rv == EAGAIN) {
			/* conflicts */
			array = xbps_dictionary_get(xhp->transd, "conflicts");
			print_array(array);
			fprintf(stderr, "Transaction aborted due to conflicting packages.\n");
			goto out;
		}
		xbps_dbg_printf(xhp, "Empty transaction dictionary: %s\n",
		    strerror(errno));
		goto out;
	}
	xbps_dbg_printf(xhp, "Dictionary before transaction happens:\n");
	xbps_dbg_printf_append(xhp, "%s",
	    xbps_dictionary_externalize(xhp->transd));

	trans->d = xhp->transd;
	trans->iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	assert(trans->iter);

	/*
	 * dry-run mode, show what would be done but don't run anything.
	 */
	if (drun) {
		show_actions(trans->iter);
		goto out;
	}
	/*
	 * Show download/installed size for the transaction.
	 */
	if ((rv = show_transaction_sizes(trans, maxcols)) != 0)
		goto out;
	/*
	 * Ask interactively (if -y not set).
	 */
	if (!yes && !yesno("Do you want to continue?")) {
		printf("Aborting!\n");
		goto out;
	}
	/*
	 * It's time to run the transaction!
	 */
	if ((rv = xbps_transaction_commit(xhp)) == 0) {
		printf("\n%u downloaded, %u installed, %u updated, "
		    "%u configured, %u removed.\n",
		    trans->dl_pkgcnt, trans->inst_pkgcnt,
		    trans->up_pkgcnt, trans->cf_pkgcnt + trans->inst_pkgcnt,
		    trans->rm_pkgcnt);
	}
out:
	if (trans->iter)
		xbps_object_iterator_release(trans->iter);
	if (trans)
		free(trans);
	return rv;
}
