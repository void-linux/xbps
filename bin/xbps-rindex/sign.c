/*-
 * Copyright (c) 2013-2014 Juan Romero Pardines.
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
	char *buf = NULL;
	int len;

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
	len = BIO_read(bp, buf, 8191);
	BIO_free(bp);
	ERR_free_strings();
	buf[len] = '\0';

	return buf;
}

static bool
rsa_sign_buf(RSA *rsa, const char *buf, unsigned int buflen,
	 unsigned char **sigret, unsigned int *siglen)
{
	SHA256_CTX context;
	unsigned char sha256[SHA256_DIGEST_LENGTH];

	SHA256_Init(&context);
	SHA256_Update(&context, buf, buflen);
	SHA256_Final(sha256, &context);

	*sigret = calloc(1, RSA_size(rsa) + 1);
	if (!RSA_sign(NID_sha1, sha256, sizeof(sha256),
				*sigret, siglen, rsa)) {
		free(*sigret);
		return false;
	}
	return true;
}

int
sign_repo(struct xbps_handle *xhp, const char *repodir,
	const char *privkey, const char *signedby)
{
	struct stat st;
	struct xbps_repo *repo;
	xbps_dictionary_t pkgd, meta = NULL;
	xbps_data_t data = NULL, rpubkey = NULL;
	xbps_object_iterator_t iter = NULL;
	xbps_object_t obj;
	RSA *rsa = NULL;
	unsigned char *sig;
	unsigned int siglen;
	uint16_t rpubkeysize, pubkeysize;
	const char *arch, *pkgver, *rsignedby = NULL;
	char *binpkg = NULL, *binpkg_sig = NULL, *buf = NULL, *defprivkey = NULL;
	int binpkg_fd, binpkg_sig_fd, rv = 0;
	bool flush = false;

	if (signedby == NULL) {
		fprintf(stderr, "--signedby unset! cannot sign repository\n");
		return -1;
	}

	/*
	 * Check that repository index exists and not empty, otherwise bail out.
	 */
	repo = xbps_repo_open(xhp, repodir, true);
	if (repo == NULL) {
		rv = errno;
		fprintf(stderr, "%s: cannot read repository data: %s\n",
		    _XBPS_RINDEX, strerror(errno));
		goto out;
	}
	if (xbps_dictionary_count(repo->idx) == 0) {
		fprintf(stderr, "%s: invalid repository, existing!\n", _XBPS_RINDEX);
		rv = EINVAL;
		goto out;
	}
	/*
	 * If privkey not set, default to ~/.ssh/id_rsa.
	 */
	if (privkey == NULL)
		defprivkey = xbps_xasprintf("%s/.ssh/id_rsa", getenv("HOME"));
	else
		defprivkey = strdup(privkey);

	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_ciphers();
	OpenSSL_add_all_digests();

	if ((rsa = load_rsa_privkey(defprivkey)) == NULL) {
		fprintf(stderr, "%s: failed to read the RSA privkey\n", _XBPS_RINDEX);
		rv = EINVAL;
		goto out;
	}
	/*
	 * Iterate over the idx dictionary and then sign all binary
	 * packages in this repository.
	 */
	iter = xbps_dictionary_iterator(repo->idx);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		pkgd = xbps_dictionary_get_keysym(repo->idx, obj);
		xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch);
		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

		binpkg = xbps_xasprintf("%s/%s.%s.xbps", repodir, pkgver, arch);
		binpkg_sig = xbps_xasprintf("%s.sig", binpkg);
		/*
		 * Skip pkg if file signature exists
		 */
		if ((binpkg_sig_fd = access(binpkg_sig, R_OK)) == 0) {
			if (xhp->flags & XBPS_FLAG_VERBOSE)
				fprintf(stderr, "skipping %s, file signature found.\n", pkgver);
			free(binpkg);
			free(binpkg_sig);
			close(binpkg_sig_fd);
			continue;
		}
		/*
		 * Generate pkg file signature.
		 */
		if ((binpkg_fd = open(binpkg, O_RDONLY)) == -1) {
			fprintf(stderr, "cannot read %s: %s\n", binpkg, strerror(errno));
			free(binpkg);
			free(binpkg_sig);
			continue;
		}
		fstat(binpkg_fd, &st);
		buf = malloc(st.st_size);
		assert(buf);
		if (read(binpkg_fd, buf, st.st_size) != st.st_size) {
			fprintf(stderr, "failed to read %s: %s\n", binpkg, strerror(errno));
			close(binpkg_fd);
			free(buf);
			free(binpkg);
			free(binpkg_sig);
			continue;
		}
		close(binpkg_fd);
		if (!rsa_sign_buf(rsa, buf, st.st_size, &sig, &siglen)) {
			fprintf(stderr, "failed to sign %s: %s\n", binpkg, strerror(errno));
			free(buf);
			free(binpkg);
			free(binpkg_sig);
			continue;
		}
		free(buf);
		free(binpkg);
		/*
		 * Write pkg file signature.
		 */
		binpkg_sig_fd = creat(binpkg_sig, 0644);
		if (binpkg_sig_fd == -1) {
			fprintf(stderr, "failed to create %s: %s\n", binpkg_sig, strerror(errno));
			free(sig);
			free(binpkg_sig);
			continue;
		}
		if (write(binpkg_sig_fd, sig, siglen) != (ssize_t)siglen) {
			fprintf(stderr, "failed to write %s: %s\n", binpkg_sig, strerror(errno));
			free(sig);
			free(binpkg_sig);
			close(binpkg_sig_fd);
			continue;
		}
		free(sig);
		free(binpkg_sig);
		close(binpkg_sig_fd);
		printf("signed successfully %s\n", pkgver);
	}
	xbps_object_iterator_release(iter);
	/*
	 * Check if repository index-meta contains changes compared to its
	 * current state.
	 */
	if ((buf = pubkey_from_privkey(rsa)) == NULL) {
		rv = EINVAL;
		goto out;
	}
	meta = xbps_dictionary_create();

	data = xbps_data_create_data(buf, strlen(buf));
	rpubkey = xbps_dictionary_get(repo->idxmeta, "public-key");
	if (!xbps_data_equals(rpubkey, data))
		flush = true;

	free(buf);

	pubkeysize = RSA_size(rsa) * 8;
	xbps_dictionary_get_uint16(repo->idxmeta, "public-key-size", &rpubkeysize);
	if (rpubkeysize != pubkeysize)
		flush = true;

	xbps_dictionary_get_cstring_nocopy(repo->idxmeta, "signature-by", &rsignedby);
	if (rsignedby == NULL || strcmp(rsignedby, signedby))
		flush = true;

	if (!flush)
		goto out;

	xbps_dictionary_set(meta, "public-key", data);
	xbps_dictionary_set_uint16(meta, "public-key-size", pubkeysize);
	xbps_dictionary_set_cstring_nocopy(meta, "signature-by", signedby);
	xbps_dictionary_set_cstring_nocopy(meta, "signature-type", "rsa");
	xbps_object_release(data);
	data = NULL;

	if (!repodata_flush(xhp, repodir, repo->idx, meta)) {
		fprintf(stderr, "failed to write repodata: %s\n", strerror(errno));
		goto out;
	}
	printf("Signed repository (%u package%s)\n",
	    xbps_dictionary_count(repo->idx),
	    xbps_dictionary_count(repo->idx) == 1 ? "" : "s");

out:
	if (defprivkey) {
		free(defprivkey);
	}
	if (rsa) {
		RSA_free(rsa);
		rsa = NULL;
	}
	if (repo) {
		xbps_repo_close(repo, true);
	}
	return rv ? -1 : 0;
}
