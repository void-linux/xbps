/*-
 * Copyright (c) 2013 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

#include "xbps_api_impl.h"

int
xbps_repo_key_import(struct xbps_repo *repo)
{
	xbps_dictionary_t repokeyd = NULL;
	char *p, *dbkeyd, *rkeyfile = NULL;
	int import, rv = 0;

	assert(repo);
	/*
	 * Ignore local repositories.
	 */
	if (!xbps_repository_is_remote(repo->uri))
		return 0;
	/*
	 * If repository does not have required metadata plist, ignore it.
	 */
	if (repo->signature == NULL && repo->pubkey == NULL) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' unsigned repository!\n", repo->uri);
		return 0;
	}
	/*
	 * Check the repository provides a working public-key data object.
	 */
	repo->is_signed = true;
	if (repo->hexfp == NULL) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s': invalid hex fingerprint: %s\n",
		    repo->uri, strerror(errno));
		rv = EINVAL;
		goto out;
	}
	/*
	 * Check if the public key is alredy stored.
	 */
	rkeyfile = xbps_xasprintf("%s/keys/%s.plist",
	    repo->xhp->metadir, repo->hexfp);
	repokeyd = xbps_dictionary_internalize_from_zfile(rkeyfile);
	if (xbps_object_type(repokeyd) == XBPS_TYPE_DICTIONARY) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' public key already stored.\n", repo->uri);
		goto out;
	}
	/*
	 * Notify the client and take appropiate action to import
	 * the repository public key. Pass back the public key openssh fingerprint
	 * to the client.
	 */
	import = xbps_set_cb_state(repo->xhp, XBPS_STATE_REPO_KEY_IMPORT, 0,
			repo->hexfp, "`%s' repository has been RSA signed by \"%s\"",
			repo->uri, repo->signedby);
	if (import <= 0) {
		rv = EAGAIN;
		goto out;
	}

	p = strdup(rkeyfile);
	dbkeyd = dirname(p);
	assert(dbkeyd);
	if (access(dbkeyd, R_OK|W_OK) == -1) {
		if (errno == ENOENT) {
			xbps_mkpath(dbkeyd, 0755);
		} else {
			rv = errno;
			xbps_dbg_printf(repo->xhp,
			    "[repo] `%s' cannot create %s: %s\n",
			    repo->uri, dbkeyd, strerror(errno));
			free(p);
			goto out;
		}
	}
	free(p);

	repokeyd = xbps_dictionary_create();
	xbps_dictionary_set(repokeyd, "public-key", repo->pubkey);
	xbps_dictionary_set_uint16(repokeyd, "public-key-size", repo->pubkey_size);
	xbps_dictionary_set_cstring_nocopy(repokeyd, "signature-by", repo->signedby);

	if (!xbps_dictionary_externalize_to_zfile(repokeyd, rkeyfile)) {
		rv = errno;
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' failed to externalize %s: %s\n",
		    repo->uri, rkeyfile, strerror(rv));
	}

out:
	if (repokeyd)
		xbps_object_release(repokeyd);
	if (rkeyfile)
		free(rkeyfile);
	return rv;
}

static int
rsa_verify_buf(struct xbps_repo *repo, xbps_data_t sigdata,
		xbps_data_t pubkey, const char *buf)
{
	SHA256_CTX context;
	BIO *bio;
	RSA *rsa;
	unsigned char sha256[SHA256_DIGEST_LENGTH];
	int rv = 0;

	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();

	bio = BIO_new_mem_buf(__UNCONST(xbps_data_data_nocopy(pubkey)),
			xbps_data_size(pubkey));
	assert(bio);

	rsa = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
	if (rsa == NULL) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' error reading public key: %s\n",
		    repo->uri, ERR_error_string(ERR_get_error(), NULL));
		return EINVAL;
	}

	SHA256_Init(&context);
	SHA256_Update(&context, buf, strlen(buf));
	SHA256_Final(sha256, &context);

	if (RSA_verify(NID_sha1, sha256, sizeof(sha256),
			xbps_data_data_nocopy(sigdata),
			xbps_data_size(sigdata), rsa) == 0) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' failed to verify signature: %s\n",
		    repo->uri, ERR_error_string(ERR_get_error(), NULL));
		rv = EPERM;
	}
	RSA_free(rsa);
	BIO_free(bio);
	ERR_free_strings();

	return rv;
}

int HIDDEN
xbps_repo_key_verify(struct xbps_repo *repo)
{
	xbps_dictionary_t repokeyd;
	xbps_data_t xbps_pubkey;
	char *idx_xml, *rkeyfile;

	if (!repo->signature || !repo->hexfp)
		return EINVAL;

	rkeyfile = xbps_xasprintf("%s/keys/%s.plist",
	    repo->xhp->metadir, repo->hexfp);
	repokeyd = xbps_dictionary_internalize_from_zfile(rkeyfile);
	free(rkeyfile);
	if (xbps_object_type(repokeyd) != XBPS_TYPE_DICTIONARY)
		return EINVAL;

	xbps_pubkey = xbps_dictionary_get(repokeyd, "public-key");
	if (xbps_object_type(xbps_pubkey) != XBPS_TYPE_DATA) {
		xbps_object_release(repokeyd);
		return EINVAL;
	}

	idx_xml = xbps_dictionary_externalize(repo->idx);
	if (idx_xml == NULL) {
		xbps_object_release(repokeyd);
		return EINVAL;
	}

	if (rsa_verify_buf(repo, repo->signature, xbps_pubkey, idx_xml) == 0)
		repo->is_verified = true;

	free(idx_xml);
	xbps_object_release(repokeyd);

	return repo->is_verified ? 0 : EPERM;
}
