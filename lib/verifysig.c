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
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

#include "xbps_api_impl.h"

static bool
rsa_verify_hash(struct xbps_repo *repo,
		unsigned char *sig, unsigned int siglen,
		unsigned char *sha256)
{
	BIO *bio;
	RSA *rsa;
	int rv;
	xbps_data_t pubkey;

	if ((pubkey = xbps_repo_pubkey(repo)) == NULL) {
		xbps_error_printf("%s: failed to get public key: %s\n",
			repo->uri, strerror(errno));
		return false;
	}

	ERR_load_crypto_strings();
	SSL_load_error_strings();

	bio = BIO_new_mem_buf(__UNCONST(xbps_data_data_nocopy(pubkey)),
			xbps_data_size(pubkey));
	assert(bio);

	rsa = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL);
	if (rsa == NULL) {
		xbps_dbg_printf(repo->xhp, "`%s' error reading public key: %s\n",
		    repo->uri, ERR_error_string(ERR_get_error(), NULL));
		return false;
	}

	rv = RSA_verify(NID_sha1, sha256, SHA256_DIGEST_LENGTH, sig, siglen, rsa);
	RSA_free(rsa);
	BIO_free(bio);
	ERR_free_strings();

	return rv ? true : false;
}

bool
xbps_verify_signature(struct xbps_repo *repo, const char *sigfile,
		unsigned char *digest)
{
	unsigned char *sig_buf = NULL;
	size_t sigbuflen, sigfilelen;
	bool val = false;

	if (!xbps_mmap_file(sigfile, (void *)&sig_buf, &sigbuflen, &sigfilelen)) {
		xbps_dbg_printf(repo->xhp, "can't open signature file %s: %s\n",
		    sigfile, strerror(errno));
		goto out;
	}
	/*
	 * Verify fname RSA signature.
	 */
	if (rsa_verify_hash(repo, sig_buf, sigfilelen, digest))
		val = true;

out:
	if (sig_buf)
		(void)munmap(sig_buf, sigbuflen);

	return val;
}

bool
xbps_verify_file_signature(struct xbps_repo *repo, const char *fname)
{
	char sig[PATH_MAX];
	unsigned char digest[XBPS_SHA256_DIGEST_SIZE];
	bool val = false;

	if (!xbps_file_sha256_raw(digest, sizeof digest, fname)) {
		xbps_dbg_printf(repo->xhp, "can't open file %s: %s\n", fname, strerror(errno));
		return false;
	}

	snprintf(sig, sizeof sig, "%s.sig", fname);
	val = xbps_verify_signature(repo, sig, digest);

	return val;
}
