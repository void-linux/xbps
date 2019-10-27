/*-
 * Copyright (c) 2015-2019 Juan Romero Pardines.
 * Copyright (c) 2019 Duncan Overbruck.
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
#include <libgen.h>

#include "xbps_api_impl.h"

/**
 * @file lib/package_alternatives.c
 * @brief Alternatives generic routines
 * @defgroup alternatives Alternatives generic functions
 *
 * These functions implement the alternatives framework.
 */

static char *
left(const char *str)
{
	char *p;
	size_t len;

	p = strdup(str);
	len = strlen(p) - strlen(strchr(p, ':'));
	p[len] = '\0';

	return p;
}

static const char *
right(const char *str)
{
	return strchr(str, ':') + 1;
}

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
remove_symlinks(struct xbps_handle *xhp, xbps_array_t a, const char *grname)
{
	unsigned int i, cnt;
	struct stat st;

	cnt = xbps_array_count(a);
	for (i = 0; i < cnt; i++) {
		xbps_string_t str;
		char *l, *lnk;

		str = xbps_array_get(a, i);
		l = left(xbps_string_cstring_nocopy(str));
		assert(l);
		if (l[0] != '/') {
			const char *tgt;
			char *tgt_dup, *tgt_dir;
			tgt = right(xbps_string_cstring_nocopy(str));
			tgt_dup = strdup(tgt);
			assert(tgt_dup);
			tgt_dir = dirname(tgt_dup);
			lnk = xbps_xasprintf("%s%s/%s", xhp->rootdir, tgt_dir, l);
			free(tgt_dup);
		} else {
			lnk = xbps_xasprintf("%s%s", xhp->rootdir, l);
		}
		if (lstat(lnk, &st) == -1 || !S_ISLNK(st.st_mode)) {
			free(lnk);
			free(l);
			continue;
		}
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_LINK_REMOVED, 0, NULL,
		    "Removing '%s' alternatives group symlink: %s", grname, l);
		unlink(lnk);
		free(lnk);
		free(l);
	}

	return 0;
}

static int
create_symlinks(struct xbps_handle *xhp, xbps_array_t a, const char *grname)
{
	int rv;
	unsigned int i, n;
	char *alternative, *tok1, *tok2, *linkpath, *target, *dir, *p;

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

		/* create target directory, necessary for dangling symlinks */
		dir = xbps_xasprintf("%s/%s", xhp->rootdir, dir);
		if (strcmp(dir, ".") && xbps_mkpath(dir, 0755) && errno != EEXIST) {
			rv = errno;
			xbps_dbg_printf(xhp,
			    "failed to create target dir '%s' for group '%s': %s\n",
			    dir, grname, strerror(errno));
			free(dir);
			goto err;
		}
		free(dir);

		/* create link directory, necessary for dangling symlinks */
		p = strdup(linkpath);
		dir = dirname(p);
		if (strcmp(dir, ".") && xbps_mkpath(dir, 0755) && errno != EEXIST) {
			rv = errno;
			xbps_dbg_printf(xhp,
			    "failed to create symlink dir '%s' for group '%s': %s\n",
			    dir, grname, strerror(errno));
			free(p);
			goto err;
		}
		free(p);

		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_LINK_ADDED, 0, NULL,
		    "Creating '%s' alternatives group symlink: %s -> %s",
		    grname, tok1, target);

		if (target[0] == '/') {
			p = relpath(linkpath + strlen(xhp->rootdir), target);
			free(target);
			target = p;
		}

		unlink(linkpath);
		if ((rv = symlink(target, linkpath)) != 0) {
			xbps_dbg_printf(xhp,
			    "failed to create alt symlink '%s' for group '%s': %s\n",
			    linkpath, grname,  strerror(errno));
			goto err;
		}

		free(alternative);
		free(target);
		free(linkpath);
	}

	return 0;

err:
	free(alternative);
	free(target);
	free(linkpath);
	return rv;
}

int
xbps_alternatives_set(struct xbps_handle *xhp, const char *pkgname,
		const char *group)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives, pkgd, prevpkgd, prevpkg_alts;
	const char *pkgver = NULL, *prevpkgname = NULL;
	int rv = 0;

	assert(xhp);
	assert(pkgname);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL)
		return ENOENT;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
	if (pkgd == NULL)
		return ENOENT;

	pkg_alternatives = xbps_dictionary_get(pkgd, "alternatives");
	if (!xbps_dictionary_count(pkg_alternatives))
		return ENOENT;

	if (group && !xbps_dictionary_get(pkg_alternatives, group))
		return ENOENT;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		xbps_string_t kstr;
		const char *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		if (group && strcmp(keyname, group))
			continue;

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		/* remove symlinks from previous alternative */
		xbps_array_get_cstring_nocopy(array, 0, &prevpkgname);
		if (prevpkgname && strcmp(pkgname, prevpkgname) != 0) {
			if ((prevpkgd = xbps_pkgdb_get_pkg(xhp, prevpkgname)) &&
			    (prevpkg_alts = xbps_dictionary_get(prevpkgd, "alternatives")) &&
			    xbps_dictionary_count(prevpkg_alts)) {
				rv = remove_symlinks(xhp,
				    xbps_dictionary_get(prevpkg_alts, keyname),
				    keyname);
				if (rv != 0)
					break;
			}
		}

		/* put this alternative group at the head */
		xbps_remove_string_from_array(array, pkgname);
		kstr = xbps_string_create_cstring(pkgname);
		xbps_array_add_first(array, kstr);
		xbps_object_release(kstr);

		/* apply the alternatives group */
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_ADDED, 0, NULL,
		    "%s: applying '%s' alternatives group", pkgver, keyname);
		rv = create_symlinks(xhp, xbps_dictionary_get(pkg_alternatives, keyname), keyname);
		if (rv != 0 || group)
			break;
	}
	xbps_object_release(allkeys);
	return rv;
}

int
xbps_alternatives_unregister(struct xbps_handle *xhp, xbps_dictionary_t pkgd)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives;
	const char *pkgver;
	char *pkgname;
	bool update = false;
	int rv = 0;

	assert(xhp);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL)
		return 0;

	pkg_alternatives = xbps_dictionary_get(pkgd, "alternatives");
	if (!xbps_dictionary_count(pkg_alternatives))
		return 0;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	if ((pkgname = xbps_pkg_name(pkgver)) == NULL)
		return EINVAL;

	xbps_dictionary_get_bool(pkgd, "alternatives-update", &update);

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		xbps_dictionary_t curpkgd = pkgd;
		bool current = false;
		const char *first = NULL, *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		xbps_array_get_cstring_nocopy(array, 0, &first);
		if (strcmp(pkgname, first) == 0) {
			/* this pkg is the current alternative for this group */
			current = true;
			rv = remove_symlinks(xhp,
				xbps_dictionary_get(pkg_alternatives, keyname),
				keyname);
			if (rv != 0)
				break;
		}

		if (!update) {
			xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_REMOVED, 0, NULL,
			    "%s: unregistered '%s' alternatives group", pkgver, keyname);
			xbps_remove_string_from_array(array, pkgname);
			xbps_array_get_cstring_nocopy(array, 0, &first);
		}

		if (xbps_array_count(array) == 0) {
			xbps_dictionary_remove(alternatives, keyname);
			continue;
		}

		if (update || !current)
			continue;

		/* get the new alternative group package */
		curpkgd = xbps_pkgdb_get_pkg(xhp, first);
		assert(curpkgd);
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_SWITCHED, 0, NULL,
		    "Switched '%s' alternatives group to '%s'", keyname, first);
		pkg_alternatives = xbps_dictionary_get(curpkgd, "alternatives");
		rv = create_symlinks(xhp,
			xbps_dictionary_get(pkg_alternatives, keyname),
			keyname);
		if (rv != 0)
			break;
	}
	xbps_object_release(allkeys);
	free(pkgname);

	return rv;
}

static void
remove_obsoletes(struct xbps_handle *xhp, xbps_dictionary_t pkgd, xbps_dictionary_t repod)
{
	xbps_array_t allkeys;

	allkeys = xbps_dictionary_all_keys(pkgd);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array, array_repo;
		xbps_object_t keysym;
		const char *keyname;

		keysym = xbps_array_get(allkeys, i);
		array = xbps_dictionary_get_keysym(pkgd, keysym);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array_repo = xbps_dictionary_get(repod, keyname);
		if (!xbps_array_equals(array, array_repo)) {
			remove_symlinks(xhp, array, keyname);
		}
	}
	xbps_object_release(allkeys);
}

int
xbps_alternatives_register(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives, pkgd, pkgd_alts;
	const char *pkgver;
	char *pkgname;
	int rv = 0;

	assert(xhp);

	if (xhp->pkgdb == NULL)
		return EINVAL;

	pkg_alternatives = xbps_dictionary_get(pkg_repod, "alternatives");
	if (!xbps_dictionary_count(pkg_alternatives))
		return 0;

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL) {
		alternatives = xbps_dictionary_create();
		xbps_dictionary_set(xhp->pkgdb, "_XBPS_ALTERNATIVES_", alternatives);
		xbps_object_release(alternatives);
	}
	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	assert(alternatives);

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	pkgname = xbps_pkg_name(pkgver);
	if (pkgname == NULL)
		return EINVAL;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
	if (xbps_object_type(pkgd) == XBPS_TYPE_DICTIONARY) {
		/*
		 * Compare alternatives from pkgdb and repo and
		 * then remove obsolete symlinks.
		 */
		pkgd_alts = xbps_dictionary_get(pkgd, "alternatives");
		if (xbps_object_type(pkgd_alts) == XBPS_TYPE_DICTIONARY) {
			remove_obsoletes(xhp, pkgd_alts, pkg_alternatives);
		}
	}

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		const char *keyname, *first;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL) {
			array = xbps_array_create();
		} else {
			if (xbps_match_string_in_array(array, pkgname)) {
				xbps_array_get_cstring_nocopy(array, 0, &first);
				if (strcmp(pkgname, first)) {
					/* current alternative does not match */
					continue;
				}
				/* already registered, update symlinks */
				rv = create_symlinks(xhp,
					xbps_dictionary_get(pkg_alternatives, keyname),
					keyname);
				if (rv != 0)
					break;
			} else {
				/* not registered, add provider */
				xbps_array_add_cstring(array, pkgname);
				xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_ADDED, 0, NULL,
				    "%s: registered '%s' alternatives group", pkgver, keyname);
			}
			continue;
		}

		xbps_array_add_cstring(array, pkgname);
		xbps_dictionary_set(alternatives, keyname, array);
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_ADDED, 0, NULL,
		    "%s: registered '%s' alternatives group", pkgver, keyname);
		/* apply alternatives for this group */
		rv = create_symlinks(xhp,
			xbps_dictionary_get(pkg_alternatives, keyname),
			keyname);
		xbps_object_release(array);
		if (rv != 0)
			break;
	}
	xbps_object_release(allkeys);
	free(pkgname);

	return rv;
}
