/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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
#include <unistd.h>
#include <limits.h>

#include "xbps_api_impl.h"

/**
 * @file lib/transaction_commit.c
 * @brief Transaction handling routines
 * @defgroup transaction Transaction handling functions
 *
 * The following image shows off the full transaction dictionary returned
 * by xbps_transaction_prepare().
 *
 * @image html images/xbps_transaction_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: The transaction dictionary.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *  - <b>Green bg box</b>: possible value set in the object, only one of them
 *    will be set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static int
check_binpkgs_hash(struct xbps_handle *xhp, prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *arch, *repoloc, *sha256, *trans;
	char *binfile, *filen;
	int rv = 0;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj,
		    "filename-sha256", &sha256);

		binfile = xbps_repository_pkg_path(xhp, obj);
		if (binfile == NULL) {
			rv = EINVAL;
			break;
		}
		filen = xbps_xasprintf("%s.%s.xbps", pkgver, arch);

		xbps_set_cb_state(xhp, XBPS_STATE_VERIFY, 0, pkgver,
		    "Verifying `%s' package integrity...", filen, repoloc);
		rv = xbps_file_hash_check(binfile, sha256);
		if (rv != 0) {
			free(binfile);
			xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL,
			    rv, pkgver,
			    "Failed to verify `%s' package integrity: %s",
			    filen, strerror(rv));
			free(filen);
			break;
		}
		free(binfile);
		free(filen);
	}
	prop_object_iterator_reset(iter);

	return rv;
}

static int
download_binpkgs(struct xbps_handle *xhp, prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *arch, *fetchstr, *repoloc, *trans;
	char *binfile, *filen;
	int rv = 0;
	bool state_dload = false;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		binfile = xbps_repository_pkg_path(xhp, obj);
		if (binfile == NULL) {
			rv = EINVAL;
			break;
		}
		/*
		 * If downloaded package is in cachedir continue.
		 */
		if (access(binfile, R_OK) == 0) {
			free(binfile);
			continue;
		}
		/*
		 * Create cachedir.
		 */
		if (access(xhp->cachedir, R_OK|X_OK|W_OK) == -1) {
			if (xbps_mkpath(xhp->cachedir, 0755) == -1) {
				xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL,
				    errno, pkgver,
				    "%s: [trans] cannot create cachedir `%s':"
				    "%s", pkgver, xhp->cachedir,
				    strerror(errno));
				free(binfile);
				rv = errno;
				break;
			}
		}
		if (state_dload == false) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_DOWNLOAD,
			    0, NULL, NULL);
			state_dload = true;
		}
		filen = xbps_xasprintf("%s.%s.xbps", pkgver, arch);
		xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD,
		    0, pkgver, "Downloading binary package `%s' (from `%s')...",
		    filen, repoloc);
		/*
		 * Fetch binary package.
		 */
		if (chdir(xhp->cachedir) == -1) {
			xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL,
			    errno, pkgver,
			    "%s: [trans] failed to change dir to cachedir"
			    "`%s': %s", pkgver, xhp->cachedir,
			    strerror(errno));
			rv = errno;
			free(binfile);
			free(filen);
			break;
		}

		rv = xbps_fetch_file(xhp, binfile, NULL);
		if (rv == -1) {
			fetchstr = xbps_fetch_error_string();
			xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL,
			    fetchLastErrCode != 0 ? fetchLastErrCode : errno,
			    pkgver, "%s: [trans] failed to download binary package "
			    "`%s' from `%s': %s", pkgver, filen, repoloc,
			    fetchstr ? fetchstr : strerror(errno));
			free(binfile);
			free(filen);
			break;
		}
		rv = 0;
		free(binfile);
		free(filen);
	}
	prop_object_iterator_reset(iter);

	return rv;
}

int
xbps_transaction_commit(struct xbps_handle *xhp)
{
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgver, *tract;
	int rv = 0;
	bool update, install, sr;

	assert(prop_object_type(xhp->transd) == PROP_TYPE_DICTIONARY);

	update = install = false;
	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;
	/*
	 * Download binary packages (if they come from a remote repository).
	 */
	if ((rv = download_binpkgs(xhp, iter)) != 0)
		goto out;
	/*
	 * Check SHA256 hashes for binary packages in transaction.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_VERIFY, 0, NULL, NULL);
	if ((rv = check_binpkgs_hash(xhp, iter)) != 0)
		goto out;
	/*
	 * Install, update, configure or remove packages as specified
	 * in the transaction dictionary.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_RUN, 0, NULL, NULL);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		update = false;
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		if (strcmp(tract, "remove") == 0) {
			update = false;
			sr = false;
			/*
			 * Remove package.
			 */
			prop_dictionary_get_bool(obj, "remove-and-update",
			    &update);
			prop_dictionary_get_bool(obj, "softreplace", &sr);
			rv = xbps_remove_pkg(xhp, pkgver, update, sr);
			if (rv != 0) {
				xbps_dbg_printf(xhp, "[trans] failed to "
				    "remove %s\n", pkgver);
				goto out;
			}
		} else if (strcmp(tract, "configure") == 0) {
			/*
			 * Reconfigure pending package.
			 */
			rv = xbps_configure_pkg(xhp, pkgver, false, false, false);
			if (rv != 0)
				goto out;
		} else {
			/*
			 * Install or update a package.
			 */
			if (strcmp(tract, "update") == 0)
				update = true;
			else
				install = true;

			if (update && xbps_pkgdb_get_pkg(xhp, pkgver)) {
				/*
				 * Update a package: execute pre-remove
				 * action if found before unpacking.
				 */
				xbps_set_cb_state(xhp, XBPS_STATE_UPDATE, 0,
				    pkgver, NULL);
				rv = xbps_remove_pkg(xhp, pkgver, true, false);
				if (rv != 0) {
					xbps_set_cb_state(xhp,
					    XBPS_STATE_UPDATE_FAIL,
					    rv, pkgver,
					    "%s: [trans] failed to update "
					    "package `%s'", pkgver,
					    strerror(rv));
					goto out;
				}
			} else {
				/* Install a package */
				xbps_set_cb_state(xhp, XBPS_STATE_INSTALL,
				    0, pkgver, NULL);
			}
			/*
			 * Unpack binary package.
			 */
			if ((rv = xbps_unpack_binary_pkg(xhp, obj)) != 0)
				goto out;
			/*
			 * Register package.
			 */
			if ((rv = xbps_register_pkg(xhp, obj)) != 0)
				goto out;
		}
	}
	/* if there are no packages to install or update we are done */
	if (!update && !install)
		goto out;

	/* if installing packages for target_arch, don't configure anything */
	if (xhp->target_arch && strcmp(xhp->native_arch, xhp->target_arch))
		goto out;

	prop_object_iterator_reset(iter);

	/*
	 * Configure all unpacked packages.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_CONFIGURE, 0, NULL, NULL);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if ((strcmp(tract, "remove") == 0) ||
		    (strcmp(tract, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		update = false;
		if (strcmp(tract, "update") == 0)
			update = true;

		rv = xbps_configure_pkg(xhp, pkgver, false, update, true);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "%s: configure failed for "
			    "%s: %s\n", pkgver, strerror(rv));
			goto out;
		}
		/*
		 * Notify client callback when a package has been
		 * installed or updated.
		 */
		if (update) {
			xbps_set_cb_state(xhp, XBPS_STATE_UPDATE_DONE, 0,
			    pkgver, NULL);
		} else {
			xbps_set_cb_state(xhp, XBPS_STATE_INSTALL_DONE, 0,
			    pkgver, NULL);
		}
	}

out:
	prop_object_iterator_release(iter);

	return rv;
}
