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

#define RUN_TRANS_CB(s, d, p, bf, burl) 		\
do {							\
	if (xhp->xbps_transaction_cb != NULL) {		\
		xhp->xtcd->state = s;			\
		xhp->xtcd->desc = d;			\
		xhp->xtcd->pkgver = p;			\
		xhp->xtcd->binpkg_fname = bf;		\
		xhp->xtcd->repourl = burl;		\
		(*xhp->xbps_transaction_cb)(xhp->xtcd);	\
	}						\
} while (0)

#define RUN_TRANS_ERR_CB(s, p, r)				\
do {								\
	if (xhp->xbps_transaction_err_cb != NULL) {		\
		xhp->xtcd->state = s;				\
		xhp->xtcd->pkgver = p;				\
		xhp->xtcd->err = r;				\
		(*xhp->xbps_transaction_err_cb)(xhp->xtcd);	\
	}							\
} while (0)

static int
check_binpkgs_hash(struct xbps_handle *xhp, prop_object_iterator_t iter)
{
	prop_object_t obj;
	const char *pkgver, *repoloc, *filename, *sha256, *trans;
	char *binfile;
	int rv = 0;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		assert(repoloc != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		assert(pkgver != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filename);
		assert(filename != NULL);
		prop_dictionary_get_cstring_nocopy(obj,
		    "filename-sha256", &sha256);
		assert(sha256 != NULL);

		binfile = xbps_path_from_repository_uri(obj, repoloc);
		if (binfile == NULL) {
			rv = EINVAL;
			break;
		}
		RUN_TRANS_CB(XBPS_TRANS_STATE_VERIFY,
		    NULL, pkgver, filename, repoloc);
		rv = xbps_file_hash_check(binfile, sha256);
		if (rv != 0) {
			free(binfile);
			RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_VERIFY, pkgver, rv);
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
	const char *pkgver, *repoloc, *filename, *trans;
	char *binfile;
	int rv = 0;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &trans);
		if ((strcmp(trans, "remove") == 0) ||
		    (strcmp(trans, "configure") == 0))
			continue;

		prop_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);
		assert(repoloc != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		assert(pkgver != NULL);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filename);
		assert(filename != NULL);

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
		if (xbps_mkpath(prop_string_cstring_nocopy(xhp->cachedir),
		    0755) == -1) {
			xbps_error_printf("xbps-bin: cannot mkdir cachedir "
			    "`%s': %s.\n",
			    prop_string_cstring_nocopy(xhp->cachedir),
			    strerror(errno));
			free(binfile);
			rv = errno;
			break;
		}
		RUN_TRANS_CB(XBPS_TRANS_STATE_DOWNLOAD,
		    NULL, pkgver, filename, repoloc);
		/*
		 * Fetch binary package.
		 */
		rv = xbps_fetch_file(binfile,
		    prop_string_cstring_nocopy(xhp->cachedir), false, NULL);
		if (rv == -1) {
			RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_DOWNLOAD, pkgver, errno);
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
	const char *pkgname, *version, *pkgver, *filen, *tract;
	int rv = 0;
	bool update;

	assert(prop_object_type(transd) == PROP_TYPE_DICTIONARY);

	xhp = xbps_handle_get();
	iter = xbps_array_iter_from_dict(transd, "packages");
	if (iter == NULL)
		return EINVAL;
	/*
	 * Download binary packages (if they come from a remote repository).
	 */
	RUN_TRANS_CB(XBPS_TRANS_STATE_DOWNLOAD,
	    "[*] Downloading binary packages", NULL, NULL, NULL);
	if ((rv = download_binpkgs(xhp, iter)) != 0)
		goto out;
	/*
	 * Check SHA256 hashes for binary packages in transaction.
	 */
	RUN_TRANS_CB(XBPS_TRANS_STATE_VERIFY,
	    "[*] Verifying binary package integrity", NULL, NULL, NULL);
	if ((rv = check_binpkgs_hash(xhp, iter)) != 0)
		goto out;
	/*
	 * Install, update, configure or remove packages as specified
	 * in the transaction dictionary.
	 */
	RUN_TRANS_CB(XBPS_TRANS_STATE_INSTALL,
	    "[*] Running transaction tasks", NULL, NULL, NULL);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(obj, "filename", &filen);

		if (strcmp(tract, "remove") == 0) {
			/*
			 * Remove a package.
			 */
			update = false;
			prop_dictionary_get_bool(obj, "remove-and-update",
			    &update);
			RUN_TRANS_CB(XBPS_TRANS_STATE_REMOVE,
			    NULL, pkgver, NULL, NULL);
			rv = xbps_remove_pkg(pkgname, version, update);
			if (rv != 0) {
				RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_REMOVE,
				    pkgver, rv);
				goto out;
			}
			if (update)
				continue;

			RUN_TRANS_CB(XBPS_TRANS_STATE_PURGE,
			    NULL, pkgver, NULL, NULL);
			if ((rv = xbps_purge_pkg(pkgname, false)) != 0) {
				RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_PURGE,
				    pkgver, rv);
				goto out;
			}
		} else if (strcmp(tract, "configure") == 0) {
			/*
			 * Reconfigure pending package.
			 */
			rv = xbps_configure_pkg(pkgname, version, false, false);
			if (rv != 0) {
				RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_CONFIGURE,
				    pkgver, rv);
				goto out;
			}
		} else {
			/*
			 * Install or update a package.
			 */
			if (strcmp(tract, "update") == 0)
				update = true;

			if (update) {
				/*
				 * Update a package: execute pre-remove
				 * action if found before unpacking.
				 */
				RUN_TRANS_CB(XBPS_TRANS_STATE_UPDATE,
				    NULL, pkgver, filen, NULL);
				rv = xbps_remove_pkg(pkgname, version, true);
				if (rv != 0) {
					RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_UPDATE,
					    pkgver, rv);
					goto out;
				}
			}
			/*
			 * Unpack binary package.
			 */
			RUN_TRANS_CB(XBPS_TRANS_STATE_UNPACK, NULL,
			    pkgver, filen, NULL);
			if ((rv = xbps_unpack_binary_pkg(obj)) != 0) {
				RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_UNPACK,
				    pkgver, rv);
				goto out;
			}
			/*
			 * Register package.
			 */
			RUN_TRANS_CB(XBPS_TRANS_STATE_REGISTER,
			    NULL, pkgver, filen, NULL);
			if ((rv = xbps_register_pkg(obj)) != 0) {
				RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_REGISTER,
				    pkgver, rv);
				goto out;
			}
		}
	}
	prop_object_iterator_reset(iter);
	/*
	 * Configure all unpacked packages.
	 */
	RUN_TRANS_CB(XBPS_TRANS_STATE_CONFIGURE,
	    "[*] Configuring unpacked packages", NULL, NULL, NULL);

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

		if ((rv = xbps_configure_pkg(pkgname, version,
		    false, update)) != 0) {
			RUN_TRANS_ERR_CB(XBPS_TRANS_STATE_CONFIGURE,
			    pkgver, rv);
			goto out;
		}
	}

out:
	prop_object_iterator_release(iter);

	return rv;
}
