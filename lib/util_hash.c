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
#include <sys/mman.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>

#include <openssl/sha.h>

#include "xbps_api_impl.h"

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

unsigned char *
xbps_buffer_hash_raw(const char *buffer, ssize_t len)
{
	unsigned char *digest;
	SHA256_CTX sha256;

	digest = malloc(SHA256_DIGEST_LENGTH);
	assert(digest);

	SHA256_Init(&sha256);
	SHA256_Update(&sha256, buffer, len);
	SHA256_Final(digest, &sha256);

	return digest;
}

unsigned char *
xbps_file_hash_raw(const char *file)
{
	int fd;
	ssize_t len;
	unsigned char *digest, buf[65536];
	SHA256_CTX sha256;

	if ((fd = open(file, O_RDONLY)) < 0)
		return NULL;
	digest = malloc(SHA256_DIGEST_LENGTH);
	assert(digest);
	SHA256_Init(&sha256);
	while ((len = read(fd, buf, sizeof(buf))) > 0)
		SHA256_Update(&sha256, buf, len);
	if(len < 0) {
		free(digest);
		return NULL;
	}
	SHA256_Final(digest, &sha256);
	(void)close(fd);

	return digest;
}

char *
xbps_file_hash(const char *file)
{
	char *hash;
	unsigned char *digest;

	if (!(digest = xbps_file_hash_raw(file)))
		return NULL;

	hash = malloc(SHA256_DIGEST_LENGTH * 2 + 1);
	assert(hash);
	digest2string(digest, hash, SHA256_DIGEST_LENGTH);
	free(digest);

	return hash;
}

int
xbps_file_hash_check(const char *file, const char *sha256)
{
	char *res;

	assert(file != NULL);
	assert(sha256 != NULL);

	res = xbps_file_hash(file);
	if (res == NULL)
		return errno;

	if (strcmp(sha256, res)) {
		free(res);
		return ERANGE;
	}
	free(res);

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
		rv = xbps_file_hash_check(file, sha256d);
	} else {
		buf = xbps_xasprintf("%s/%s", xhp->rootdir, file);
		rv = xbps_file_hash_check(buf, sha256d);
		free(buf);
	}
	if (rv == 0)
		return 0; /* matched */
	else if (rv == ERANGE || rv == ENOENT)
		return 1; /* no match */
	else
		return -1; /* error */
}
