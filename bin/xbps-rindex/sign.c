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
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

#include "defs.h"

static RSA *
load_rsa_privkey(const char *path)
{
	FILE *fp;
	RSA *rsa = NULL;
	const char *p;
	char *passphrase = NULL;

	if ((fp = fopen(path, "r")) == 0)
		return NULL;

	if ((rsa = RSA_new()) == NULL) {
		fclose(fp);
		return NULL;
	}

	p = getenv("XBPS_PASSPHRASE");
	if (p) {
		passphrase = strdup(p);
	}
	rsa = PEM_read_RSAPrivateKey(fp, 0, NULL, passphrase);
	if (passphrase) {
		free(passphrase);
		passphrase = NULL;
	}
	fclose(fp);
	return rsa;
}

static char *
pubkey_from_privkey(RSA *rsa)
{
	BIO *bp;
	char *buf;

	bp = BIO_new(BIO_s_mem());
	assert(bp);

	if (!PEM_write_bio_RSA_PUBKEY(bp, rsa)) {
		fprintf(stderr, "error writing public key: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		BIO_free(bp);
		return NULL;
	}
	/* XXX (xtraeme) 8192 should be always enough? */
	buf = malloc(8192);
	assert(buf);
	BIO_read(bp, buf, 8192);
	BIO_free(bp);
	ERR_free_strings();

	return buf;
}

static RSA *
rsa_sign_buf(const char *privkey, const char *buf,
	 unsigned char **sigret, unsigned int *siglen)
{
	SHA256_CTX context;
	RSA *rsa;
	unsigned char sha256[SHA256_DIGEST_LENGTH];

	ERR_load_crypto_strings();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	rsa = load_rsa_privkey(privkey);
	if (rsa == NULL) {
		fprintf(stderr, "can't load private key from %s\n", privkey);
		return NULL;
	}

	SHA256_Init(&context);
	SHA256_Update(&context, buf, strlen(buf));
	SHA256_Final(sha256, &context);

	*sigret = calloc(1, RSA_size(rsa) + 1);
	if (RSA_sign(NID_sha1, sha256, sizeof(sha256),
				*sigret, siglen, rsa) == 0) {
		fprintf(stderr, "%s: %s\n", privkey,
		    ERR_error_string(ERR_get_error(), NULL));
		return NULL;
	}
	return rsa;
}

int
sign_repo(struct xbps_handle *xhp, const char *repodir,
	const char *privkey, const char *signedby)
{
	RSA *rsa = NULL;
	struct xbps_repo *repo;
	xbps_dictionary_t idx, idxfiles, meta = NULL;
	xbps_data_t data;
	unsigned int siglen;
	unsigned char *sig;
	char *buf = NULL, *xml = NULL, *defprivkey = NULL;
	int rv = -1;

	if (signedby == NULL) {
		fprintf(stderr, "--signedby unset! cannot sign repository\n");
		return -1;
	}
	/*
	 * Check that repository index exists and not empty, otherwise bail out.
	 */
	repo = xbps_repo_open(xhp, repodir);
	if (repo == NULL) {
		fprintf(stderr, "cannot read repository data: %s\n", strerror(errno));
		goto out;
	}
	if (xbps_dictionary_count(repo->idx) == 0) {
		fprintf(stderr, "invalid number of objects in repository index!\n");
		xbps_repo_close(repo);
		return -1;
	}
	xbps_repo_open_idxfiles(repo);
	idx = xbps_dictionary_copy(repo->idx);
	idxfiles = xbps_dictionary_copy(repo->idxfiles);
	xbps_repo_close(repo);

	/*
	 * Externalize the index and then sign it.
	 */
	xml = xbps_dictionary_externalize(idx);
	if (xml == NULL) {
		fprintf(stderr, "failed to externalize repository index: %s\n", strerror(errno));
		goto out;
	}
	/*
	 * If privkey not set, default to ~/.ssh/id_rsa.
	 */
	if (privkey == NULL)
		defprivkey = xbps_xasprintf("%s/.ssh/id_rsa", getenv("HOME"));
	else
		defprivkey = strdup(privkey);

	rsa = rsa_sign_buf(defprivkey, xml, &sig, &siglen);
	if (rsa == NULL) {
		free(xml);
		goto out;
	}
	/*
	 * Prepare the XBPS_REPOIDX_META for our repository data.
	 */
	meta = xbps_dictionary_create();
	xbps_dictionary_set_cstring_nocopy(meta, "signature-by", signedby);
	xbps_dictionary_set_cstring_nocopy(meta, "signature-type", "rsa");
	/*
	 * If the signature in repo has not changed do not generate the
	 * repodata file again.
	 */
	data = xbps_data_create_data_nocopy(sig, siglen);
	if (xbps_data_equals_data(data, sig, siglen)) {
		fprintf(stderr, "Not signing again, matched signature found.\n");
		rv = 0;
		goto out;
	}
	xbps_dictionary_set(meta, "signature", data);

	buf = pubkey_from_privkey(rsa);
	assert(buf);
	data = xbps_data_create_data_nocopy(buf, strlen(buf));
	xbps_dictionary_set(meta, "public-key", data);
	xbps_dictionary_set_uint16(meta, "public-key-size", RSA_size(rsa) * 8);

	/*
	 * and finally write our repodata file!
	 */
	if (!repodata_flush(xhp, repodir, idx, idxfiles, meta)) {
		fprintf(stderr, "failed to write repodata: %s\n", strerror(errno));
		goto out;
	}

	rv = 0;

out:
	if (xml != NULL)
		free(xml);
	if (buf != NULL)
		free(buf);
	if (rsa != NULL)
		RSA_free(rsa);

	return rv;
}
