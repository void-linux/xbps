/*-
 * Copyright (c) 2008-2013 Juan Romero Pardines.
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "xbps_api_impl.h"

struct thread_data {
	pthread_t thread;
	pthread_mutex_t *mtx;
	xbps_array_t array;
	xbps_dictionary_t dict;
	struct xbps_handle *xhp;
	unsigned int start;
	unsigned int end;
	int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *);
	void *fn_arg;
};

/**
 * @file lib/plist.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
static void *
array_foreach_thread(void *arg)
{
	xbps_object_t obj, pkgd;
	struct thread_data *thd = arg;
	const char *key;
	int rv;
	bool loop_done = false;

	/* process pkgs from start until end */
	for (unsigned int i = thd->start; i < thd->end; i++) {
		if (thd->mtx)
			pthread_mutex_lock(thd->mtx);

		obj = xbps_array_get(thd->array, i);
		if (xbps_object_type(thd->dict) == XBPS_TYPE_DICTIONARY) {
			pkgd = xbps_dictionary_get_keysym(thd->dict, obj);
			key = xbps_dictionary_keysym_cstring_nocopy(obj);
		} else {
			pkgd = obj;
			key = NULL;
		}
		if (thd->mtx)
			pthread_mutex_unlock(thd->mtx);

		rv = (*thd->fn)(thd->xhp, pkgd, key, thd->fn_arg, &loop_done);
		if (rv != 0 || loop_done)
			break;
	}
	return NULL;
}

int
xbps_array_foreach_cb_multi(struct xbps_handle *xhp,
	xbps_array_t array,
	xbps_dictionary_t dict,
	int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
	void *arg)
{
	struct thread_data *thd;
	pthread_mutex_t mtx;
	unsigned int arraycount, slicecount, pkgcount;
	int rv = 0, maxthreads;

	assert(fn != NULL);

	if (xbps_object_type(array) != XBPS_TYPE_ARRAY)
		return 0;

	arraycount = xbps_array_count(array);
	if (arraycount == 0)
		return 0;

	maxthreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (maxthreads == 1) /* use single threaded routine */
		return xbps_array_foreach_cb(xhp, array, dict, fn, arg);

	thd = calloc(maxthreads, sizeof(*thd));
	assert(thd);
	slicecount = arraycount / maxthreads;
	pkgcount = 0;
	pthread_mutex_init(&mtx, NULL);

	for (int i = 0; i < maxthreads; i++) {
		thd[i].mtx = &mtx;
		thd[i].array = array;
		thd[i].dict = dict;
		thd[i].xhp = xhp;
		thd[i].fn = fn;
		thd[i].fn_arg = arg;
		thd[i].start = pkgcount;
		if (i + 1 >= maxthreads)
			thd[i].end = arraycount;
		else
			thd[i].end = pkgcount + slicecount;
		pthread_create(&thd[i].thread, NULL,
		    array_foreach_thread, &thd[i]);
		pkgcount += slicecount;
	}
	/* wait for all threads */
	for (int i = 0; i < maxthreads; i++)
		rv = pthread_join(thd[i].thread, NULL);

	pthread_mutex_destroy(&mtx);
	free(thd);

	return rv;
}

int
xbps_array_foreach_cb(struct xbps_handle *xhp,
	xbps_array_t array,
	xbps_dictionary_t dict,
	int (*fn)(struct xbps_handle *, xbps_object_t, const char *, void *, bool *),
	void *arg)
{
	xbps_dictionary_t pkgd;
	xbps_object_t obj;
	const char *key;
	int rv = 0;
	bool loop_done = false;

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		if (xbps_object_type(dict) == XBPS_TYPE_DICTIONARY) {
			pkgd = xbps_dictionary_get_keysym(dict, obj);
			key = xbps_dictionary_keysym_cstring_nocopy(obj);
		} else {
			pkgd = obj;
			key = NULL;
		}
		rv = (*fn)(xhp, pkgd, key, arg, &loop_done);
		if (rv != 0 || loop_done)
			break;
	}
	return rv;
}

xbps_object_iterator_t
xbps_array_iter_from_dict(xbps_dictionary_t dict, const char *key)
{
	xbps_array_t array;

	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(key != NULL);

	array = xbps_dictionary_get(dict, key);
	if (xbps_object_type(array) != XBPS_TYPE_ARRAY) {
		errno = EINVAL;
		return NULL;
	}

	return xbps_array_iterator(array);
}

static int
array_replace_dict(xbps_array_t array,
		   xbps_dictionary_t dict,
		   const char *str,
		   bool bypattern)
{
	xbps_object_t obj;
	const char *curpkgver;
	char *curpkgname;

	assert(xbps_object_type(array) == XBPS_TYPE_ARRAY);
	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(str != NULL);

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		obj = xbps_array_get(array, i);
		if (obj == NULL)
			continue;
		if (bypattern) {
			/* pkgpattern match */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curpkgver);
			if (xbps_pkgpattern_match(curpkgver, str)) {
				if (!xbps_array_set(array, i, dict))
					return EINVAL;

				return 0;
			}
		} else {
			/* pkgname match */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "pkgver", &curpkgver);
			curpkgname = xbps_pkg_name(curpkgver);
			assert(curpkgname);
			if (strcmp(curpkgname, str) == 0) {
				if (!xbps_array_set(array, i, dict)) {
					free(curpkgname);
					return EINVAL;
				}
				free(curpkgname);
				return 0;
			}
			free(curpkgname);
		}
	}
	/* no match */
	return ENOENT;
}

int HIDDEN
xbps_array_replace_dict_by_name(xbps_array_t array,
				xbps_dictionary_t dict,
				const char *pkgver)
{
	return array_replace_dict(array, dict, pkgver, false);
}

int HIDDEN
xbps_array_replace_dict_by_pattern(xbps_array_t array,
				   xbps_dictionary_t dict,
				   const char *pattern)
{
	return array_replace_dict(array, dict, pattern, true);
}
