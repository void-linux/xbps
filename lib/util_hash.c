/*-
 * Copyright (c) 2008-2011 Juan Romero Pardines.
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

char *
xbps_file_hash(const char *file)
{
	struct stat st;
	size_t pgsize = (size_t)sysconf(_SC_PAGESIZE);
	size_t pgmask = pgsize - 1, mapsize;
	char hash[SHA256_DIGEST_LENGTH * 2 + 1];
	unsigned char *buf = NULL, digest[SHA256_DIGEST_LENGTH];
	int fd;
	bool need_guard = false;

	assert(file != NULL);

	if ((fd = open(file, O_RDONLY)) == -1) {
		free(buf);
		return NULL;
	}
	memset(&st, 0, sizeof(st));
	if (fstat(fd, &st) == -1) {
		(void)close(fd);
		return NULL;
	}
	if (st.st_size > SSIZE_MAX - 1) {
		(void)close(fd);
		return NULL;
	}

	mapsize = ((size_t)st.st_size + pgmask) & ~pgmask;
	if (mapsize < (size_t)st.st_size) {
		(void)close(fd);
		return NULL;
	}
	/*
	 * If the file length is an integral number of pages, then we
	 * need to map a guard page at the end in order to provide the
	 * necessary NUL-termination of the buffer.
	 */
	if ((st.st_size & pgmask) == 0)
		need_guard = true;

	buf = mmap(NULL, need_guard ? mapsize + pgsize : mapsize,
		PROT_READ, MAP_PRIVATE, fd, 0);
	(void)close(fd);
	if (buf == MAP_FAILED)
		return NULL;

	if (SHA256(buf, st.st_size, digest) == NULL) {
		munmap(buf, mapsize);
		return NULL;
	}
	munmap(buf, mapsize);
	digest2string(digest, hash, SHA256_DIGEST_LENGTH);

	return strdup(hash);
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

const char *
xbps_file_hash_dictionary(prop_dictionary_t d,
			  const char *key,
			  const char *file)
{
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *curfile = NULL, *sha256 = NULL;

	assert(prop_object_type(d) == PROP_TYPE_DICTIONARY);
	assert(key != NULL);
	assert(file != NULL);

	iter = xbps_array_iter_from_dict(d, key);
	if (iter == NULL) {
		errno = ENOENT;
		return NULL;
	}
	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj,
		    "file", &curfile);
		if (strcmp(file, curfile) == 0) {
			/* file matched */
			prop_dictionary_get_cstring_nocopy(obj,
			    "sha256", &sha256);
			break;
		}
	}
	prop_object_iterator_release(iter);
	if (sha256 == NULL)
		errno = ENOENT;

	return sha256;
}

int
xbps_file_hash_check_dictionary(struct xbps_handle *xhp,
				prop_dictionary_t d,
				const char *key,
				const char *file)
{
	const char *sha256d = NULL;
	char *buf;
	int rv;

	assert(prop_object_type(d) == PROP_TYPE_DICTIONARY);
	assert(key != NULL);
	assert(file != NULL);

	if ((sha256d = xbps_file_hash_dictionary(d, key, file)) == NULL) {
		if (errno == ENOENT)
			return 1; /* no match, file not found */

		return -1; /* error */
	}

	if (strcmp(xhp->rootdir, "/") == 0) {
		rv = xbps_file_hash_check(file, sha256d);
	} else {
		buf = xbps_xasprintf("%s/%s", xhp->rootdir, file);
		assert(buf != NULL);
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
