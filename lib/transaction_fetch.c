/*-
 * Copyright (c) 2009-2015 Juan Romero Pardines.
 * Copyright (c) 2019      Duncan Overbruck <mail@duncano.de>.
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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

#include "xbps_api_impl.h"

static int
verify_binpkg(struct xbps_handle *xhp, xbps_dictionary_t pkgd)
{
	struct xbps_repo *repo;
	const char *pkgver, *repoloc, *sha256;
	char *binfile;
	struct xbps_sha256_digest digest;
	int rv = 0;

	xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	binfile = xbps_repository_pkg_path(xhp, pkgd);
	if (binfile == NULL) {
		return ENOMEM;
	}
	/*
	 * For all pkgs, check the sha256 hash.
	 * For pkgs in local repos, check the RSA sig if requested.
	 * For pkgs in remote repos, always check the RSA signature.
	 */
	if ((repo = xbps_rpool_get_repo(repoloc)) == NULL) {
		rv = errno;
		xbps_dbg_printf(xhp, "%s: failed to get repository "
			"%s: %s\n", pkgver, repoloc, strerror(errno));
		goto out;
	}

	/* check sha256 */
	xbps_set_cb_state(xhp, XBPS_STATE_VERIFY, 0, pkgver,
		"%s: verifying SHA256 hash...", pkgver);
	xbps_dictionary_get_cstring_nocopy(pkgd, "filename-sha256", &sha256);
	if ((rv = xbps_file_sha256_check_raw(binfile, sha256, &digest)) != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
			"%s: SHA256 hash is not valid: %s", pkgver, strerror(rv));
		goto out;
	}

	if (repo->is_remote || xhp->flags & XBPS_FLAG_VERIFY_LOCAL_REPO) {
		char sigfile[PATH_MAX];
		if ((rv = xbps_file_sig_path(sigfile, sizeof sigfile, NULL, "%s.sig", binfile))) {
			xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv,
				pkgver, "[trans] can't access signature for `%s': %s",
				pkgver, strerror(rv));
			goto out;
		}

		/* check RSA sig */
		xbps_set_cb_state(xhp, XBPS_STATE_VERIFY, 0, pkgver,
			"%s: verifying RSA signature...", pkgver);

		if (!xbps_verify_signature(repo, sigfile, &digest)) {
			rv = EPERM;
			xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
				"%s: the RSA signature is not valid!", pkgver);
			if (repo->is_remote) {
				/*
				 * Don't remove files from local repositories, since we might
				 * not be the "owner"; with the XBPS cache, we are the owners.
				 */
				const char *errmsg;
				if (remove(binfile) == 0 && remove(sigfile) == 0) {
					errmsg = "removed pkg archive and its signature";
				} else {
					errmsg = "there was an error removing pkg archive and its signature";
				}
				xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
					"%s: %s.", pkgver, errmsg);
			}
			goto out;
		}
	}
out:
	free(binfile);
	return rv;
}

static int
download_binpkg(struct xbps_handle *xhp, xbps_dictionary_t repo_pkgd)
{
	struct xbps_repo *repo;
	/* FIXME: increase max length for remote requests? */
	char buf[PATH_MAX];
	char *sigsuffix;
	const char *pkgver, *arch, *fetchstr, *repoloc;
	struct xbps_sha256_digest digest = {0};
	int rv = 0;

	xbps_dictionary_get_cstring_nocopy(repo_pkgd, "repository", &repoloc);
	if (!xbps_repository_is_remote(repoloc))
		return ENOTSUP;

	xbps_dictionary_get_cstring_nocopy(repo_pkgd, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(repo_pkgd, "architecture", &arch);

	if ((rv = xbps_file_sig_path(buf, sizeof buf, &sigsuffix, "%s/%s.%s.xbps.sig", repoloc, pkgver, arch))) {
		xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL, rv,
			pkgver, "[trans] failed to create signature request for `%s': %s",
			pkgver, strerror(rv));
		return rv;
	}

	xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD, 0, pkgver,
		"Downloading `%s' signature (from `%s')...", pkgver, repoloc);

	if ((rv = xbps_fetch_file(xhp, buf, NULL)) == -1) {
		rv = fetchLastErrCode ? fetchLastErrCode : errno;
		fetchstr = xbps_fetch_error_string();
		xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL, rv,
			pkgver, "[trans] failed to download `%s' signature from `%s': %s",
			pkgver, repoloc, fetchstr ? fetchstr : strerror(rv));
		return rv;
	}
	rv = 0;

	*sigsuffix = '\0';

	xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD, 0, pkgver,
		"Downloading `%s' package (from `%s')...", pkgver, repoloc);

	if ((rv = xbps_fetch_file_sha256(xhp, buf, NULL, digest.buffer,
	    sizeof digest.buffer)) == -1) {
		rv = fetchLastErrCode ? fetchLastErrCode : errno;
		fetchstr = xbps_fetch_error_string();
		xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL, rv,
			pkgver, "[trans] failed to download `%s' package from `%s': %s",
			pkgver, repoloc, fetchstr ? fetchstr : strerror(rv));
		return rv;
	}
	rv = 0;

	xbps_set_cb_state(xhp, XBPS_STATE_VERIFY, 0, pkgver,
		"%s: verifying RSA signature...", pkgver);

	if ((rv = xbps_file_sig_path(buf, sizeof buf, &sigsuffix, "%s/%s.%s.xbps.sig", xhp->cachedir, pkgver, arch))) {
		xbps_set_cb_state(xhp, XBPS_STATE_DOWNLOAD_FAIL, rv,
			pkgver, "[trans] failed to create signature path for `%s': %s",
			pkgver, strerror(rv));
		return rv;
	}

	if ((repo = xbps_rpool_get_repo(repoloc)) == NULL) {
		rv = errno;
		xbps_dbg_printf(xhp, "%s: failed to get repository "
			"%s: %s\n", pkgver, repoloc, strerror(errno));
		return rv;
	}

	/*
	 * If digest is not set, binary package was not downloaded,
	 * i.e. 304 not modified, verify by file instead.
	 */
	/* FIXME: this rejects hashes that start with 0 */
	if (digest.buffer[0]) {
		*sigsuffix = '\0';
		if (!xbps_verify_file_signature(repo, buf)) {
			rv = EPERM;
			/* remove binpkg */
			(void)remove(buf);
			/* remove signature */
			*sigsuffix = '.';
			(void)remove(buf);
		}
	} else {
		if (!xbps_verify_signature(repo, buf, &digest)) {
			rv = EPERM;
			/* remove signature */
			(void)remove(buf);
			/* remove binpkg */
			*sigsuffix = '\0';
			(void)remove(buf);
		}
	}

	if (rv == EPERM) {
		xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
			"%s: the RSA signature is not valid!", pkgver);
		xbps_set_cb_state(xhp, XBPS_STATE_VERIFY_FAIL, rv, pkgver,
			"%s: removed pkg archive and its signature.", pkgver);
	}

	return rv;
}

int
xbps_transaction_fetch(struct xbps_handle *xhp, xbps_object_iterator_t iter)
{
	xbps_array_t fetch = NULL, verify = NULL;
	xbps_object_t obj;
	xbps_trans_type_t ttype;
	const char *repoloc;
	int rv = 0;
	unsigned int i, n;

	xbps_object_iterator_reset(iter);

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		ttype = xbps_transaction_pkg_type(obj);
		if (ttype == XBPS_TRANS_REMOVE || ttype == XBPS_TRANS_HOLD ||
		    ttype == XBPS_TRANS_CONFIGURE) {
			continue;
		}
		xbps_dictionary_get_cstring_nocopy(obj, "repository", &repoloc);

		/*
		 * Download binary package and signature if either one
		 * of them don't exist.
		 */
		if (xbps_repository_is_remote(repoloc) &&
		    !xbps_remote_binpkg_exists(xhp, obj)) {
			if (!fetch && !(fetch = xbps_array_create())) {
				rv = errno;
				goto out;
			}
			xbps_array_add(fetch, obj);
			continue;
		}

		/*
		 * Verify binary package from local repository or cache.
		 */
		if (!verify && !(verify = xbps_array_create())) {
			rv = errno;
			goto out;
		}
		xbps_array_add(verify, obj);
	}
	xbps_object_iterator_reset(iter);

	/*
	 * Download binary packages (if they come from a remote repository)
	 * and don't exist already.
	 */
	n = xbps_array_count(fetch);
	if (n) {
		xbps_set_cb_state(xhp, XBPS_STATE_TRANS_DOWNLOAD, 0, NULL, NULL);
		xbps_dbg_printf(xhp, "[trans] downloading %d packages.\n", n);
	}
	for (i = 0; i < n; i++) {
		if ((rv = download_binpkg(xhp, xbps_array_get(fetch, i))) != 0) {
			xbps_dbg_printf(xhp, "[trans] failed to download binpkgs: "
				"%s\n", strerror(rv));
			goto out;
		}
	}

	/*
	 * Check binary package integrity.
	 */
	n = xbps_array_count(verify);
	if (n) {
		xbps_set_cb_state(xhp, XBPS_STATE_TRANS_VERIFY, 0, NULL, NULL);
		xbps_dbg_printf(xhp, "[trans] verifying %d packages.\n", n);
	}
	for (i = 0; i < n; i++) {
		if ((rv = verify_binpkg(xhp, xbps_array_get(verify, i))) != 0) {
			xbps_dbg_printf(xhp, "[trans] failed to check binpkgs: "
				"%s\n", strerror(rv));
			goto out;
		}
	}

out:
	if (fetch)
		xbps_object_release(fetch);
	if (verify)
		xbps_object_release(verify);
	return rv;
}
