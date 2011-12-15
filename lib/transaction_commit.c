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

#include "xbps_api_impl.h"

static int
check_binpkgs_hash(prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *repoloc, *filen, *sha256, *trans;
	const char *pkgname, *version;
	char *binfile;
	int rv = 0;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		assert(repoloc != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		assert(pkgver != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filen);
		assert(filen != NULL);
		prop_dictionary_get_cstring_nocopy(obj,
		    "filename-sha256", &sha256);
		assert(sha256 != NULL);

		binfile = xbps_path_from_repository_uri(obj, repoloc);
		if (binfile == NULL) {
			rv = EINVAL;
			break;
		}
		xbps_set_cb_state(XBPS_STATE_VERIFY, 0, pkgname, version,
		    "Verifying `%s' package integrity...", filen, repoloc);
		rv = xbps_file_hash_check(binfile, sha256);
		if (rv != 0) {
			free(binfile);
			xbps_set_cb_state(XBPS_STATE_VERIFY_FAIL,
			    rv, pkgname, version,
			    "Failed to verify `%s' package integrity: %s",
			    filen, strerror(rv));
			break;
		}
		free(binfile);
	}
	prop_object_iterator_reset(iter);

	return rv;
}

static int
download_binpkgs(struct xbps_handle *xhp, prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *repoloc, *filen, *trans;
	const char *pkgname, *version;
	char *binfile;
	int rv = 0;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		assert(repoloc != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		assert(pkgver != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filen);
		assert(filen != NULL);

		binfile = xbps_path_from_repository_uri(obj, repoloc);
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
		if (xbps_mkpath(xhp->cachedir, 0755) == -1) {
			xbps_set_cb_state(XBPS_STATE_DOWNLOAD_FAIL,
			    errno, pkgname, version,
			    "%s: [trans] cannot create cachedir `%s': %s",
			    pkgver, xhp->cachedir, strerror(errno));
			free(binfile);
			rv = errno;
			break;
		}
		xbps_set_cb_state(XBPS_STATE_DOWNLOAD,
		    0, pkgname, version,
		    "Downloading binary package `%s' (from `%s')...",
		    filen, repoloc);
		/*
		 * Fetch binary package.
		 */
		rv = xbps_fetch_file(binfile, xhp->cachedir, false, NULL);
		if (rv == -1) {
			xbps_set_cb_state(XBPS_STATE_DOWNLOAD_FAIL,
			    errno, pkgname, version,
			    "%s: [trans] failed to download binary package "
			    "`%s' from `%s': %s", pkgver, filen, repoloc,
			    strerror(errno));
			free(binfile);
			break;
		}
		rv = 0;
		free(binfile);
	}
	prop_object_iterator_reset(iter);

	return rv;
}

int
xbps_transaction_commit(prop_dictionary_t transd)
{
	struct xbps_handle *xhp;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *pkgname, *version, *pkgver, *tract;
	int rv = 0;
	bool update, install, purge;

	assert(prop_object_type(transd) == PROP_TYPE_DICTIONARY);

	update = install = purge = false;
	xhp = xbps_handle_get();
	iter = xbps_array_iter_from_dict(transd, "packages");
	if (iter == NULL)
		return EINVAL;
	/*
	 * Download binary packages (if they come from a remote repository).
	 */
	xbps_set_cb_state(XBPS_STATE_TRANS_DOWNLOAD, 0, NULL, NULL, NULL);
	if ((rv = download_binpkgs(xhp, iter)) != 0)
		goto out;
	/*
	 * Check SHA256 hashes for binary packages in transaction.
	 */
	xbps_set_cb_state(XBPS_STATE_TRANS_VERIFY, 0, NULL, NULL, NULL);
	if ((rv = check_binpkgs_hash(iter)) != 0)
		goto out;
	/*
	 * Install, update, configure or remove packages as specified
	 * in the transaction dictionary.
	 */
	xbps_set_cb_state(XBPS_STATE_TRANS_RUN, 0, NULL, NULL, NULL);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		update = false;
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		if (strcmp(tract, "remove") == 0) {
			purge = update = false;
			/*
			 * Remove and optionally also purge package.
			 */
			prop_dictionary_get_bool(obj, "remove-and-update",
			    &update);
			prop_dictionary_get_bool(obj, "remove-and-purge",
			    &purge);
			rv = xbps_remove_pkg(pkgname, version, update);
			if (rv != 0)
				goto out;
			if (update || !purge)
				continue;

			if ((rv = xbps_purge_pkg(pkgname, false)) != 0)
				goto out;
		} else if (strcmp(tract, "configure") == 0) {
			/*
			 * Reconfigure pending package.
			 */
			rv = xbps_configure_pkg(pkgname, version, false, false);
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

			if (update) {
				/*
				 * Update a package: execute pre-remove
				 * action if found before unpacking.
				 */
				xbps_set_cb_state(XBPS_STATE_UPDATE, 0,
				    pkgname, version, NULL);
				rv = xbps_remove_pkg(pkgname, version, true);
				if (rv != 0) {
					xbps_set_cb_state(
					    XBPS_STATE_UPDATE_FAIL,
					    rv, pkgname, version,
					    "%s: [trans] failed to update "
					    "package to `%s': %s", pkgver,
					    version, strerror(rv));
					goto out;
				}
			} else {
				/* Install a package */
				xbps_set_cb_state(XBPS_STATE_INSTALL, 0,
				    pkgname, version, NULL);
			}
			/*
			 * Unpack binary package.
			 */
			if ((rv = xbps_unpack_binary_pkg(obj)) != 0)
				goto out;
			/*
			 * Register package.
			 */
			if ((rv = xbps_register_pkg(obj)) != 0)
				goto out;
		}
	}
	prop_object_iterator_reset(iter);

	/* if there are no packages to install or update we are done */
	if (!update && !install)
		goto out;
	/*
	 * Configure all unpacked packages.
	 */
	xbps_set_cb_state(XBPS_STATE_TRANS_CONFIGURE, 0, NULL, NULL, NULL);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
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
		if (rv != 0)
			goto out;
		/*
		 * Notify client callback when a package has been
		 * installed or updated.
		 */
		if (update) {
			xbps_set_cb_state(XBPS_STATE_UPDATE_DONE, 0,
			    pkgname, version, NULL);
		} else {
			xbps_set_cb_state(XBPS_STATE_INSTALL_DONE, 0,
			    pkgname, version, NULL);
		}
	}

out:
	prop_object_iterator_release(iter);

	return rv;
}
