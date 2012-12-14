/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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

#include <sys/stat.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>
#include <pthread.h>

#include <xbps_api.h>
#include "defs.h"

struct thread_data {
	pthread_t thread;
	prop_dictionary_t idx;
	prop_array_t result;
	struct xbps_handle *xhp;
	unsigned int start;
	unsigned int end;
	int thread_num;
};

static void *
cleaner_thread(void *arg)
{
	prop_object_t obj;
	prop_dictionary_t pkgd;
	prop_array_t array;
	struct thread_data *thd = arg;
	const char *pkgver, *filen, *sha256;
	unsigned int i;

	/* process pkgs from start until end */
	array = prop_dictionary_all_keys(thd->idx);

	for (i = thd->start; i < thd->end; i++) {
		obj = prop_array_get(array, i);
		pkgd = prop_dictionary_get_keysym(thd->idx, obj);
		prop_dictionary_get_cstring_nocopy(pkgd, "filename", &filen);
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		xbps_dbg_printf(thd->xhp, "thread[%d] checking %s\n",
		    thd->thread_num, pkgver);
		if (access(filen, R_OK) == -1) {
			/*
			 * File cannot be read, might be permissions,
			 * broken or simply unexistent; either way, remove it.
			 */
			prop_array_add_cstring_nocopy(thd->result, pkgver);
			continue;
		}
		/*
		 * File can be read; check its hash.
		 */
		prop_dictionary_get_cstring_nocopy(pkgd,
		    "filename-sha256", &sha256);
		if (xbps_file_hash_check(filen, sha256) != 0)
			prop_array_add_cstring_nocopy(thd->result, pkgver);
	}
	prop_object_release(array);

	return NULL;
}

/*
 * Removes stalled pkg entries in repository's index.plist file, if any
 * binary package cannot be read (unavailable, not enough perms, etc).
 */
int
index_clean(struct xbps_handle *xhp, const char *repodir)
{
	struct thread_data *thd;
	prop_dictionary_t idx, idxfiles;
	const char *keyname;
	char *plist, *plistf, *pkgname;
	size_t x, pkgcount, slicecount;
	int i, maxthreads, rv = 0;
	bool flush = false;

	plist = xbps_pkg_index_plist(xhp, repodir);
	assert(plist);
	plistf = xbps_pkg_index_files_plist(xhp, repodir);
	assert(plistf);

	idx = prop_dictionary_internalize_from_zfile(plist);
	if (idx == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "index: cannot read `%s': %s\n",
			    plist, strerror(errno));
			free(plist);
			return -1;
		} else {
			free(plist);
			return 0;
		}
	}
	idxfiles = prop_dictionary_internalize_from_zfile(plistf);
	if (idxfiles == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "index: cannot read `%s': %s\n",
			    plistf, strerror(errno));
			rv = -1;
			goto out;
		} else {
			goto out;
		}
	}
	if (chdir(repodir) == -1) {
		fprintf(stderr, "index: cannot chdir to %s: %s\n",
		    repodir, strerror(errno));
		rv = -1;
		goto out;
	}
	printf("Cleaning `%s' index, please wait...\n", repodir);

	maxthreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
	thd = calloc(maxthreads, sizeof(*thd));

	slicecount = prop_dictionary_count(idx) / maxthreads;
	pkgcount = 0;

	for (i = 0; i < maxthreads; i++) {
		thd[i].thread_num = i;
		thd[i].idx = idx;
		thd[i].result = prop_array_create();
		thd[i].xhp = xhp;
		thd[i].start = pkgcount;
		if (i + 1 >= maxthreads)
			thd[i].end = prop_dictionary_count(idx);
		else
			thd[i].end = pkgcount + slicecount;
		pthread_create(&thd[i].thread, NULL, cleaner_thread, &thd[i]);
		pkgcount += slicecount;
	}
	/* wait for all threads */
	for (i = 0; i < maxthreads; i++)
		pthread_join(thd[i].thread, NULL);

	for (i = 0; i < maxthreads; i++) {
		if (!prop_array_count(thd[i].result))
			continue;
		for (x = 0; x < prop_array_count(thd[i].result); x++) {
			prop_array_get_cstring_nocopy(thd[i].result,
			    x, &keyname);
			printf("index: removed entry %s\n", keyname);
			pkgname = xbps_pkg_name(keyname);
			prop_dictionary_remove(idx, pkgname);
			prop_dictionary_remove(idxfiles, pkgname);
			free(pkgname);
			flush = true;
		}
		prop_object_release(thd[i].result);
	}
	free(thd);
	if (!flush)
		goto out;

	if (!prop_dictionary_externalize_to_zfile(idx, plist) &&
	    !prop_dictionary_externalize_to_zfile(idxfiles, plistf))
		fprintf(stderr, "index: failed to externalize %s: %s\n",
		    plist, strerror(errno));

out:
	printf("index: %u packages registered.\n",
	    prop_dictionary_count(idx));
	printf("index-files: %u packages registered.\n",
	    prop_dictionary_count(idxfiles));

	if (plist)
		free(plist);
	if (plistf)
		free(plistf);
	if (idx)
		prop_object_release(idx);
	if (idxfiles)
		prop_object_release(idxfiles);

	return rv;
}
