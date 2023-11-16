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

#define _DEFAULT_SOURCE /* reallocarray */
#include <sys/stat.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "crypto-impl.h"
#include "external/codecs.h"
#include "xbps.h"

#include "compat.h"

/**
 * @file lib/crypto.c
 * @brief Cryptography functinons
 * @defgroup crypto Cryptography functions for public-key signatures
 *
 * Functions to sign and verify Ed25519 public-key signatues.
 *
 * The public-key signatures and secret keys are based on and comptaible with
 * [minisign](https://jedisct1.github.io/minisign/) and use
 * [libsodium](https://libsodium.org/).
 */

#define B64_LEN(BIN_LEN) BASE64_ENCODED_LEN((BIN_LEN), BASE64_VARIANT_ORIGINAL)

static inline int
b64encode(char *b64, size_t b64_len, const void *bin, size_t bin_len)
{
	assert(b64_len >= B64_LEN(bin_len));
	if (!bin2base64(b64, b64_len, bin, bin_len, BASE64_VARIANT_ORIGINAL))
		return -ENOBUFS;
	return 0;
}

static inline int
b64decode(void *bin, size_t bin_len, const char *b64, size_t b64_len)
{
	size_t decoded_len = 0;
	if (base642bin(bin, bin_len, b64, b64_len, NULL, &decoded_len,
	    NULL, BASE64_VARIANT_ORIGINAL) != 0)
		return -EINVAL;
	if (decoded_len != bin_len)
		return -EINVAL;
	return 0;
}

static int
pubkey_decode(struct xbps_pubkey *pubkey, const char *pubkey_s, size_t pubkey_s_len)
{
	int r;

	r = b64decode(pubkey, sizeof(*pubkey), pubkey_s, pubkey_s_len);
	if (r < 0)
		return -EINVAL;
	if (memcmp(pubkey->sig_alg, SIGALG, sizeof(pubkey->sig_alg)) != 0) {
		xbps_dbg_printf("%s: unsupported public key signature algortihm\n", __FUNCTION__);
		return -ENOTSUP;
	}
	return 0;
}

int
xbps_pubkey_decode(struct xbps_pubkey *pubkey, const char *pubkey_s)
{
	return pubkey_decode(pubkey, pubkey_s, strlen(pubkey_s));
}

struct buf {
	char *pos, *end;
	char mem[BUFSIZ+1];
};

static int
readline(int fd, struct buf *buf, char *dst, size_t dstsz, size_t *linelen)
{
	char *d = NULL;
	size_t l = 0;
	if (!buf->pos) {
		buf->pos = buf->end = buf->mem;
		*buf->pos = '\0';
	}
	for (const char *scan = buf->pos; !(d = strpbrk(scan, "\r\n"));) {
		ssize_t rd;
		/* no newline in buffer, copy out data so we can read more at once */
		if (buf->end - buf->pos > 0) {
			size_t n = buf->end - buf->pos;
			if (l + n >= dstsz)
				return -ENOBUFS;
			memcpy(dst + l, buf->pos, n);
			l += n;
			scan = buf->pos = buf->end = buf->mem;
		}
		/* fill the buffer */
		rd = read(fd, buf->end, sizeof(buf->mem) - 1 - (buf->end - buf->pos));
		if (rd < 0) {
			if (errno != EINTR)
				return -errno;
			continue;
		} else if (rd == 0) {
			break;
		}
		scan = buf->end;
		buf->end += rd;
		*buf->end = '\0';
	}
	/* copy til EOF if there is no trailing newline */
	if (!d)
		d = buf->end;
	if (d > buf->pos) {
		size_t n = d - buf->pos;
		if (l + n >= dstsz)
			return -ENOBUFS;
		memcpy(dst + l, buf->pos, n);
		l += n;
		if (d == buf->end) {
			buf->pos = buf->end;
		} else {
			buf->pos = d + 1 + (*d == '\r' && d[1] == '\n');
		}
	}
	dst[l] = '\0';
	if (linelen)
		*linelen = l;
	return 0;
}

static int
writeflush(int fd, struct buf *buf)
{
	while (buf->pos < buf->end) {
		ssize_t wr = write(fd, buf->pos, buf->end - buf->pos);
		if (wr < 0) {
			if (errno != EINTR)
				return -errno;
			continue;
		}
		buf->pos += wr;
	}
	buf->pos = buf->end = buf->mem;
	return 0;
}

static int
writebuf(int fd, struct buf *buf, const void *src, size_t srclen)
{
	if (!buf->pos)
		buf->pos = buf->end = buf->mem;
	while (srclen > 0) {
		size_t avail = sizeof(buf->mem) - 1 - (buf->end - buf->pos);
		size_t n;
		if (avail == 0) {
			int r = writeflush(fd, buf);
			if (r < 0)
				return r;
			continue;
		}
		n = srclen < avail ? srclen : avail;
		memcpy(buf->end, src, n);
		srclen -= n;
		src = (const char *)src + n;
		buf->end += n;
		*buf->end = '\0';
	}
	return 0;
}

static int
writestrs(int fd, struct buf *buf, ...)
{
	va_list ap;
	int r = 0;

	va_start(ap, buf);
	for (;;) {
		const char *s = va_arg(ap, const char *);
		if (!s)
			break;
		r = writebuf(fd, buf, s, strlen(s));
		if (r < 0)
			break;
	}
	va_end(ap);
	return r;
}

int
xbps_pubkey_read(struct xbps_pubkey *pubkey, int fd)
{
	char comment[COMMENTMAXBYTES];
	char pubkey_s[B64_LEN(sizeof(struct xbps_pubkey))];
	struct buf buf = {0};
	size_t pubkey_s_len = 0;
	int r;

	r = readline(fd, &buf, comment, sizeof(comment), NULL);
	if (r < 0) {
		xbps_dbg_printf("missing or invalid comment\n");
		return r;
	}
	r = readline(fd, &buf, pubkey_s, sizeof(pubkey_s), &pubkey_s_len);
	if (r < 0) {
		xbps_dbg_printf("missing or invalid base64 encoded public key\n");
		return r;
	}
	r = pubkey_decode(pubkey, pubkey_s, pubkey_s_len);
	if (r < 0) {
		xbps_dbg_printf("failed to decode base64 encoded public key: '%.*s'\n",
		    (int)pubkey_s_len, pubkey_s);
		return r;
	}
	return 0;
}

static uint64_t
le64_load(const unsigned char *p)
{
	return ((uint64_t) (p[0])) | ((uint64_t) (p[1]) << 8) | ((uint64_t) (p[2]) << 16) |
	    ((uint64_t) (p[3]) << 24) | ((uint64_t) (p[4]) << 32) | ((uint64_t) (p[5]) << 40) |
	    ((uint64_t) (p[6]) << 48) | ((uint64_t) (p[7]) << 56);
}

int
xbps_pubkey_encode(const struct xbps_pubkey *pubkey, char *pubkey_s, size_t pubkey_s_len)
{
	return b64encode(pubkey_s, pubkey_s_len, pubkey, sizeof(*pubkey));
}

int
xbps_pubkey_write(const struct xbps_pubkey *pubkey, const char *path)
{
	char comment[COMMENTMAXBYTES];
	char pubkey_s[B64_LEN(sizeof(struct xbps_pubkey))];
	struct buf buf = {0};
	int fd;
	int r;

	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC, 0644);
	if (fd == -1)
		return -errno;

	snprintf(comment, sizeof(comment), "minisign public key %" PRIX64,
	    le64_load(pubkey->keynum_pk.keynum));

	xbps_pubkey_encode(pubkey, pubkey_s, sizeof(pubkey_s));

	r = writestrs(fd, &buf, COMMENT_PREFIX, comment, "\n", pubkey_s, "\n", (char *)NULL);
	if (r < 0)
		goto err;

	r = writeflush(fd, &buf);
	if (r < 0)
		goto err;

	close(fd);
	return 0;
err:
	close(fd);
	unlink(path);
	return r;
}

#if 0
static void
xor_buf(unsigned char *dst, const unsigned char *src, size_t len)
{
	size_t i;

	for (i = (size_t) 0U; i < len; i++) {
		dst[i] ^= src[i];
	}
}

static void
le64_store(unsigned char *p, uint64_t x)
{
	p[0] = (unsigned char) x;
	p[1] = (unsigned char) (x >> 8);
	p[2] = (unsigned char) (x >> 16);
	p[3] = (unsigned char) (x >> 24);
	p[4] = (unsigned char) (x >> 32);
	p[5] = (unsigned char) (x >> 40);
	p[6] = (unsigned char) (x >> 48);
	p[7] = (unsigned char) (x >> 56);
}
#endif

int
xbps_seckey_write(const struct xbps_seckey *seckey, const char *passphrase, const char *path)
{
	char seckey_s[B64_LEN(sizeof(*seckey))];
	struct xbps_seckey seckey_enc;
	struct buf buf = {0};
	int fd;
	int r;

	if (passphrase) {
		seckey_enc = *seckey;
		r = encrypt_key(&seckey_enc, passphrase);
		if (r < 0) {
			xbps_wipe_secret(&seckey_enc, sizeof(seckey_enc));
			return r;
		}
		seckey = &seckey_enc;
	}

	fd = open(path, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC, 0600);
	if (fd == -1)
		return -errno;

	b64encode(seckey_s, sizeof(seckey_s), seckey, sizeof(*seckey));

	r = writestrs(fd, &buf, COMMENT_PREFIX, SECRETKEY_DEFAULT_COMMENT, "\n",
	    seckey_s, "\n", (char *)NULL);
	xbps_wipe_secret(seckey_s, sizeof(seckey_s));
	if (r < 0)
		goto err;
	r = writeflush(fd, &buf);
	if (r < 0)
		goto err;
	close(fd);
	if (seckey == &seckey_enc)
		xbps_wipe_secret(&seckey_enc, sizeof(seckey_enc));
	return 0;
err:
	if (seckey == &seckey_enc)
		xbps_wipe_secret(&seckey_enc, sizeof(seckey_enc));
	close(fd);
	unlink(path);
	return r;
}

static int
seckey_decode(struct xbps_seckey *seckey, const char *seckey_s, size_t seckey_s_len,
		const char *passphrase)
{
	int r;

	r = b64decode(seckey, sizeof(*seckey), seckey_s, seckey_s_len);
	if (r < 0)
		goto err;
	if (memcmp(seckey->sig_alg, SIGALG, sizeof(seckey->sig_alg)) != 0 ||
	    memcmp(seckey->chk_alg, CHKALG, sizeof(seckey->chk_alg)) != 0) {
		r = -ENOTSUP;
		goto err;
	}
	if (memcmp(seckey->kdf_alg, KDFALG, sizeof seckey->kdf_alg) == 0) {
		if (!passphrase) {
			r = -ERANGE;
			goto err;
		}
		r = decrypt_key(seckey, passphrase);
		if (r < 0)
			goto err;
	} else if (memcmp(seckey->kdf_alg, KDFNONE, sizeof seckey->kdf_alg) == 0) {
		/* unencrypted key */
	} else {
		r = -ENOTSUP;
		goto err;
	}
	return 0;
err:
	xbps_wipe_secret(seckey, sizeof(*seckey));
	return r;
}

int
xbps_seckey_read(struct xbps_seckey *seckey, const char *passphrase, const char *path)
{
	char comment[COMMENTMAXBYTES];
	char seckey_s[B64_LEN(sizeof(*seckey))];
	struct buf buf = {0};
	size_t linelen = 0;
	int fd;
	int r;

	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd == -1)
		return -errno;

	r = readline(fd, &buf, comment, sizeof(comment), &linelen);
	if (r < 0) {
		xbps_dbg_printf("%s: error reading comment: %s\n",
		    __FUNCTION__, strerror(-r));
		goto err;
	}
	r = readline(fd, &buf, seckey_s, sizeof(seckey_s), &linelen);
	if (r < 0) {
		xbps_dbg_printf("%s: error reading base64: %s\n",
		    __FUNCTION__, strerror(-r));
		goto err;
	}
	r = seckey_decode(seckey, seckey_s, linelen, passphrase);
	if (r < 0) {
		xbps_dbg_printf("%s: error decoding: %s\n",
		    __FUNCTION__, strerror(-r));
		goto err;
	}

err:
	close(fd);
	xbps_wipe_secret(&buf, sizeof(buf));
	xbps_wipe_secret(seckey_s, sizeof(seckey_s));
	return r;
}

int
xbps_minisig_read(struct xbps_minisig *minisig, const char *path)
{
	char sig_s[B64_LEN(sizeof(minisig->sig))];
	char global_sig_s[B64_LEN(sizeof(minisig->global_sig))];
	struct buf buf = {0};
	size_t sig_s_len = 0, global_sig_s_len = 0;
	size_t comment_len = 0, trusted_comment_len = 0;
	int fd;
	int r;

	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd == -1)
		return -errno;

	r = readline(fd, &buf, minisig->comment, sizeof(minisig->comment), &comment_len);
	if (r < 0)
		goto err;
	if (strncmp(minisig->comment, COMMENT_PREFIX, sizeof(COMMENT_PREFIX) - 1) != 0) {
		r = -EINVAL;
		goto err;
	}
	memmove(minisig->comment,
	    minisig->comment + sizeof(COMMENT_PREFIX) - 1,
	    comment_len - sizeof(COMMENT_PREFIX) + 2);

	r = readline(fd, &buf, sig_s, sizeof(sig_s), &sig_s_len);
	if (r < 0)
		goto err;
	r = b64decode(&minisig->sig, sizeof(minisig->sig), sig_s, sig_s_len);
	if (r < 0)
		goto err;
	if (memcmp(minisig->sig.sig_alg, SIGALG_HASHED, sizeof(minisig->sig.sig_alg)) != 0) {
		r = -ENOTSUP;
		goto err;
	}

	r = readline(fd, &buf, minisig->trusted_comment, sizeof(minisig->trusted_comment),
	    &trusted_comment_len);
	if (r < 0)
		goto err;
	if (strncmp(minisig->trusted_comment, TRUSTED_COMMENT_PREFIX,
	    sizeof(TRUSTED_COMMENT_PREFIX) - 1) != 0) {
		r = -EINVAL;
		goto err;
	}
	memmove(minisig->trusted_comment,
	    minisig->trusted_comment + sizeof(TRUSTED_COMMENT_PREFIX) - 1,
	    trusted_comment_len - sizeof(TRUSTED_COMMENT_PREFIX) + 2);

	r = readline(fd, &buf, global_sig_s, sizeof(global_sig_s), &global_sig_s_len);
	if (r < 0)
		goto err;
	r = b64decode(minisig->global_sig, sizeof(minisig->global_sig),
	    global_sig_s, global_sig_s_len);
	if (r < 0)
		goto err;
	close(fd);
	return 0;
err:
	close(fd);
	return r;
}

struct atomicfile {
	char path[PATH_MAX];
};

static int
atomicfile_open(struct atomicfile *a, const char *path)
{
	const char *fname;
	int fd;
	int l;

	fname =  strrchr(path, '/');
	if (fname) {
		size_t dirlen = fname - path;
		if (dirlen >= PATH_MAX) {
			errno = ENOBUFS;
			return -1;
		}
		l = snprintf(a->path, sizeof(a->path), "%.*s/.%s.XXXXXXX",
		    (int)dirlen, path, fname+1);
	} else {
		l = snprintf(a->path, sizeof(a->path), ".%s.XXXXXXX", path);
	}
	if (l < 0 || (size_t)l >= sizeof(a->path)) {
		errno = ENOBUFS;
		return -1;
	}
	fd = mkstemp(a->path);
	if (fd == -1)
		return -1;
	return fd;
}

int
xbps_minisig_write(const struct xbps_minisig *minisig, const char *path)
{
	char sig_s[B64_LEN(sizeof(minisig->sig))];
	char global_sig_s[B64_LEN(sizeof(minisig->global_sig))];
	struct buf buf = {0};
	struct atomicfile tmpfile;
	int fd;
	int r;

	fd = atomicfile_open(&tmpfile, path);
	if (fd == -1)
		return -errno;

	b64encode(sig_s, sizeof(sig_s), &minisig->sig, sizeof(minisig->sig));
	b64encode(global_sig_s, sizeof(global_sig_s),
	    &minisig->global_sig, sizeof(minisig->global_sig));

	r = writestrs(fd, &buf, COMMENT_PREFIX, minisig->comment, "\n", sig_s,
	    "\n", TRUSTED_COMMENT_PREFIX, minisig->trusted_comment, "\n",
	    global_sig_s, "\n",
	    (char *)NULL);
	if (r < 0)
		goto err;
	r = writeflush(fd, &buf);
	if (r < 0)
		goto err;
	if (rename(tmpfile.path, path) == -1) {
		r = -errno;
		goto err;
	}
	close(fd);
	return 0;
err:
	close(fd);
	unlink(tmpfile.path);
	return r;
}
