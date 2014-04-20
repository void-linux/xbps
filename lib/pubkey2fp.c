/*
 * An implementation of convertion from OpenSSL to OpenSSH public key format
 *
 * Copyright (c) 2008 Mounir IDRASSI <mounir.idrassi@idrix.fr>. All rights reserved.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#include "xbps_api_impl.h"

static unsigned char pSshHeader[11] = {
	0x00, 0x00, 0x00, 0x07, 0x73, 0x73, 0x68, 0x2D, 0x72, 0x73, 0x61
};

static int
SshEncodeBuffer(unsigned char *pEncoding, int bufferLen, unsigned char *pBuffer)
{
	int adjustedLen = bufferLen, index;

	if (*pBuffer & 0x80) {
		adjustedLen++;
		pEncoding[4] = 0;
		index = 5;
	} else {
		index = 4;
	}
	pEncoding[0] = (unsigned char) (adjustedLen >> 24);
	pEncoding[1] = (unsigned char) (adjustedLen >> 16);
	pEncoding[2] = (unsigned char) (adjustedLen >>  8);
	pEncoding[3] = (unsigned char) (adjustedLen      );
	memcpy(&pEncoding[index], pBuffer, bufferLen);
	return index + bufferLen;
}

static char *
fp2str(unsigned const char *fp, unsigned int len)
{
	unsigned int i, c = 0;
	char res[48], cur[4];

	for (i = 0; i < len; i++) {
		if (i > 0)
			c = i*3;
		sprintf(cur, "%02x", fp[i]);
		res[c] = cur[0];
		res[c+1] = cur[1];
		res[c+2] = ':';
	}
	res[c+2] = '\0';

	return strdup(res);
}

char *
xbps_pubkey2fp(struct xbps_handle *xhp, xbps_data_t pubkey)
{
	EVP_MD_CTX mdctx;
	EVP_PKEY *pPubKey = NULL;
	RSA *pRsa = NULL;
	BIO *bio = NULL;
	const void *pubkeydata;
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned char *nBytes = NULL, *eBytes = NULL, *pEncoding = NULL;
	unsigned int md_len = 0;
	char *hexfpstr = NULL;
	int index = 0, nLen = 0, eLen = 0, encodingLength = 0;

	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();

	pubkeydata = xbps_data_data_nocopy(pubkey);
	bio = BIO_new_mem_buf(__UNCONST(pubkeydata), xbps_data_size(pubkey));
	assert(bio);

	pPubKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
	if (!pPubKey) {
		xbps_dbg_printf(xhp,
		    "unable to decode public key from the given file: %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		goto error;
	}

	if (EVP_PKEY_type(pPubKey->type) != EVP_PKEY_RSA) {
		xbps_dbg_printf(xhp, "only RSA public keys are currently supported\n");
		goto error;
	}

	pRsa = EVP_PKEY_get1_RSA(pPubKey);
	if (!pRsa) {
		xbps_dbg_printf(xhp, "failed to get RSA public key : %s\n",
		    ERR_error_string(ERR_get_error(), NULL));
		goto error;
	}

	// reading the modulus
	nLen = BN_num_bytes(pRsa->n);
	nBytes = (unsigned char*) malloc(nLen);
	BN_bn2bin(pRsa->n, nBytes);

	// reading the public exponent
	eLen = BN_num_bytes(pRsa->e);
	eBytes = (unsigned char*) malloc(eLen);
	BN_bn2bin(pRsa->e, eBytes);

	encodingLength = 11 + 4 + eLen + 4 + nLen;
	// correct depending on the MSB of e and N
	if (eBytes[0] & 0x80)
		encodingLength++;
	if (nBytes[0] & 0x80)
		encodingLength++;

	pEncoding = malloc(encodingLength);
	memcpy(pEncoding, pSshHeader, 11);

	index = SshEncodeBuffer(&pEncoding[11], eLen, eBytes);
	(void)SshEncodeBuffer(&pEncoding[11 + index], nLen, nBytes);

	/*
	 * Compute the RSA fingerprint (MD5).
	 */
	EVP_MD_CTX_init(&mdctx);
	EVP_DigestInit_ex(&mdctx, EVP_md5(), NULL);
	EVP_DigestUpdate(&mdctx, pEncoding, encodingLength);
	if (EVP_DigestFinal_ex(&mdctx, md_value, &md_len) == 0)
		goto error;
	EVP_MD_CTX_cleanup(&mdctx);
	/*
	 * Convert result to a compatible OpenSSH hex fingerprint.
	 */
	hexfpstr = fp2str(md_value, md_len);

error:
	if (bio)
		BIO_free_all(bio);
	if (pRsa)
		RSA_free(pRsa);
	if (pPubKey)
		EVP_PKEY_free(pPubKey);
	if (nBytes)
		free(nBytes);
	if (eBytes)
		free(eBytes);
	if (pEncoding)
		free(pEncoding);

	EVP_cleanup();
	ERR_free_strings();

	return hexfpstr;
}
