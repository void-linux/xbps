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

#include <xbps_api.h>
#include "../xbps-bin/defs.h"
#include "defs.h"

int
show_pkg_info_from_repolist(struct xbps_handle *xhp,
			    const char *pattern,
			    const char *option)
{
	prop_dictionary_t pkgd;

	if (xbps_pkgpattern_version(pattern))
		pkgd = xbps_rpool_find_pkg(xhp, pattern, true, false);
	else
		pkgd = xbps_rpool_find_pkg(xhp, pattern, false, true);

	if (pkgd == NULL)
		return errno;

	if (option)
		show_pkg_info_one(pkgd, option);
	else
		show_pkg_info(pkgd);

	prop_object_release(pkgd);

	return 0;
}

int
show_pkg_deps_from_repolist(struct xbps_handle *xhp, const char *pattern)
{
	prop_dictionary_t pkgd;
	const char *ver, *repoloc;

	if (xbps_pkgpattern_version(pattern))
		pkgd = xbps_rpool_find_pkg(xhp, pattern, true, false);
	else
		pkgd = xbps_rpool_find_pkg(xhp, pattern, false, true);

	if (pkgd == NULL)
		return errno;

	prop_dictionary_get_cstring_nocopy(pkgd, "version", &ver);
	prop_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc);

	printf("Repository %s [pkgver: %s]\n", repoloc, ver);
	(void)xbps_callback_array_iter_in_dict(xhp, pkgd,
	    "run_depends", list_strings_sep_in_array, NULL);

	prop_object_release(pkgd);
	return 0;
}

int
show_pkg_namedesc(struct xbps_handle *xhp,
		  prop_object_t obj,
		  void *arg,
		  bool *loop_done)
{
	struct repo_search_data *rsd = arg;
	const char *pkgver, *pkgname, *desc, *arch, *inststr;
	char *tmp = NULL, *out = NULL;
	size_t i, x, len;

	(void)xhp;
	(void)loop_done;

	prop_dictionary_get_cstring_nocopy(obj, "architecture", &arch);
	if (!xbps_pkg_arch_match(xhp, arch, NULL))
		return 0;

	prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
	prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
	prop_dictionary_get_cstring_nocopy(obj, "short_desc", &desc);

	for (i = 1; i < (size_t)rsd->npatterns; i++) {
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
				inststr = "[*] ";
			else
				inststr = "[ ] ";

			len = strlen(inststr) + strlen(tmp) + strlen(desc) + 1;
			if (len > rsd->maxcols) {
				out = malloc(rsd->maxcols);
				assert(out);
				snprintf(out, rsd->maxcols-2, "%s%s %s",
				    inststr, tmp, desc);
				strncat(out, "...", rsd->maxcols);
				printf("%s\n", out);
				free(out);
			} else {
				printf("%s%s %s\n", inststr, tmp, desc);
			}
		}
	}

	return 0;
}
