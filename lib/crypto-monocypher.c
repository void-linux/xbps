/*-
 * Copyright (c) 2023 Duncan Overbruck <mail@duncano.de>.
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
/*
 * Copyright (c) 2015-2018
 * Frank Denis <j at pureftpd dot org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <asm-generic/errno-base.h>
#ifdef HAVE_GETRANDOM
#include <sys/random.h>
#endif

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "external/monocypher.h"
#include "external/monocypher-ed25519.h"

#include "crypto-impl.h"

void
xbps_wipe_secret(void *secret, size_t size)
{
	crypto_wipe(secret, size);
}

#ifdef HAVE_GETRANDOM
int
randombytes_buf(void *buf, size_t buflen)
{
	assert(buflen > 0);
	while (buflen > 0) {
		size_t chunk = buflen > 256 ? 256 : buflen;
		ssize_t rd = getrandom(buf, chunk, 0);
		if (rd < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -errno;
		}
		buf = ((uint8_t *)buf) + rd;
		buflen -= rd;
	}
	return 0;
}
#endif

int
encrypt_key(struct xbps_seckey *seckey, const char *passphrase)
{
#if 0
	unsigned char stream[sizeof(seckey->keynum_sk)];
	unsigned long kdf_memlimit;
	unsigned long kdf_opslimit;

	randombytes_buf(seckey->kdf_salt, sizeof(seckey->kdf_salt));
	kdf_opslimit = crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_SENSITIVE;
	kdf_memlimit = crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_SENSITIVE;
	while (crypto_pwhash_scryptsalsa208sha256(stream, sizeof(stream),
	    passphrase, strlen(passphrase), seckey->kdf_salt, kdf_opslimit, kdf_memlimit)) {
		kdf_opslimit /= 2;
		kdf_memlimit /= 2;
		if (kdf_opslimit < crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_MIN ||
		    kdf_memlimit < crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_MIN)
			return -ENOMEM;
	}
	if (kdf_memlimit < crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_SENSITIVE)
		xbps_warn_printf("due to limited memory the KDF used less memory than the default\n");
	memcpy(seckey->kdf_alg, KDFALG, sizeof(seckey->kdf_alg));
	le64_store(seckey->kdf_opslimit_le, kdf_opslimit);
	le64_store(seckey->kdf_memlimit_le, kdf_memlimit);
	seckey_compute_chk(seckey->keynum_sk.chk, seckey);
	xor_buf((unsigned char *) (void *) &seckey->keynum_sk, stream,
	    sizeof(seckey->keynum_sk));
	crypto_wipe(&stream, sizeof(stream));
	return 0;
#else
	(void) seckey;
	(void) passphrase;
	return -ENOTSUP;
#endif
}

static void
seckey_compute_chk(unsigned char chk[CHK_HASH_BYTES], const struct xbps_seckey *seckey)
{
	crypto_blake2b_ctx ctx;

	crypto_blake2b_general_init(&ctx, CHK_HASH_BYTES, NULL, 0);
	crypto_blake2b_update(&ctx, seckey->sig_alg, sizeof(seckey->sig_alg));
	crypto_blake2b_update(&ctx, seckey->keynum_sk.keynum,
	    sizeof(seckey->keynum_sk.keynum));
	crypto_blake2b_update(&ctx, seckey->keynum_sk.sk, sizeof(seckey->keynum_sk.sk));
	crypto_blake2b_final(&ctx, chk);
}

int
decrypt_key(struct xbps_seckey *seckey, const char *passphrase)
{
#if 0
	unsigned char chk[CHK_HASH_BYTES];
	unsigned char stream[sizeof(seckey->keynum_sk)];
	if (crypto_pwhash_scryptsalsa208sha256(stream, sizeof(seckey->keynum_sk),
	    passphrase, strlen(passphrase), seckey->kdf_salt,
	    le64_load(seckey->kdf_opslimit_le),
	    le64_load(seckey->kdf_memlimit_le)) != 0)
		return -ENOMEM;
	xor_buf((unsigned char *) (void *) &seckey->keynum_sk, stream,
	    sizeof(seckey->keynum_sk));
	crypto_wipe(stream, sizeof(stream));
	seckey_compute_chk(chk, seckey);
	if (memcmp(chk, seckey->keynum_sk.chk, sizeof(seckey->keynum_sk.chk)) != 0)
		return -ERANGE;
	return 0;
#else
	(void) seckey;
	(void) passphrase;
	return -ENOTSUP;
#endif
}


int
xbps_generate_keypair(struct xbps_seckey *seckey, struct xbps_pubkey *pubkey)
{
	int r;
	r = randombytes_buf(seckey->keynum_sk.keynum, sizeof(seckey->keynum_sk.keynum));
	if (r < 0)
		return r;
	r = randombytes_buf(seckey->keynum_sk.sk, sizeof(seckey->keynum_sk.sk));
	if (r < 0)
		return r;
	crypto_ed25519_public_key(seckey->keynum_sk.pk, seckey->keynum_sk.sk);
	memcpy(seckey->sig_alg, SIGALG, sizeof(seckey->sig_alg));
	memcpy(seckey->kdf_alg, KDFNONE, sizeof(seckey->kdf_alg));
	memcpy(seckey->chk_alg, CHKALG, sizeof(seckey->chk_alg));
	seckey_compute_chk(seckey->keynum_sk.chk, seckey);
	memcpy(pubkey->keynum_pk.keynum, seckey->keynum_sk.keynum, sizeof(pubkey->keynum_pk.keynum));
	memcpy(pubkey->sig_alg, SIGALG, sizeof(pubkey->sig_alg));
	memcpy(pubkey->keynum_pk.pk, seckey->keynum_sk.pk, sizeof(pubkey->keynum_pk.pk));
	return 0;
}

static void
xbps_sig_sign(struct xbps_sig *sig, const struct xbps_seckey *seckey, const struct xbps_hash *hash)
{
	memcpy(sig->sig_alg, SIGALG_HASHED, sizeof(sig->sig_alg));
	memcpy(sig->keynum, seckey->keynum_sk.keynum, sizeof(sig->keynum));
	crypto_ed25519_sign(sig->sig, seckey->keynum_sk.sk, seckey->keynum_sk.pk,
	    hash->mem, sizeof(hash->mem));
}

int
xbps_minisig_sign(struct xbps_minisig *minisig, const struct xbps_seckey *seckey,
		const struct xbps_hash *hash)
{
	unsigned char sig_and_trusted_comment[TRUSTEDCOMMENTMAXBYTES + sizeof(minisig->sig.sig)];
	size_t trusted_comment_len;

	xbps_sig_sign(&minisig->sig, seckey, hash);

	trusted_comment_len = strlen(minisig->trusted_comment);
	memcpy(sig_and_trusted_comment, minisig->sig.sig, sizeof(minisig->sig.sig));
	memcpy(sig_and_trusted_comment + sizeof(minisig->sig.sig), minisig->trusted_comment,
	    trusted_comment_len);
	crypto_ed25519_sign(minisig->global_sig, seckey->keynum_sk.sk, seckey->keynum_sk.pk,
	    sig_and_trusted_comment, sizeof(minisig->sig.sig) + trusted_comment_len);

	return 0;
}

static int
xbps_sig_verify(const struct xbps_sig *sig, const struct xbps_pubkey *pubkey,
		const struct xbps_hash *hash)
{
	if (memcmp(sig->keynum, pubkey->keynum_pk.keynum, sizeof(sig->keynum)) != 0)
		return -EINVAL;
	if (crypto_ed25519_check(sig->sig, pubkey->keynum_pk.pk, hash->mem, sizeof(hash->mem)) != 0)
		return -ERANGE;
	return 0;
}

int
xbps_minisig_verify(const struct xbps_minisig *minisig, const struct xbps_pubkey *pubkey,
		const struct xbps_hash *hash)
{
	unsigned char sig_and_trusted_comment[TRUSTEDCOMMENTMAXBYTES + sizeof(minisig->sig.sig)];
	size_t trusted_comment_len;

	int r;
	r = xbps_sig_verify(&minisig->sig, pubkey, hash);
	if (r < 0)
		return r;

	trusted_comment_len = strlen(minisig->trusted_comment);
	memcpy(sig_and_trusted_comment, minisig->sig.sig, sizeof(minisig->sig.sig));
	memcpy(sig_and_trusted_comment + sizeof(minisig->sig.sig), minisig->trusted_comment,
	    trusted_comment_len);
	if (crypto_ed25519_check(minisig->global_sig, pubkey->keynum_pk.pk, sig_and_trusted_comment, sizeof(minisig->sig.sig) + trusted_comment_len) != 0)
		return -ERANGE;
	return 0;
}



#if 0 /* use openssl for now for performance */
int
xbps_hash_file(struct xbps_hash *hash, const char *path)
{
	unsigned char buf[BUFSIZ];
	struct xbps_hash_state hs;
	int fd;
	int r;

	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd == -1)
		return -errno;

	xbps_hash_init(&hs);
	for (;;) {
		ssize_t rd = read(fd, buf, sizeof(buf));
		if (rd < 0) {
			if (errno == EINTR)
				continue;
			r = -errno;
			close(fd);
			return r;
		}
		if (rd == 0)
			break;
		xbps_hash_update(&hs, buf, rd);
	}
	close(fd);
	xbps_hash_final(&hs, hash);
	return 0;
}
#endif
