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
rsa_verify_buf(struct xbps_repo *repo, xbps_data_t pubkey,
		unsigned char *sig, unsigned int siglen,
		unsigned char *buf, unsigned int buflen)
{
	SHA256_CTX context;
	BIO *bio;
	RSA *rsa;
	unsigned char sha256[SHA256_DIGEST_LENGTH];
	int rv;

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

	SHA256_Init(&context);
	SHA256_Update(&context, buf, buflen);
	SHA256_Final(sha256, &context);

	rv = RSA_verify(NID_sha1, sha256, sizeof(sha256), sig, siglen, rsa);
	RSA_free(rsa);
	BIO_free(bio);
	ERR_free_strings();

	return rv ? true : false;
}

bool
xbps_verify_file_signature(struct xbps_repo *repo, const char *fname)
{
	xbps_dictionary_t repokeyd = NULL;
	xbps_data_t pubkey;
	char *hexfp = NULL;
	unsigned char *buf = NULL, *sig_buf = NULL;
	size_t buflen, filelen, sigbuflen, sigfilelen;
	char *rkeyfile = NULL, *sig = NULL;
	bool val = false;

	if (!xbps_dictionary_count(repo->idxmeta)) {
		xbps_dbg_printf(repo->xhp, "%s: unsigned repository\n", repo->uri);
		return false;
	}
	hexfp = xbps_pubkey2fp(repo->xhp,
	    xbps_dictionary_get(repo->idxmeta, "public-key"));
	if (hexfp == NULL) {
		xbps_dbg_printf(repo->xhp, "%s: incomplete signed repo, missing hexfp obj\n", repo->uri);
		return false;
	}
	/*
	 * Prepare repository RSA public key to verify fname signature.
	 */
	rkeyfile = xbps_xasprintf("%s/keys/%s.plist", repo->xhp->metadir, hexfp);
	repokeyd = xbps_plist_dictionary_from_file(repo->xhp, rkeyfile);
	if (xbps_object_type(repokeyd) != XBPS_TYPE_DICTIONARY) {
		xbps_dbg_printf(repo->xhp, "cannot read rkey data at %s: %s\n",
		    rkeyfile, strerror(errno));
		goto out;
	}

	pubkey = xbps_dictionary_get(repokeyd, "public-key");
	if (xbps_object_type(pubkey) != XBPS_TYPE_DATA)
		goto out;

	/*
	 * Prepare fname and signature data buffers.
	 */
	if (!xbps_mmap_file(fname, (void *)&buf, &buflen, &filelen)) {
		xbps_dbg_printf(repo->xhp, "can't open file %s: %s\n", fname, strerror(errno));
		goto out;
	}
	sig = xbps_xasprintf("%s.sig", fname);
	if (!xbps_mmap_file(sig, (void *)&sig_buf, &sigbuflen, &sigfilelen)) {
		xbps_dbg_printf(repo->xhp, "can't open signature file %s: %s\n", sig, strerror(errno));
		goto out;
	}
	/*
	 * Verify fname RSA signature.
	 */
	if (rsa_verify_buf(repo, pubkey, sig_buf, sigfilelen, buf, filelen))
		val = true;

out:
	if (hexfp)
		free(hexfp);
	if (rkeyfile)
		free(rkeyfile);
	if (buf)
		(void)munmap(buf, buflen);
	if (sig_buf)
		(void)munmap(sig_buf, sigbuflen);
	if (sig)
		free(sig);
	if (repokeyd)
		xbps_object_release(repokeyd);

	return val;
}
