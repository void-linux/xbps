/*-
 * Copyright (c) 2015 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

/*
 * XXX TODO: relative symlinks.
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

static int
remove_symlinks(struct xbps_handle *xhp, xbps_array_t a, const char *grname)
{
	unsigned int i, cnt;

	cnt = xbps_array_count(a);
	for (i = 0; i < cnt; i++) {
		xbps_string_t str;
		char *l, *lnk;

		str = xbps_array_get(a, i);
		l = left(xbps_string_cstring_nocopy(str));
		assert(l);
		lnk = xbps_xasprintf("%s%s", xhp->rootdir, l);
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
	unsigned int i, cnt;

	cnt = xbps_array_count(a);
	for (i = 0; i < cnt; i++) {
		xbps_string_t str;
		char *l, *lnk;
		const char *tgt;
		int rv;

		str = xbps_array_get(a, i);
		l = left(xbps_string_cstring_nocopy(str));
		assert(l);
		tgt = right(xbps_string_cstring_nocopy(str));
		assert(tgt);
		lnk = xbps_xasprintf("%s%s", xhp->rootdir, l);
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_LINK_ADDED, 0, NULL,
		    "Creating '%s' alternatives group symlink: %s -> %s", grname, l, tgt);
		unlink(lnk);
		if ((rv = symlink(tgt, lnk)) != 0) {
			xbps_dbg_printf(xhp, "failed to create alt symlink '%s'"
			    "for group '%s': %s\n", lnk, grname,
			    strerror(errno));
			free(lnk);
			free(l);
			return rv;
		}
		free(lnk);
		free(l);
	}

	return 0;
}

int
xbps_alternatives_set(struct xbps_handle *xhp, const char *pkgname,
		const char *group)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives, pkgd;
	const char *pkgver;
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

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		xbps_string_t kstr;
		const char *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		if (group && strcmp(keyname, group)) {
			rv = ENOENT;
			continue;
		}

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		/* put this alternative group at the head */
		xbps_remove_string_from_array(array, pkgname);
		kstr = xbps_string_create_cstring(pkgname);
		xbps_array_add_first(array, kstr);
		xbps_object_release(kstr);

		/* apply the alternatives group */
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_ADDED, 0, NULL,
		    "%s: applying '%s' alternatives group", pkgver, keyname);
		rv = create_symlinks(xhp, xbps_dictionary_get(pkg_alternatives, keyname), keyname);
		if (rv != 0)
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

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		const char *first = NULL, *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		xbps_array_get_cstring_nocopy(array, 0, &first);
		if (strcmp(pkgname, first) == 0) {
			/* this pkg is the current alternative for this group */
			rv = remove_symlinks(xhp,
				xbps_dictionary_get(pkg_alternatives, keyname),
				keyname);
			if (rv != 0)
				break;
		}
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_REMOVED, 0, NULL,
		    "%s: unregistered '%s' alternatives group", pkgver, keyname);
		xbps_remove_string_from_array(array, pkgname);
		if (xbps_array_count(array) == 0) {
			xbps_dictionary_remove(alternatives, keyname);
		} else {
			xbps_dictionary_t curpkgd;

			first = NULL;
			xbps_array_get_cstring_nocopy(array, 0, &first);
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

	}
	xbps_object_release(allkeys);
	free(pkgname);

	return rv;
}

int
xbps_alternatives_register(struct xbps_handle *xhp, xbps_dictionary_t pkgd)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives;
	const char *pkgver;
	char *pkgname;
	int rv = 0;

	assert(xhp);

	if (xhp->pkgdb == NULL)
		return EINVAL;

	pkg_alternatives = xbps_dictionary_get(pkgd, "alternatives");
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

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	pkgname = xbps_pkg_name(pkgver);
	if (pkgname == NULL)
		return EINVAL;

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		const char *keyname;
		bool alloc = false;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL) {
			alloc = true;
			array = xbps_array_create();
		}
		xbps_array_add_cstring(array, pkgname);
		xbps_dictionary_set(alternatives, keyname, array);
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_ADDED, 0, NULL,
		    "%s: registered '%s' alternatives group", pkgver, keyname);
		if (alloc) {
			/* apply alternatives for this group */
			rv = create_symlinks(xhp,
				xbps_dictionary_get(pkg_alternatives, keyname),
				keyname);
			xbps_object_release(array);
			if (rv != 0)
				break;
		}
	}
	xbps_object_release(allkeys);
	free(pkgname);

	return rv;
}
