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

int
xbps_alternative_link(const char *alternative,
		char *path, size_t pathsz,
		char *target, size_t targetsz)
{
	const char *d = strchr(alternative, ':');
	if (!d || d == alternative)
		return -EINVAL;

	assert(path);
	if (alternative[0] == '/') {
		if ((size_t)(d-alternative) >= pathsz)
			return -ENOBUFS;
		strncpy(path, alternative, d-alternative);
		path[d-alternative] = '\0';
	} else {
		const char *p = strrchr(d+1, '/');
		if (!p)
			return -EINVAL;
		if ((size_t)((p-d)+(p-alternative)-1) >= pathsz)
			return -ENOBUFS;
		strncpy(path, d+1, p-d);
		strncpy(path+(p-d), alternative, d-alternative);
		path[(p-d)+(d-alternative)] = '\0';
	}
	if (target != NULL) {
		if (d[1] != '/') {
			if (xbps_strlcpy(target, d+1, targetsz) >= targetsz)
				return -ENOBUFS;
		} else if (xbps_path_rel(target, targetsz, path, d+1) == -1) {
			return -errno;
		}
	}
	return 0;
}

static int
remove_symlinks(struct xbps_handle *xhp, xbps_array_t a, const char *grname)
{
	char path[PATH_MAX];
	struct stat st;

	for (unsigned int i = 0; i < xbps_array_count(a); i++) {
		const char *alternative;
		int r;

		xbps_array_get_cstring_nocopy(a, i, &alternative);

		r = xbps_alternative_link(alternative, path, sizeof(path), NULL, 0);
		if (r < 0) {
			/* XXX: print error, but don't abort transaction */
			continue;
		}
		if (xbps_path_prepend(path, sizeof(path), xhp->rootdir) == -1) {
			/* XXX: print error, but don't abort transaction */
			continue;
		}

		if (lstat(path, &st) == -1 || !S_ISLNK(st.st_mode))
			continue;

		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_LINK_REMOVED, 0, NULL,
		    "Removing '%s' alternatives group symlink: %s", grname, path);
		unlink(path);
	}

	return 0;
}

static int
create_symlinks(struct xbps_handle *xhp, xbps_array_t a, const char *grname)
{
	char path[PATH_MAX];
	char target[PATH_MAX];
	char dir[PATH_MAX];
	int rv = 0;

	for (unsigned int i = 0; i < xbps_array_count(a); i++) {
		const char *alt;
		char *p;
		int r;
		xbps_array_get_cstring_nocopy(a, i, &alt);

		r = xbps_alternative_link(alt, path, sizeof(path), target, sizeof(target));
		if (r < 0) {
			/* XXX: print error, but don't abort transaction */
			continue;
		}
		if (xbps_path_prepend(path, sizeof(path), xhp->rootdir) == -1) {
			/* XXX: print error, but don't abort transaction */
			continue;
		}

		p = strrchr(path, '/');
		if (!p) {
			/* XXX: print error, but don't abort transaction */
			continue;
		}
		strncpy(dir, path, p-path);
		dir[p-path] = '\0';
		/* create target directory, necessary for dangling symlinks */
		if (strcmp(dir, ".") && xbps_mkpath(dir, 0755) && errno != EEXIST) {
			rv = errno;
			xbps_dbg_printf(xhp,
			    "failed to create target dir '%s' for group '%s': %s\n",
			    dir, grname, strerror(errno));
			continue;
		}
		if (xbps_path_append(dir, sizeof(dir), target) == -1) {
			rv = errno;
			xbps_dbg_printf(xhp,
			    "failed to create target symlink\n");
			continue;
		}
		p= strrchr(dir, '/');
		if (!p) {
			rv = EINVAL;
			continue;
		}
		*p = '\0';
		/* create link directory, necessary for dangling symlinks */
		if (strcmp(dir, ".") && xbps_mkpath(dir, 0755) && errno != EEXIST) {
			rv = errno;
			xbps_dbg_printf(xhp,
			    "failed to create symlink dir '%s' for group '%s': %s\n",
			    dir, grname, strerror(errno));
			continue;
		}

		/* skip the rootdir in the status callback */
		p = path;
		if (strcmp(xhp->rootdir, "/") != 0)
			p += strlen(xhp->rootdir);
		xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_LINK_ADDED, 0, NULL,
		    "Creating '%s' alternatives group symlink: %s -> %s",
		    grname, p, target);

		(void) unlink(path);
		if (symlink(target, path) != 0) {
			rv = errno;
			xbps_error_printf(
			    "Failed to create alternative symlink '%s' for group '%s': %s\n",
			    path, grname, strerror(errno));
		}
	}

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

static int
switch_alt_group(struct xbps_handle *xhp, const char *grpn, const char *pkgn,
		xbps_dictionary_t *pkg_alternatives)
{
	xbps_dictionary_t curpkgd, pkgalts;

	curpkgd = xbps_pkgdb_get_pkg(xhp, pkgn);
	assert(curpkgd);

	xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_SWITCHED, 0, NULL,
		"Switched '%s' alternatives group to '%s'", grpn, pkgn);
	pkgalts = xbps_dictionary_get(curpkgd, "alternatives");
	if (pkg_alternatives) *pkg_alternatives = pkgalts;
	return create_symlinks(xhp, xbps_dictionary_get(pkgalts, grpn), grpn);
}

int HIDDEN
xbps_alternatives_unregister(struct xbps_handle *xhp, xbps_dictionary_t pkgd, bool update)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives;
	const char *pkgver, *pkgname;
	int rv = 0;

	assert(xhp);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL)
		return 0;

	pkg_alternatives = xbps_dictionary_get(pkgd, "alternatives");
	if (!xbps_dictionary_count(pkg_alternatives))
		return 0;

	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		bool current = false;
		const char *first = NULL, *keyname;

		keysym = xbps_array_get(allkeys, i);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array = xbps_dictionary_get(alternatives, keyname);
		if (array == NULL)
			continue;

		if (!xbps_array_get_cstring_nocopy(array, 0, &first)) {
			/* XXX: does this need to be handled? */
			continue;
		}
		if (strcmp(pkgname, first) == 0) {
			/* this pkg is the current alternative for this group */
			current = true;
#if 0
			/* this is handled by obsolete file removal */
			rv = remove_symlinks(xhp,
				xbps_dictionary_get(pkg_alternatives, keyname),
				keyname);
			if (rv != 0)
				break;
#endif
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
		if (switch_alt_group(xhp, keyname, first, &pkg_alternatives) != 0)
			break;
	}
	xbps_object_release(allkeys);

	return rv;
}

/*
 * Prune the alternatives group from the db. This will first unregister
 * it for the package and if there's no other package left providing the
 * same, also ditch the whole group. When this is called, it is guaranteed
 * that what is happening is an upgrade, because it's only invoked when
 * the repo and installed alternatives sets differ for a specific package.
 */
static void
prune_altgroup(struct xbps_handle *xhp, xbps_dictionary_t repod,
		const char *pkgname, const char *pkgver, const char *keyname)
{
	const char *newpkg = NULL, *curpkg = NULL;
	xbps_array_t array;
	xbps_dictionary_t alternatives;
	xbps_string_t kstr;
	unsigned int grp_count;
	bool current = false;

	xbps_set_cb_state(xhp, XBPS_STATE_ALTGROUP_REMOVED, 0, NULL,
		"%s: unregistered '%s' alternatives group", pkgver, keyname);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	assert(alternatives);
	array = xbps_dictionary_get(alternatives, keyname);

	/* if using alt group from another package, we won't switch anything */
	xbps_array_get_cstring_nocopy(array, 0, &curpkg);
	current = (strcmp(pkgname, curpkg) == 0);

	/* actually prune the alt group for the current package */
	xbps_remove_string_from_array(array, pkgname);
	grp_count = xbps_array_count(array);
	if (grp_count == 0) {
		/* it was the last one, ditch the whole thing */
		xbps_dictionary_remove(alternatives, keyname);
		return;
	}
	if (!current) {
		/* not the last one, and ours wasn't the one being used */
		return;
	}

	if (xbps_array_count(xbps_dictionary_get(repod, "run_depends")) == 0 &&
	    xbps_array_count(xbps_dictionary_get(repod, "shlib-requires")) == 0) {
		/*
		 * Empty dependencies indicate a removed package (pure meta),
		 * use the first available group after ours has been pruned
		 */
		xbps_array_get_cstring_nocopy(array, 0, &newpkg);
		switch_alt_group(xhp, keyname, newpkg, NULL);
		return;
	}

	/*
	 * Use the last group, as this indicates that a transitional metapackage
	 * is replacing the original and therefore a new package has registered
	 * a replacement group, which should be last in the array (most recent).
	 */
	xbps_array_get_cstring_nocopy(array, grp_count - 1, &newpkg);

	/* put the new package as head */
	kstr = xbps_string_create_cstring(newpkg);
	xbps_remove_string_from_array(array, newpkg);
	xbps_array_add_first(array, kstr);
	xbps_array_get_cstring_nocopy(array, 0, &newpkg);
	xbps_object_release(kstr);

	switch_alt_group(xhp, keyname, newpkg, NULL);
}


static void
remove_obsoletes(struct xbps_handle *xhp, const char *pkgname, const char *pkgver,
		xbps_dictionary_t pkgdb_alts, xbps_dictionary_t repod)
{
	xbps_array_t allkeys;
	xbps_dictionary_t pkgd, pkgd_alts, repod_alts;

	pkgd = xbps_pkgdb_get_pkg(xhp, pkgname);
	if (xbps_object_type(pkgd) != XBPS_TYPE_DICTIONARY) {
		return;
	}

	pkgd_alts = xbps_dictionary_get(pkgd, "alternatives");
	repod_alts = xbps_dictionary_get(repod, "alternatives");

	if (xbps_object_type(pkgd_alts) != XBPS_TYPE_DICTIONARY) {
		return;
	}

	allkeys = xbps_dictionary_all_keys(pkgd_alts);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array, array2, array_repo;
		xbps_object_t keysym;
		const char *keyname, *first = NULL;

		keysym = xbps_array_get(allkeys, i);
		array = xbps_dictionary_get_keysym(pkgd_alts, keysym);
		keyname = xbps_dictionary_keysym_cstring_nocopy(keysym);

		array_repo = xbps_dictionary_get(repod_alts, keyname);
		if (!xbps_array_equals(array, array_repo)) {
			/*
			 * Check if current provider in pkgdb is this pkg.
			 */
			array2 = xbps_dictionary_get(pkgdb_alts, keyname);
			if (array2) {
				xbps_array_get_cstring_nocopy(array2, 0, &first);
				if (strcmp(pkgname, first) == 0) {
#if 0
					remove_symlinks(xhp, array_repo, keyname);
#endif
				}
			}
		}
		/*
		 * There is nothing left in the alternatives group, which means
		 * the package is being upgraded and is removing it; if we don't
		 * prune it, the system will keep it set after removal of its
		 * parent package, but it will be empty and invalid...
		 */
		if (xbps_array_count(array_repo) == 0) {
			prune_altgroup(xhp, repod, pkgname, pkgver, keyname);
		}
	}
	xbps_object_release(allkeys);
}

int HIDDEN
xbps_alternatives_register(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	xbps_array_t allkeys;
	xbps_dictionary_t alternatives, pkg_alternatives;
	const char *pkgver, *pkgname;
	int rv = 0;

	assert(xhp);
	assert(xhp->pkgdb);

	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	if (alternatives == NULL) {
		alternatives = xbps_dictionary_create();
		xbps_dictionary_set(xhp->pkgdb, "_XBPS_ALTERNATIVES_", alternatives);
		xbps_object_release(alternatives);
	}
	alternatives = xbps_dictionary_get(xhp->pkgdb, "_XBPS_ALTERNATIVES_");
	assert(alternatives);

	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgver", &pkgver);
	xbps_dictionary_get_cstring_nocopy(pkg_repod, "pkgname", &pkgname);

	/*
	 * Compare alternatives from pkgdb and repo and then remove obsolete
	 * symlinks, also remove obsolete (empty) alternatives groups.
	 */
	remove_obsoletes(xhp, pkgname, pkgver, alternatives, pkg_repod);

	pkg_alternatives = xbps_dictionary_get(pkg_repod, "alternatives");
	if (!xbps_dictionary_count(pkg_alternatives))
		return 0;

	allkeys = xbps_dictionary_all_keys(pkg_alternatives);
	for (unsigned int i = 0; i < xbps_array_count(allkeys); i++) {
		xbps_array_t array;
		xbps_object_t keysym;
		const char *keyname, *first = NULL;

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

	return rv;
}
