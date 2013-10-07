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

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

#include "xbps_api_impl.h"

int HIDDEN
xbps_repo_key_import(struct xbps_repo *repo)
{
	xbps_dictionary_t repokeyd, newmetad = NULL;
	xbps_data_t rpubkey;
	const char *signedby;
	unsigned char *fp;
	char *rkeypath = NULL;
	int import, rv = 0;

	assert(repo);
	/*
	 * If repository does not have required metadata plist, ignore it.
	 */
	if (xbps_dictionary_count(repo->meta) == 0) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' unsigned repository!\n", repo->uri);
		return 0;
	}
	/*
	 * Check if the public key has been stored for this repository.
	 */
	if (repo->xhp->repokeys == NULL) {
		rkeypath = xbps_xasprintf("%s/%s", repo->xhp->metadir, XBPS_REPOKEYS);
		repo->xhp->repokeys = xbps_dictionary_internalize_from_file(rkeypath);
		if (xbps_object_type(repo->xhp->repokeys) != XBPS_TYPE_DICTIONARY)
			repo->xhp->repokeys = xbps_dictionary_create();
	}
	repokeyd = xbps_dictionary_get(repo->xhp->repokeys, repo->uri);
	if (xbps_object_type(repokeyd) == XBPS_TYPE_DICTIONARY) {
		if (xbps_dictionary_get(repokeyd, "public-key")) {
			xbps_dbg_printf(repo->xhp,
			    "[repo] `%s' public key already stored.\n",
			    repo->uri);
			goto out;
		}
	}
	/*
	 * Check the repository provides a working public-key data object.
	 */
	rpubkey = xbps_dictionary_get(repo->meta, "public-key");
	if (xbps_object_type(rpubkey) != XBPS_TYPE_DATA) {
		rv = EINVAL;
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' invalid public-key object!\n", repo->uri);
		goto out;
	}
	repo->is_signed = true;
	/*
	 * Notify the client and take appropiate action to import
	 * the repository public key. Pass back the public key openssh fingerprint
	 * to the client.
	 */
	fp = xbps_pubkey2fp(repo->xhp, rpubkey);
	xbps_dictionary_get_cstring_nocopy(repo->meta, "signature-by", &signedby);
	import = xbps_set_cb_state(repo->xhp, XBPS_STATE_REPO_KEY_IMPORT,
			0, (const char *)fp,
			"This repository is RSA signed by \"%s\"",
			signedby);
	free(fp);
	if (import <= 0) {
		rv = EAGAIN;
		goto out;
	}
	/*
	 * Add the meta dictionary into XBPS_REPOKEYS and externalize it.
	 */
	newmetad = xbps_dictionary_copy_mutable(repo->meta);
	xbps_dictionary_remove(newmetad, "signature");
	xbps_dictionary_set(repo->xhp->repokeys, repo->uri, newmetad);

	if (access(repo->xhp->metadir, R_OK|W_OK) == -1) {
		if (errno == ENOENT) {
			xbps_mkpath(repo->xhp->metadir, 0755);
		} else {
			rv = errno;
			xbps_dbg_printf(repo->xhp,
			    "[repo] `%s' cannot create metadir: %s\n",
			    repo->uri, strerror(errno));
			goto out;
		}
	}
	if (!xbps_dictionary_externalize_to_file(repo->xhp->repokeys, rkeypath)) {
		rv = errno;
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' failed to externalize %s: %s\n",
		    repo->uri, XBPS_REPOKEYS, strerror(rv));
	}

out:
	if (newmetad)
		xbps_object_release(newmetad);
	if (rkeypath)
		free(rkeypath);
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
	xbps_data_t sigdata, pubkey;
	char *idx_xml;

	if (repo->xhp->repokeys == NULL)
		return ENOENT;

	repokeyd = xbps_dictionary_get(repo->xhp->repokeys, repo->uri);
	if (xbps_dictionary_count(repokeyd) == 0) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s': empty %s dictionary\n",
		    repo->uri, XBPS_REPOKEYS);
		return ENOENT;
	}

	idx_xml = xbps_dictionary_externalize(repo->idx);
	assert(idx_xml);

	sigdata = xbps_dictionary_get(repo->meta, "signature");
	assert(xbps_object_type(sigdata) == XBPS_TYPE_DATA);
	pubkey = xbps_dictionary_get(repokeyd, "public-key");
	assert(xbps_object_type(pubkey) == XBPS_TYPE_DATA);
	/* XXX ignore 'signature-type' for now */
	if (rsa_verify_buf(repo, sigdata, pubkey, idx_xml) == 0)
		repo->is_verified = true;

	free(idx_xml);

	return repo->is_verified ? 0 : EPERM;
}
