/*-
 * Copyright (c) 2013-2019 Juan Romero Pardines.
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
		xbps_error_printf("error writing public key: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		BIO_free(bp);
		return NULL;
	}
	/* XXX (xtraeme) 8192 should be always enough? */
	buf = malloc(8192);
	assert(buf);
	len = BIO_read(bp, buf, 8191);
	BIO_free(bp);
	buf[len] = '\0';

	return buf;
}

static bool
rsa_sign_file(RSA *rsa, const char *file,
	 unsigned char **sigret, unsigned int *siglen)
{
	unsigned char digest[XBPS_SHA256_DIGEST_SIZE];

	if (!xbps_file_sha256_raw(digest, sizeof digest, file))
		return false;

	if ((*sigret = calloc(1, RSA_size(rsa) + 1)) == NULL) {
		return false;
	}

	if (!RSA_sign(NID_sha256, digest, XBPS_SHA256_DIGEST_SIZE,
				*sigret, siglen, rsa)) {
		free(*sigret);
		return false;
	}

	return true;
}

static RSA *
load_rsa_key(const char *privkey)
{
	RSA *rsa = NULL;
	char *defprivkey;

	/*
	 * If privkey not set, default to ~/.ssh/id_rsa.
	 */
	if (privkey == NULL)
		defprivkey = xbps_xasprintf("%s/.ssh/id_rsa", getenv("HOME"));
	else
		defprivkey = strdup(privkey);

	if ((rsa = load_rsa_privkey(defprivkey)) == NULL) {
		xbps_error_printf("%s: failed to read the RSA privkey\n", _XBPS_RINDEX);
		exit(EXIT_FAILURE);
	}
	free(defprivkey);
	defprivkey = NULL;

	return rsa;
}

static void
ssl_init(void)
{
	SSL_load_error_strings();
	SSL_library_init();
}

int
sign_repo(struct xbps_handle *xhp, const char *repodir,
	const char *privkey, const char *signedby, const char *compression)
{
	struct xbps_repo *repo = NULL;
	xbps_dictionary_t meta = NULL;
	xbps_data_t data = NULL, rpubkey = NULL;
	RSA *rsa = NULL;
	uint16_t rpubkeysize, pubkeysize;
	const char *rsignedby = NULL;
	char *buf = NULL, *rlockfname = NULL;
	int rlockfd = -1, rv = 0;
	bool flush_failed = false, flush = false;

	if (signedby == NULL) {
		xbps_error_printf("--signedby unset! cannot initialize signed repository\n");
		return -1;
	}

	/*
	 * Check that repository index exists and not empty, otherwise bail out.
	 */
	repo = xbps_repo_open(xhp, repodir);
	if (repo == NULL) {
		rv = errno;
		xbps_error_printf("%s: cannot read repository data: %s\n",
		    _XBPS_RINDEX, strerror(errno));
		goto out;
	}
	if (xbps_dictionary_count(repo->idx) == 0) {
		xbps_error_printf("%s: invalid repository, exiting!\n", _XBPS_RINDEX);
		rv = EINVAL;
		goto out;
	}

	ssl_init();

	rsa = load_rsa_key(privkey);
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

	/* lock repository to write repodata file */
	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		rv = errno;
		xbps_error_printf("%s: cannot lock repository: %s\n",
		    _XBPS_RINDEX, strerror(errno));
		goto out;
	}
	flush_failed = repodata_flush(xhp, repodir, "repodata", repo->idx, meta, compression);
	xbps_repo_unlock(rlockfd, rlockfname);
	if (!flush_failed) {
		xbps_error_printf("failed to write repodata: %s\n", strerror(errno));
		goto out;
	}
	printf("Initialized signed repository (%u package%s)\n",
	    xbps_dictionary_count(repo->idx),
	    xbps_dictionary_count(repo->idx) == 1 ? "" : "s");

out:
	if (rsa) {
		RSA_free(rsa);
		rsa = NULL;
	}
	if (repo)
		xbps_repo_release(repo);

	return rv ? -1 : 0;
}

static int
sign_pkg(struct xbps_handle *xhp, const char *binpkg, const char *privkey, bool force)
{
	RSA *rsa = NULL;
	unsigned char *sig = NULL;
	unsigned int siglen = 0;
	char *sigfile = NULL;
	int rv = 0, sigfile_fd = -1;

	sigfile = xbps_xasprintf("%s.sig2", binpkg);
	/*
	 * Skip pkg if file signature exists
	 */
	if (!force && ((sigfile_fd = access(sigfile, R_OK)) == 0)) {
		if (xhp->flags & XBPS_FLAG_VERBOSE)
			fprintf(stderr, "skipping %s, file signature found.\n", binpkg);

		sigfile_fd = -1;
		goto out;
	}
	/*
	 * Generate pkg file signature.
	 */
	rsa = load_rsa_key(privkey);
	if (!rsa_sign_file(rsa, binpkg, &sig, &siglen)) {
		xbps_error_printf("failed to sign %s: %s\n", binpkg, strerror(errno));
		rv = EINVAL;
		goto out;
	}
	/*
	 * Write pkg file signature.
	 */
	if (force)
		sigfile_fd = open(sigfile, O_WRONLY|O_TRUNC, 0644);
	else
		sigfile_fd = creat(sigfile, 0644);

	if (sigfile_fd == -1) {
		xbps_error_printf("failed to create %s: %s\n", sigfile, strerror(errno));
		rv = EINVAL;
		free(sig);
		goto out;
	}
	if (write(sigfile_fd, sig, siglen) != (ssize_t)siglen) {
		xbps_error_printf("failed to write %s: %s\n", sigfile, strerror(errno));
		rv = EINVAL;
		free(sig);
		goto out;
	}
	free(sig);
	printf("signed successfully %s\n", binpkg);

out:
	if (rsa) {
		RSA_free(rsa);
		rsa = NULL;
	}
	if (sigfile)
		free(sigfile);
	if (sigfile_fd != -1)
		close(sigfile_fd);

	return rv;
}

int
sign_pkgs(struct xbps_handle *xhp, int args, int argmax, char **argv,
		const char *privkey, bool force)
{
	ssl_init();
	/*
	 * Process all packages specified in argv.
	 */
	for (int i = args; i < argmax; i++) {
		int rv;
		const char *binpkg = argv[i];
		rv = sign_pkg(xhp, binpkg, privkey, force);
		if (rv != 0)
			return rv;
	}
	return 0;
}
