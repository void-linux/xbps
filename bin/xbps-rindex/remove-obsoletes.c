/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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
	prop_array_t array;
	struct xbps_rindex *ri;
	unsigned int start;
	unsigned int end;
	int thread_num;
};

static int
remove_pkg(const char *repodir, const char *arch, const char *file)
{
	char *filepath;
	int rv;

	filepath = xbps_xasprintf("%s/%s/%s", repodir, arch, file);
	if (remove(filepath) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			fprintf(stderr, "xbps-rindex: failed to remove "
			    "package `%s': %s\n", file,
			    strerror(rv));
			free(filepath);
			return rv;
		}
	}
	free(filepath);

	filepath = xbps_xasprintf("%s/%s", repodir, file);
	if (remove(filepath) == -1) {
		if (errno != ENOENT) {
			rv = errno;
			fprintf(stderr, "xbps-rindex: failed to remove "
			    "package `%s': %s\n", file,
			    strerror(rv));
			free(filepath);
			return rv;
		}
	}
	free(filepath);

	return 0;
}

static void *
cleaner_thread(void *arg)
{
	prop_dictionary_t pkgd;
	struct thread_data *thd = arg;
	const char *binpkg, *pkgver, *arch;
	unsigned int i;
	int rv;

	/* process pkgs from start until end */
	for (i = thd->start; i < thd->end; i++) {
		prop_array_get_cstring_nocopy(thd->array, i, &binpkg);
		pkgd = xbps_get_pkg_plist_from_binpkg(binpkg, "./props.plist");
		if (pkgd == NULL) {
			rv = remove_pkg(thd->ri->uri, arch, binpkg);
			if (rv != 0) {
				prop_object_release(pkgd);
				continue;
			}
			printf("Removed broken package `%s'.\n", binpkg);
		}
		prop_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
		prop_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch);
		/* ignore pkgs from other archs */
		if (!xbps_pkg_arch_match(thd->ri->xhp, arch, NULL)) {
			prop_object_release(pkgd);
			continue;
		}
		xbps_dbg_printf(thd->ri->xhp, "thread[%d] checking %s (%s)\n",
		    thd->thread_num, pkgver, binpkg);
		/*
		 * If binpkg is not registered in index, remove binpkg.
		 */
		if (!xbps_rindex_get_pkg(thd->ri, pkgver)) {
			rv = remove_pkg(thd->ri->uri, arch, binpkg);
			if (rv != 0) {
				prop_object_release(pkgd);
				continue;
			}
			printf("Removed obsolete package `%s'.\n", binpkg);
		}
		prop_object_release(pkgd);
	}

	return NULL;
}

int
remove_obsoletes(struct xbps_handle *xhp, const char *repodir)
{
	prop_dictionary_t idx;
	prop_array_t array = NULL;
	struct xbps_rindex ri;
	struct thread_data *thd;
	DIR *dirp;
	struct dirent *dp;
	char *plist, *ext;
	int i, maxthreads, rv = 0;
	size_t slicecount, pkgcount;

	if ((plist = xbps_pkg_index_plist(xhp, repodir)) == NULL)
		return -1;

	idx = prop_dictionary_internalize_from_zfile(plist);
	if (idx == NULL) {
		if (errno != ENOENT) {
			fprintf(stderr, "xbps-rindex: cannot read `%s': %s\n",
			    plist, strerror(errno));
			return -1;
		} else
			return 0;
	}
	/* initialize repository index */
	ri.repod = idx;
	ri.uri = repodir;
	ri.xhp = xhp;

	if (chdir(repodir) == -1) {
		fprintf(stderr, "xbps-rindex: cannot chdir to %s: %s\n",
		    repodir, strerror(errno));
		return errno;
	}
	if ((dirp = opendir(repodir)) == NULL) {
		fprintf(stderr, "xbps-rindex: failed to open %s: %s\n",
		    repodir, strerror(errno));
		return errno;
	}
	while ((dp = readdir(dirp))) {
		if (strcmp(dp->d_name, "..") == 0)
			continue;
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".xbps"))
			continue;
		if (array == NULL)
			array = prop_array_create();

		prop_array_add_cstring(array, dp->d_name);
	}
	(void)closedir(dirp);

	maxthreads = (int)sysconf(_SC_NPROCESSORS_ONLN);
	thd = calloc(maxthreads, sizeof(*thd));

	slicecount = prop_array_count(array) / maxthreads;
	pkgcount = 0;

	for (i = 0; i < maxthreads; i++) {
		thd[i].thread_num = i;
		thd[i].array = array;
		thd[i].ri = &ri;
		thd[i].start = pkgcount;
		if (i + 1 >= maxthreads)
			thd[i].end = prop_array_count(array);
		else
			thd[i].end = pkgcount + slicecount;
		pthread_create(&thd[i].thread, NULL, cleaner_thread, &thd[i]);
		pkgcount += slicecount;
	}

	/* wait for all threads */
	for (i = 0; i < maxthreads; i++)
		pthread_join(thd[i].thread, NULL);

	return rv;
}
