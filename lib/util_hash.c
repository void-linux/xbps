/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include <asm-generic/errno-base.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/evp.h>
#include <openssl/err.h>

#include "xbps.h"
#include "xbps_api_impl.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#  define EVP_MD_CTX_new   EVP_MD_CTX_create
#  define EVP_MD_CTX_free  EVP_MD_CTX_destroy
#endif


/**
 * @file lib/util.c
 * @brief Utility routines
 * @defgroup util Utility functions
 */
static void
digest2string(const uint8_t *digest, char *string, size_t len)
{
	while (len--) {
		if (*digest / 16 < 10)
			*string++ = '0' + *digest / 16;
		else
			*string++ = 'a' + *digest / 16 - 10;
		if (*digest % 16 < 10)
			*string++ = '0' + *digest % 16;
		else
			*string++ = 'a' + *digest % 16 - 10;
		++digest;
	}
	*string = '\0';
}

bool
xbps_mmap_file(const char *file, void **mmf, size_t *mmflen, size_t *filelen)
{
	struct stat st;
	size_t pgsize = (size_t)sysconf(_SC_PAGESIZE);
	size_t pgmask = pgsize - 1, mapsize;
	unsigned char *mf;
	bool need_guard = false;
	int fd;

	assert(file);

	if ((fd = open(file, O_RDONLY|O_CLOEXEC)) == -1)
		return false;

	if (fstat(fd, &st) == -1) {
		(void)close(fd);
		return false;
	}
	if (st.st_size > SSIZE_MAX - 1) {
		(void)close(fd);
		return false;
	}
	mapsize = ((size_t)st.st_size + pgmask) & ~pgmask;
	if (mapsize < (size_t)st.st_size) {
		(void)close(fd);
		return false;
	}
	/*
	 * If the file length is an integral number of pages, then we
	 * need to map a guard page at the end in order to provide the
	 * necessary NUL-termination of the buffer.
	 */
	if ((st.st_size & pgmask) == 0)
		need_guard = true;

	mf = mmap(NULL, need_guard ? mapsize + pgsize : mapsize,
	    PROT_READ, MAP_PRIVATE, fd, 0);
	(void)close(fd);
	if (mf == MAP_FAILED) {
		(void)munmap(mf, mapsize);
		return false;
	}

	*mmf = mf;
	*mmflen = mapsize;
	*filelen = st.st_size;

	return true;
}

static const EVP_MD*
algorithm2epv_md(xbps_hash_algorithm_t type) {
	switch (type) {
		case XBPS_HASH_SHA256:
			return EVP_sha256();
		case XBPS_HASH_BLAKE2B256:
			return EVP_blake2s256();
	}
	return NULL;
}

int
xbps_hash_size_raw(xbps_hash_algorithm_t type) {
	return EVP_MD_size(algorithm2epv_md(type));
}

int
xbps_hash_size(xbps_hash_algorithm_t type) {
	return EVP_MD_size(algorithm2epv_md(type)) * 2 + 1;
}

bool 
xbps_file_hash_raw(xbps_hash_algorithm_t type, unsigned char *dst, unsigned int dstlen, const char *file) {
	int fd;
	ssize_t len;
	char buf[65536];
	EVP_MD_CTX *mdctx;

	// invalid type
	if (algorithm2epv_md(type) == NULL) {
		errno = EINVAL;
		return false;
	}		

	if ((int) dstlen < xbps_hash_size_raw(type)) {
		errno = ENOBUFS;
		return false;
	}

	if ((fd = open(file, O_RDONLY)) < 0)
		return false;

	if((mdctx = EVP_MD_CTX_new()) == NULL) {
		xbps_error_printf("Unable to initial openssl: %s\n", ERR_error_string(ERR_get_error(), NULL));
		return false;
	}

	if(EVP_DigestInit_ex(mdctx, algorithm2epv_md(type), NULL) != 1) {
		xbps_error_printf("Unable to initial algorithm: %s\n", ERR_error_string(ERR_get_error(), NULL));
		return false;
	}

	while ((len = read(fd, buf, sizeof(buf))) > 0)
		if(EVP_DigestUpdate(mdctx, buf, len) != 1) {
			xbps_error_printf("Unable to update digest: %s\n", ERR_error_string(ERR_get_error(), NULL));
			return false;
		}

	close(fd);

	if(EVP_DigestFinal_ex(mdctx, dst, &dstlen) != 1) {
		xbps_error_printf("Unable to finalize digest: %s\n", ERR_error_string(ERR_get_error(), NULL));
		return false;
	}

	EVP_MD_CTX_free(mdctx);

	return true;
}

bool
xbps_file_hash(xbps_hash_algorithm_t type, char *dst, size_t dstlen, const char *file)
{
	unsigned char digest[XBPS_SHA256_DIGEST_SIZE];

	assert(dstlen >= XBPS_SHA256_SIZE);
	if (dstlen < XBPS_SHA256_SIZE) {
		errno = ENOBUFS;
		return false;
	}

	if (!xbps_file_hash_raw(type, digest, sizeof digest, file))
		return false;

	digest2string(digest, dst, XBPS_SHA256_DIGEST_SIZE);

	return true;
}

static bool
hash_digest_compare(const char *sha256, size_t shalen,
		const unsigned char *digest, size_t digestlen)
{

	assert(shalen == XBPS_SHA256_SIZE - 1);
	if (shalen != XBPS_SHA256_SIZE -1)
		return false;

	assert(digestlen == XBPS_SHA256_DIGEST_SIZE);
	if (digestlen != XBPS_SHA256_DIGEST_SIZE)
		return false;

	for (; *sha256;) {
		if (*digest / 16 < 10) {
			if (*sha256++ != '0' + *digest / 16)
				return false;
		} else {
			if (*sha256++ != 'a' + *digest / 16 - 10)
				return false;
		}
		if (*digest % 16 < 10) {
			if (*sha256++ != '0' + *digest % 16)
				return false;
		} else {
			if (*sha256++ != 'a' + *digest % 16 - 10)
				return false;
		}
		digest++;
	}

	return true;
}

int
xbps_file_hash_check(xbps_hash_algorithm_t type, const char *file, const char *sha256)
{
	unsigned char digest[XBPS_SHA256_DIGEST_SIZE];

	assert(file != NULL);
	assert(sha256 != NULL);

	if (!xbps_file_hash_raw(type, digest, sizeof digest, file))
		return errno;

	if (!hash_digest_compare(sha256, strlen(sha256), digest, sizeof digest))
		return ERANGE;

	return 0;
}

static const char *
file_hash_dictionary(xbps_dictionary_t d, const char *key, const char *file)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *curfile = NULL, *sha256 = NULL;

	assert(xbps_object_type(d) == XBPS_TYPE_DICTIONARY);
	assert(key != NULL);
	assert(file != NULL);

	iter = xbps_array_iter_from_dict(d, key);
	if (iter == NULL) {
		errno = ENOENT;
		return NULL;
	}
	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj,
		    "file", &curfile);
		if (strcmp(file, curfile) == 0) {
			/* file matched */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "sha256", &sha256);
			break;
		}
	}
	xbps_object_iterator_release(iter);
	if (sha256 == NULL)
		errno = ENOENT;

	return sha256;
}

int HIDDEN
xbps_file_hash_check_dictionary(struct xbps_handle *xhp,
				xbps_dictionary_t d,
				const char *key,
				const char *file)
{
	const char *sha256d = NULL;
	char *buf;
	int rv;

	assert(xbps_object_type(d) == XBPS_TYPE_DICTIONARY);
	assert(key != NULL);
	assert(file != NULL);

	if ((sha256d = file_hash_dictionary(d, key, file)) == NULL) {
		if (errno == ENOENT)
			return 1; /* no match, file not found */

		return -1; /* error */
	}

	if (strcmp(xhp->rootdir, "/") == 0) {
		rv = xbps_file_hash_check(XBPS_HASH_SHA256, file, sha256d);
	} else {
		buf = xbps_xasprintf("%s/%s", xhp->rootdir, file);
		rv = xbps_file_hash_check(XBPS_HASH_SHA256, buf, sha256d);
		free(buf);
	}
	if (rv == 0)
		return 0; /* matched */
	else if (rv == ERANGE || rv == ENOENT)
		return 1; /* no match */
	else
		return -1; /* error */
}
