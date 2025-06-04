/*-
 * Copyright (c) 2013-2014 Juan Romero Pardines.
 * Copyright (c) 2025      Duncan Overbruck <mail@duncano.de>.
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

#include <sys/stat.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

#include "xbps_api_impl.h"

enum pubkey_type {
	PUBKEY_RSA,
};

struct pubkey {
	enum pubkey_type type;
};

struct pubkey_rsa {
	struct pubkey common;
	EVP_PKEY *pkey;
};

struct pubkey HIDDEN *
pubkey_load_rsa(xbps_data_t data)
{
	EVP_PKEY *pkey;
	BIO *bio;
	int r;
	struct pubkey_rsa *pubkey;

	pubkey = calloc(1, sizeof(*pubkey));
	if (!pubkey) {
		r = -errno;
		xbps_error_printf("failed to load pubkey: %s\n", strerror(-r));
		errno = -r;
		return NULL;
	}

	bio =
	    BIO_new_mem_buf(xbps_data_data_nocopy(data), xbps_data_size(data));
	if (!bio) {
		r = -errno;
		xbps_error_printf(
		    "failed to load pubkey: BIO_new_mem_buf: %s\n",
		    strerror(-r));
		free(pubkey);
		errno = -r;
		return NULL;
	}
	pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
	if (!pkey) {
		r = -EINVAL;
		xbps_error_printf(
		    "failed to load pubkey: reading PEM failed\n");
		BIO_free(bio);
		free(pubkey);
		errno = -r;
		return NULL;
	}
	if (EVP_PKEY_base_id(pkey) != EVP_PKEY_RSA) {
		r = -EINVAL;
		xbps_error_printf(
		    "failed to load pubkey: unsupported key type\n");
		BIO_free(bio);
		EVP_PKEY_free(pkey);
		free(pubkey);
		errno = -r;
		return NULL;
	}

	BIO_free(bio);
	pubkey->common.type = PUBKEY_RSA;
	pubkey->pkey = pkey;
	return (struct pubkey *)pubkey;
}

void HIDDEN
pubkey_free(struct pubkey *pubkey)
{
	struct pubkey_rsa *rsa;
	if (!pubkey)
		return;
	switch (pubkey->type) {
	case PUBKEY_RSA:
		rsa = (struct pubkey_rsa *)pubkey;
		EVP_PKEY_free(rsa->pkey);
		free(rsa);
		break;
	}
}

static int
pubkey_rsa_verify(const struct pubkey_rsa *pubkey, const unsigned char *sig,
    unsigned int siglen, const unsigned char *md, size_t mdlen)
{
	EVP_PKEY_CTX *ctx;
	int r;

	ctx = EVP_PKEY_CTX_new(pubkey->pkey, NULL);
	if (!ctx) {
		xbps_error_printf(
		    "failed to verify signature: EVP_PKEY_CTX_new: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		return errno ? -errno : -ENOTSUP;
	}
	if (EVP_PKEY_verify_init(ctx) <= 0) {
		xbps_error_printf(
		    "failed to verify signature: EVP_PKEY_verify_init: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}
	if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_PADDING) <= 0) {
		xbps_error_printf("failed to verify signature: "
		                  "EVP_PKEY_CTX_set_rsa_padding: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}
	if (EVP_PKEY_CTX_set_signature_md(ctx, EVP_sha256()) <= 0) {
		xbps_error_printf("failed to verify signature: "
		                  "EVP_PKEY_CTX_set_signature_md: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}
	r = EVP_PKEY_verify(ctx, sig, siglen, md, mdlen);
	if (r < 0) {
		xbps_error_printf(
		    "failed to verify signature: EVP_PKEY_verify: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		goto err;
	}

	EVP_PKEY_CTX_free(ctx);
	return r;
err:
	EVP_PKEY_CTX_free(ctx);
	return -EINVAL;
}

int
pubkey_verify(const struct pubkey *pubkey, const unsigned char *sig,
    unsigned int siglen, const unsigned char *md, size_t mdlen)
{
	switch (pubkey->type) {
	case PUBKEY_RSA:
		return pubkey_rsa_verify(
		    (const struct pubkey_rsa *)pubkey, sig, siglen, md, mdlen);
	}
	abort();
}
