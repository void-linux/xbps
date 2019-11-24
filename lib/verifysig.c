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
rsa_verify_hash(struct xbps_repo *repo, xbps_data_t pubkey,
		unsigned char *sig, unsigned int siglen,
		unsigned char *sha256)
{
	BIO *bio;
	RSA *rsa;
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

	rv = RSA_verify(NID_sha1, sha256, SHA256_DIGEST_LENGTH, sig, siglen, rsa);
	RSA_free(rsa);
	BIO_free(bio);
	ERR_free_strings();

	return rv ? true : false;
}

bool
xbps_verify_digest_signature(struct xbps_repo *repo, xbps_dictionary_t idxmeta,
		unsigned char *sig_buf, size_t sigfilelen, unsigned char *digest)
{
	xbps_dictionary_t repokeyd = NULL;
	xbps_data_t pubkey;
	char *hexfp = NULL, *rkeyfile = NULL;
	bool val = false;

	if (!xbps_dictionary_count(idxmeta)) {
		xbps_dbg_printf(repo->xhp, "%s: unsigned repository\n", repo->uri);
		return false;
	}
	hexfp = xbps_pubkey2fp(repo->xhp,
	    xbps_dictionary_get(idxmeta, "public-key"));
	if (hexfp == NULL) {
		xbps_dbg_printf(repo->xhp, "%s: incomplete signed repo, missing hexfp obj\n", repo->uri);
		return false;
	}
	/*
	 * Prepare repository RSA public key to verify fname signature.
	 */
	/* XXX: xbps-rindex does not set rootdir, use cwd and fallback to defaults otherwise */
	rkeyfile = xbps_xasprintf("keys/%s.plist", hexfp);
	if (access(rkeyfile, R_OK) == -1) {
		free(rkeyfile);
		rkeyfile = xbps_xasprintf("%s/keys/%s.plist", repo->xhp->metadir, hexfp);
	}
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
	 * Verify fname RSA signature.
	 */
	if (rsa_verify_hash(repo, pubkey, sig_buf, sigfilelen, digest))
		val = true;

out:
	if (hexfp)
		free(hexfp);
	if (rkeyfile)
		free(rkeyfile);
	if (repokeyd)
		xbps_object_release(repokeyd);

	return val;
}

bool
xbps_verify_signature(struct xbps_repo *repo, const char *sigfile,
		unsigned char *digest)
{
	unsigned char *sig_buf = NULL;
	size_t sigbuflen, sigfilelen;
	bool result = false;

	if (xbps_mmap_file(sigfile, (void *)&sig_buf, &sigbuflen, &sigfilelen)) {
		result = xbps_verify_digest_signature(repo, repo->idxmeta, sig_buf, sigfilelen, digest);
	} else {
		xbps_dbg_printf(repo->xhp, "can't open signature file %s: %s\n",
		    sigfile, strerror(errno));
	}

	if (sig_buf)
		(void)munmap(sig_buf, sigbuflen);
	return result;
}

bool
xbps_verify_file_signature(struct xbps_repo *repo, const char *fname)
{
	char sig[PATH_MAX];
	unsigned char *digest = NULL;
	bool val = false;

	if (!(digest = xbps_file_hash_raw(fname))) {
		xbps_dbg_printf(repo->xhp, "can't open file %s: %s\n", fname, strerror(errno));
		return false;
	}

	snprintf(sig, sizeof sig, "%s.sig", fname);
	val = xbps_verify_signature(repo, sig, digest);

	free(digest);
	return val;
}
