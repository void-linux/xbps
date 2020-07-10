/*-
 * Copyright (c) 2015 Juan Romero Pardines.
 * Copyright (c) 2019 Duncan Overbruck <mail@duncano.de>.
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

#include <sys/param.h>

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbps.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check alternative symlinks for set alternative groups.
 *
 * returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */

static const char *
normpath(char *path)
{
	char *seg, *p;

	for (p = path, seg = NULL; *p; p++) {
		if (strncmp(p, "/../", 4) == 0 || strncmp(p, "/..", 4) == 0) {
			memmove(seg ? seg : p, p+3, strlen(p+3) + 1);
			return normpath(path);
		} else if (strncmp(p, "/./", 3) == 0 || strncmp(p, "/.", 3) == 0) {
			memmove(p, p+2, strlen(p+2) + 1);
		} else if (strncmp(p, "//", 2) == 0 || strncmp(p, "/", 2) == 0) {
			memmove(p, p+1, strlen(p+1) + 1);
		}
		if (*p == '/')
			seg = p;
	}
	return path;
}

static char *
relpath(char *from, char *to)
{
	int up;
	char *p = to, *rel;

	assert(from[0] == '/');
	assert(to[0] == '/');
	normpath(from);
	normpath(to);

	for (; *from == *to && *to; from++, to++) {
		if (*to == '/')
			p = to;
	}

	for (up = -1, from--; from && *from; from = strchr(from + 1, '/'), up++);

	rel = calloc(3 * up + strlen(p), sizeof(char));

	while (up--)
		strcat(rel, "../");
	if (*p)
		strcat(rel, p+1);
	return rel;
}

static int
check_symlinks(struct xbps_handle *xhp, const char *pkgname, xbps_array_t a,
	const char *grname)
{
	int rv = 0;
	ssize_t l;
	unsigned int i, n;
	char *alternative, *tok1, *tok2, *linkpath, *target, *dir, *p;
	char path[PATH_MAX];

	n = xbps_array_count(a);

	for (i = 0; i < n; i++) {
		alternative = xbps_string_cstring(xbps_array_get(a, i));

		if (!(tok1 = strtok(alternative, ":")) ||
		    !(tok2 = strtok(NULL, ":"))) {
			free(alternative);
			return -1;
		}

		target = strdup(tok2);
		dir = dirname(tok2);

		/* add target dir to relative links */
		if (tok1[0] != '/')
			linkpath = xbps_xasprintf("%s/%s/%s", xhp->rootdir, dir, tok1);
		else
			linkpath = xbps_xasprintf("%s/%s", xhp->rootdir, tok1);

		if (target[0] == '/') {
			p = relpath(linkpath + strlen(xhp->rootdir), target);
			free(target);
			target = p;
		}

		if (strncmp(linkpath, "//", 2) == 0) {
			p = linkpath+1;
		} else {
			p = linkpath;
		}
		if ((l = readlink(linkpath, path, sizeof path)) == -1) {
			xbps_error_printf(
			    "%s: alternatives group %s symlink %s: %s\n",
			    pkgname, grname, p, strerror(errno));
			rv = 1;
		} else if (strncmp(path, target, l) != 0) {
			xbps_error_printf("%s: alternatives group %s symlink %s has wrong target.\n",
			    pkgname, grname, p);
			rv = 1;
		}
		free(alternative);
		free(target);
		free(linkpath);
	}

	return rv;
}

int
check_pkg_alternatives(struct xbps_handle *xhp, const char *pkgname, xbps_dictionary_t pkg_propsd)
{
	xbps_array_t allkeys, array;
	xbps_dictionary_t alternatives, pkg_alternatives;
	int rv = 0;

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL)
		return 0;

	pkg_alternatives = xbps_dictionary_get(pkg_propsd, "alternatives");
	if (!xbps_dictionary_count(pkg_alternatives))
		return 0;

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_object_t keysym;
		const char *first= NULL, *group;

		keysym = xbps_array_get(allkeys, i);
		group = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array = xbps_dictionary_get(alternatives, group);
		if (array == NULL)
			continue;

		/* if pkgname is the first entry its set as alternative */
		xbps_array_get_cstring_nocopy(array, 0, &first);
		if (strcmp(pkgname, first) != 0)
			continue;

		array = xbps_dictionary_get(pkg_alternatives, group);
		if (check_symlinks(xhp, pkgname, array, group) != 0)
			rv = 1;
	}
	xbps_object_release(allkeys);

	return rv;
}
