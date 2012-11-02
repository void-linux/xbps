/*-
 * Copyright (c) 2008-2012 Juan Romero Pardines.
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

#ifdef HAVE_STRCASESTR
# define _GNU_SOURCE    /* for strcasestr(3) */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <fnmatch.h>

#include <xbps_api.h>
#include "defs.h"

struct repo_search_data {
	int npatterns;
	char **patterns;
	void *arg;
	size_t pkgver_len;
	size_t maxcols;
};

static int
repo_longest_pkgver(struct xbps_handle *xhp,
		    struct xbps_rpool_index *rpi,
		    void *arg,
		    bool *done)
{
	size_t *len = arg, olen = 0;

	(void)done;

	if (*len == 0) {
		*len = find_longest_pkgver(xhp, rpi->repo);
		return 0;
	}
	olen = find_longest_pkgver(xhp, rpi->repo);
	if (olen > *len)
		*len = olen;

	return 0;
}

static size_t
repo_find_longest_pkgver(struct xbps_handle *xhp)
{
	size_t len = 0;

	xbps_rpool_foreach(xhp, repo_longest_pkgver, &len);

	return len;
}

static int
show_pkg_namedesc(struct xbps_handle *xhp,
		  prop_object_t obj,
		  void *arg,
		  bool *loop_done)
{
	struct repo_search_data *rsd = arg;
	const char *pkgver, *pkgname, *desc, *arch, *inststr;
	char *tmp = NULL, *out = NULL;
	size_t x, len;
	int i;

	(void)xhp;
	(void)loop_done;

	prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	if (!xbps_pkg_arch_match(xhp, arch, NULL))
		return 0;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "short_desc", &desc);

	for (i = 0; i < rsd->npatterns; i++) {
		if ((xbps_pkgpattern_match(pkgver, rsd->patterns[i]) == 1) ||
		    (xbps_pkgpattern_match(desc, rsd->patterns[i]) == 1)  ||
		    (strcasecmp(pkgname, rsd->patterns[i]) == 0) ||
		    (strcasestr(pkgver, rsd->patterns[i])) ||
		    (strcasestr(desc, rsd->patterns[i]))) {
			tmp = calloc(1, rsd->pkgver_len + 1);
			assert(tmp);
			memcpy(tmp, pkgver, rsd->pkgver_len);
			for (x = strlen(tmp); x < rsd->pkgver_len; x++)
				tmp[x] = ' ';

			tmp[x] = '\0';
			if (xbps_pkgdb_get_pkgd_by_pkgver(xhp, pkgver))
				inststr = "[*]";
			else
				inststr = "[-]";

			len = strlen(inststr) + strlen(tmp) + strlen(desc) + 1;
			if (len > rsd->maxcols) {
				out = malloc(rsd->maxcols+1);
				assert(out);
				snprintf(out, rsd->maxcols-3, "%s %s %s",
				    inststr, tmp, desc);
				strncat(out, "...", rsd->maxcols);
				out[rsd->maxcols+1] = '\0';
				printf("%s\n", out);
				free(out);
			} else {
				printf("%s %s %s\n", inststr, tmp, desc);
			}
		}
	}

	return 0;
}
static int
repo_search_pkgs_cb(struct xbps_handle *xhp,
		    struct xbps_rpool_index *rpi,
		    void *arg,
		    bool *done)
{
	struct repo_search_data *rsd = arg;
	(void)done;

	(void)xbps_callback_array_iter(xhp, rpi->repo, show_pkg_namedesc, rsd);

	return 0;
}

int
repo_search(struct xbps_handle *xhp, int npatterns, char **patterns)
{
	struct repo_search_data rsd;
	int rv;

	rsd.npatterns = npatterns;
	rsd.patterns = patterns;
	rsd.pkgver_len = repo_find_longest_pkgver(xhp);
	rsd.maxcols = get_maxcols();

	rv = xbps_rpool_foreach(xhp, repo_search_pkgs_cb, &rsd);
	if (rv != 0 && rv != ENOTSUP)
		fprintf(stderr, "Failed to initialize rpool: %s\n",
		    strerror(rv));

	return rv;
}
