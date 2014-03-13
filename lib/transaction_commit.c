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
check_binpkgs(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_object_t obj;
	struct xbps_repo *repo;
	const char *pkgver, *repoloc, *trans, *sha256;
	char *binfile;
	int rv = 0;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		xbps_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		binfile = xbps_repository_pkg_path(xhp, obj);
		if (binfile == NULL) {
			rv = ENOMEM;
			break;
		}
		/*
		 * For pkgs in local repos check the sha256 hash.
		 * For pkgs in remote repos check the RSA signature.
		 */
		if ((repo = xbps_rpool_get_repo(repoloc)) == NULL) {
			rv = errno;
			xbps_dbg_printf(xhp, "%s: failed to get repository "
			    "%s: %s\n", pkgver, repoloc, strerror(errno));
			break;
		}
		if (repo->is_remote) {
			/* remote repo */
			xbps_set_cb_state(xhp, XBPS_STATE_VERIFY, 0, pkgver,
			    "%s: verifying RSA signature...", pkgver);

			if (!xbps_verify_file_signature(repo, binfile)) {
				rv = EPERM;
				xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
				    "%s: the RSA signature is not valid!", pkgver);
				free(binfile);
				break;
			}
		} else {
			/* local repo */
			xbps_set_cb_state(xhp, XBPS_STATE_VERIFY, 0, pkgver,
			    "%s: verifying SHA256 hash...", pkgver);
			xbps_dictionary_get_cstring_nocopy(obj, "filename-sha256", &sha256);
			if ((rv = xbps_file_hash_check(binfile, sha256)) != 0) {
				xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
				    "%s: SHA256 hash is not valid!", pkgver, strerror(rv));
				free(binfile);
				break;
			}

		}
		free(binfile);
	}
	xbps_object_iterator_reset(iter);

	return rv;
}

static int
download_binpkgs(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_object_t obj;
	const char *pkgver, *arch, *fetchstr, *repoloc, *trans;
	char *file, *sigfile;
	int rv = 0;

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		xbps_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		if (!xbps_repository_is_remote(repoloc))
			continue;

		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "architecture", &arch);

		/*
		 * Download binary package.
		 */
		if ((file = xbps_repository_pkg_path(xhp, obj)) == NULL) {
			rv = EINVAL;
			break;
		}
		if (access(file, R_OK) == -1) {
			xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD, 0, pkgver,
			    "Downloading `%s' package (from `%s')...", pkgver, repoloc);
			if ((rv = xbps_fetch_file(xhp, file, NULL)) == -1) {
				fetchstr = xbps_fetch_error_string();
				xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL,
				    fetchLastErrCode != 0 ? fetchLastErrCode : errno,
				    pkgver, "[trans] failed to download `%s' package from `%s': %s",
				    pkgver, repoloc, fetchstr ? fetchstr : strerror(errno));
				free(file);
				break;
			}
			rv = 0;
		}
		/*
		 * Download binary package signature.
		 */
		sigfile = xbps_xasprintf("%s.sig", file);
		free(file);
		file = NULL;
		if (access(sigfile, R_OK) == -1) {
			xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD, 0, pkgver,
			    "Downloading `%s' signature (from `%s')...", pkgver, repoloc);
			file = xbps_xasprintf("%s/%s.%s.xbps.sig", repoloc, pkgver, arch);
			if ((rv = xbps_fetch_file(xhp, file, NULL)) == -1) {
				fetchstr = xbps_fetch_error_string();
				xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL,
				    fetchLastErrCode != 0 ? fetchLastErrCode : errno,
				    pkgver, "[trans] failed to download `%s' signature from `%s': %s",
				    pkgver, repoloc, fetchstr ? fetchstr : strerror(errno));
				free(sigfile);
				free(file);
				break;
			}
			rv = 0;
		}
		free(sigfile);
		if (file != NULL)
			free(file);
	}
	xbps_object_iterator_reset(iter);

	return rv;
}

int
xbps_transaction_commit(struct xbps_handle *xhp)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *pkgver, *tract;
	int rv = 0;
	bool update;

	assert(xbps_object_type(xhp->transd) == XBPS_TYPE_DICTIONARY);
	/*
	 * Create cachedir if necessary.
	 */
	if (access(xhp->cachedir, R_OK|X_OK|W_OK) == -1) {
		if (xbps_mkpath(xhp->cachedir, 0755) == -1) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL,
			    errno, NULL,
			    "[trans] cannot create cachedir `%s': %s",
			    xhp->cachedir, strerror(errno));
			return errno;
		}
	}
	if (chdir(xhp->cachedir) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL,
		    errno, NULL,
		    "[trans] failed to change dir to cachedir `%s': %s",
		    xhp->cachedir, strerror(errno));
		return errno;
	}
	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;
	/*
	 * Download binary packages (if they come from a remote repository).
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_DOWNLOAD, 0, NULL, NULL);
	if ((rv = download_binpkgs(xhp, iter)) != 0) {
		xbps_dbg_printf(xhp, "[trans] failed to download binpkgs: "
		    "%s\n", strerror(rv));
		goto out;
	}
	/*
	 * Check binary package integrity.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_VERIFY, 0, NULL, NULL);
	if ((rv = check_binpkgs(xhp, iter)) != 0) {
		xbps_dbg_printf(xhp, "[trans] failed to check binpkgs: "
		    "%s\n", strerror(rv));
		goto out;
	}
	/*
	 * Install, update, configure or remove packages as specified
	 * in the transaction dictionary.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_RUN, 0, NULL, NULL);

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		if (strcmp(tract, "remove") == 0) {
			/*
			 * Remove package.
			 */
			update = false;
			xbps_dictionary_get_bool(obj, "remove-and-update", &update);
			rv = xbps_remove_pkg(xhp, pkgver, update);
			if (rv != 0) {
				xbps_dbg_printf(xhp, "[trans] failed to "
				    "remove %s: %s\n", pkgver, strerror(rv));
				goto out;
			}
			continue;

		} else if (strcmp(tract, "configure") == 0) {
			/*
			 * Reconfigure pending package.
			 */
			rv = xbps_configure_pkg(xhp, pkgver, false, false);
			if (rv != 0) {
				xbps_dbg_printf(xhp, "[trans] failed to "
				    "configure %s: %s\n", pkgver, strerror(rv));
				goto out;
			}
			continue;

		} else if (strcmp(tract, "update") == 0) {
			/*
			 * Update a package: execute pre-remove action of
			 * existing package before unpacking new version.
			 */
			xbps_set_cb_state(xhp, XBPS_STATE_UPDATE, 0, pkgver, NULL);
			rv = xbps_remove_pkg(xhp, pkgver, true);
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
			xbps_set_cb_state(xhp, XBPS_STATE_INSTALL, 0,
			    pkgver, NULL);
		}
		/*
		 * Unpack binary package.
		 */
		if ((rv = xbps_unpack_binary_pkg(xhp, obj)) != 0) {
			xbps_dbg_printf(xhp, "[trans] failed to unpack "
			    "%s: %s\n", pkgver, strerror(rv));
			goto out;
		}
		/*
		 * Register package.
		 */
		if ((rv = xbps_register_pkg(xhp, obj)) != 0) {
			xbps_dbg_printf(xhp, "[trans] failed to register "
			    "%s: %s\n", pkgver, strerror(rv));
			goto out;
		}
	}
	/* if there are no packages to install or update we are done */
	if (!xbps_dictionary_get(xhp->transd, "total-update-pkgs") &&
	    !xbps_dictionary_get(xhp->transd, "total-install-pkgs"))
		goto out;

	/* if installing packages for target_arch, don't configure anything */
	if (xhp->target_arch && strcmp(xhp->native_arch, xhp->target_arch))
		goto out;

	xbps_object_iterator_reset(iter);
	/* Force a pkgdb write for all unpacked pkgs in transaction */
	(void)xbps_pkgdb_update(xhp, true);

	/*
	 * Configure all unpacked packages.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_CONFIGURE, 0, NULL, NULL);

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if ((strcmp(tract, "remove") == 0) ||
		    (strcmp(tract, "configure") == 0))
			continue;

		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		update = false;
		if (strcmp(tract, "update") == 0)
			update = true;

		rv = xbps_configure_pkg(xhp, pkgver, false, update);
		if (rv != 0) {
			xbps_dbg_printf(xhp, "%s: configure failed for "
			    "%s: %s\n", __func__, pkgver, strerror(rv));
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
	xbps_object_iterator_release(iter);
	/* Force a pkgdb write for all unpacked pkgs in transaction */
	(void)xbps_pkgdb_update(xhp, true);

	return rv;
}
