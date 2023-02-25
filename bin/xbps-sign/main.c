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

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <xbps.h>
#include <xbps/crypto.h>

#define PASSPHRASE_MAX_BYTES 1024

static const char *comment = NULL;
static const char *pubkey_file = NULL;
static const char *pubkey_s = NULL;
static const char *seckey_file = NULL;
static const char *passphrase_file = NULL;
static const char *msg_file = NULL;
static const char *sig_file = NULL;

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stdout,
	"Usage: xbps-sign MODE OPTIONS\n"
	"       xbps-sign -G [-c comment] -p pubkey -s seckey\n"
	"       xbps-sign -S [-x sigfile] -s seckey -m file\n"
	"       xbps-sign -V [-x sigfile] [-p pubkey] -m file\n"
	"\nMODE\n"
	" -G --generate           Generate a new key pair\n"
	" -S --sign               Sign a file\n"
	" -V --verify             Verify a file\n"
	" -h --help               Print help usage\n"
	"    --version            Prints the xbps release version\n"
	"\nOPTIONS\n"
	" -m --message <file>     Message file to sign/verify\n"
	" -p --pubkey <file>      Public-key file\n"
	" -s --seckey <file>      Secret-key file\n"
	" -x --signature <file>   Signature file (default <message-file>.minisig)\n"
	" -c --comment <comment>  Untrusted comment\n"
	"    --passphrase <file>  Passphrase file\n"
	"");
	exit(EXIT_FAILURE);
}

static const char *
read_passphrase(char *buf, size_t bufsz)
{
	if (passphrase_file) {
		FILE *fp;
		int r = 0;

		fp = fopen(passphrase_file, "r");
		if (!fp) {
			xbps_error_printf("failed to open passphrase file: %s: %s\n",
			    passphrase_file, strerror(errno));
			exit(1);
		}
		if (!fgets(buf, bufsz, fp))
			r = -EINVAL;
		if (fclose(fp) != 0)
			r = -errno;
		if (r < 0) {
			xbps_error_printf("failed to read passphrase file: %s: %s\n",
			    passphrase_file, strerror(-r));
			exit(1);
		}
		return buf;
	}
	xbps_warn_printf("generating unencrypted secret-key\n");
	return NULL;
}


static void __attribute__((noreturn))
generate(void)
{
	char passphrase_buf[PASSPHRASE_MAX_BYTES];
	struct xbps_pubkey pubkey = {0};
	struct xbps_seckey seckey = {0};
	const char *passphrase = NULL;
	int r;

	if (!seckey_file) {
		xbps_error_printf("missing secret-key path\n");
		exit(1);
	}

	passphrase = read_passphrase(passphrase_buf, sizeof(passphrase_buf));

	r = xbps_generate_keypair(&seckey, &pubkey);
	if (r < 0) {
		exit(1);
	}

	r = xbps_seckey_write(&seckey, passphrase, seckey_file);
	if (passphrase)
		xbps_wipe_secret(passphrase_buf, sizeof(passphrase_buf));
	xbps_wipe_secret(&seckey, sizeof(seckey));
	if (r < 0) {
		xbps_error_printf("failed to write secret-key file: %s: %s\n",
		    seckey_file, strerror(-r));
		exit(1);
	}

	if (pubkey_file) {
		r = xbps_pubkey_write(&pubkey, pubkey_file);
		if (r < 0) {
			xbps_warn_printf("failed to write public-key file: %s: %s\n",
			    pubkey_file, strerror(-r));
			exit(1);
		}
	}

	exit(0);
}

static void
load_pubkey(struct xbps_pubkey *pubkey)
{
	if (pubkey_file) {
		int fd;
		int r;
		fd = open(pubkey_file, O_RDONLY);
		if (fd == -1) {
			xbps_error_printf("failed to open public-key file: %s: %s\n",
			   pubkey_file, strerror(errno));
			exit(1);
		}
		r = xbps_pubkey_read(pubkey, fd);
		close(fd);
		if (r < 0) {
			xbps_error_printf("failed to read public-key file: %s: %s\n",
			   pubkey_file, strerror(-r));
			exit(1);
		}
	} else if (pubkey_s) {
		int r = xbps_pubkey_decode(pubkey, pubkey_s);
		if (r < 0) {
			xbps_error_printf("failed to decode public-key: %s\n", strerror(-r));
			exit(1);
		}
	} else {
		xbps_error_printf("missing public-key\n");
		exit(1);
	}
}

static void
load_seckey(struct xbps_seckey *seckey)
{
	char passphrase_buf[PASSPHRASE_MAX_BYTES];
	const char *passphrase = NULL;
	int r;

	if (!seckey_file) {
		xbps_error_printf("missing secret-key\n");
		exit(1);
	}
	if (passphrase_file)
		exit(1);
	r = xbps_seckey_read(seckey, passphrase, seckey_file);
	if (passphrase)
		xbps_wipe_secret(passphrase_buf, sizeof(passphrase_buf));
	if (r < 0) {
		xbps_error_printf("failed to read secret-key file: %s: %s\n",
		    seckey_file, strerror(-r));
		exit(1);
	}
}

static void __attribute__((noreturn))
sign(void)
{
	char sigfile_buf[PATH_MAX];
	struct xbps_seckey seckey = {0};
	struct xbps_pubkey pubkey = {0};
	struct xbps_minisig minisig = {0};
	struct xbps_hash hash;
	int r;

	if (!msg_file) {
		xbps_error_printf("missing file to sign\n");
		exit(1);
	}

	if (pubkey_file || pubkey_s)
		load_pubkey(&pubkey);

	r = xbps_hash_file(&hash, msg_file);
	if (r < 0) {
		xbps_error_printf("failed to hash file: %s: %s\n",
		    msg_file, strerror(-r));
		exit(1);
	}

	xbps_strlcpy(minisig.comment, "signature from minisign secret-key", sizeof(minisig.comment));
	snprintf(minisig.trusted_comment, sizeof(minisig.trusted_comment),
	    "foo bar");

	load_seckey(&seckey);

	r = xbps_minisig_sign(&minisig, &seckey, &hash);
	if (r < 0) {
		xbps_wipe_secret(&seckey, sizeof(seckey));
		xbps_error_printf("failed to sign file: %s: %s\n",
		    seckey_file, strerror(errno));
		exit(1);
	}
	xbps_wipe_secret(&seckey, sizeof(seckey));

	if (pubkey_file || pubkey_s) {
		r = xbps_minisig_verify(&minisig, &pubkey, &hash);
		if (r < 0) {
			xbps_error_printf("failed to verify generated signature: %s\n",
			    strerror(-r));
			exit(1);
		}
	}

	if (!sig_file) {
		snprintf(sigfile_buf, sizeof(sigfile_buf), "%s.minisig", msg_file);
		sig_file = sigfile_buf;
	}
	r = xbps_minisig_write(&minisig, sig_file);
	if (r < 0) {
		xbps_error_printf("failed to write signature file: %s: %s\n",
		    sig_file, strerror(-r));
		exit(1);
	}
	exit(0);
}

static void __attribute__((noreturn))
verify(void)
{
	char sigfile_buf[PATH_MAX];
	struct xbps_pubkey pubkey = {0};
	struct xbps_hash hash;
	struct xbps_minisig minisig;
	int r;

	load_pubkey(&pubkey);

	r = xbps_hash_file(&hash, msg_file);
	if (r < 0) {
		xbps_error_printf("failed to hash file: %s: %s\n",
		    msg_file, strerror(-r));
		exit(1);
	}
	if (!sig_file) {
		snprintf(sigfile_buf, sizeof(sigfile_buf), "%s.minisig", msg_file);
		sig_file = sigfile_buf;
	}
	r = xbps_minisig_read(&minisig, sig_file);
	if (r < 0) {
		xbps_error_printf("failed to read minisig file: %s: %s\n",
		    sig_file, strerror(-r));
		exit(1);
	}
	fprintf(stderr, "untrusted comment: %s\n", minisig.comment);
	fprintf(stderr, "trusted comment: %s\n", minisig.trusted_comment);
	r = xbps_minisig_verify(&minisig, &pubkey, &hash);
	if (r < 0) {
		xbps_error_printf("failed to verify file: %s: %s\n",
		    msg_file, strerror(-r));
		exit(1);
	}
	exit(0);
}

int
main(int argc, char *argv[])
{
	const char *shortopts = "dGHSVhc:p:s:m:x:P:";
	const struct option longopts[] = {
		{ "generate", no_argument, NULL, 'G' },
		{ "sign", no_argument, NULL, 'S' },
		{ "verify", no_argument, NULL, 'V' },
		{ "message", no_argument, NULL, 'm' },
		{ "seckey", required_argument, NULL, 's' },
		{ "pubkey-file", required_argument, NULL, 'p' },
		{ "pubkey", required_argument, NULL, 'P' },
		{ "comment", required_argument, NULL, 'c' },
		{ "passphrase-file", required_argument, NULL, 1 },
		{ "help", no_argument, NULL, 'h' },
		{ "version", no_argument, NULL, 0 },
		{ "debug", no_argument, NULL, 'd' },
		{ NULL, 0, NULL, 0 }
	};
	int c;

	enum {
		GENERATE = 1,
		SIGN,
		VERIFY,
	} act = 0;

	(void) comment;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'd':
			xbps_debug_level = 1;
			break;
		case 'G':
			act = GENERATE;
			break;
		case 'V':
			act = VERIFY;
			break;
		case 'c':
			comment = optarg;
			break;
		case 'p':
			pubkey_file = optarg;
			break;
		case 'P':
			pubkey_s = optarg;
			break;
		case 's':
			seckey_file = optarg;
			break;
		case 'S':
			act = SIGN;
			break;
		case 'x':
			sig_file = optarg;
			break;
		case 'm':
			msg_file = optarg;
			break;
		case '?':
		case 'h':
			usage();
			/* NOTREACHED */
		case 0:
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 1:
			passphrase_file = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	switch (act) {
	case GENERATE: generate(); break;
	case SIGN:     sign();     break;
	case VERIFY:   verify();   break;
	default:
		usage();
	}
	exit(1);
}
