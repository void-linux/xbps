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

#ifdef HAVE_VASPRINTF
# define _GNU_SOURCE	/* for vasprintf(3) */
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fnmatch.h>
#include <sys/utsname.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "xbps_api_impl.h"

/**
 * @file lib/util.c
 * @brief Utility routines
 * @defgroup util Utility functions
 */
bool
xbps_check_is_repository_uri_remote(const char *uri)
{
	assert(uri != NULL);

	if ((strncmp(uri, "https://", 8) == 0) ||
	    (strncmp(uri, "http://", 7) == 0) ||
	    (strncmp(uri, "ftp://", 6) == 0))
		return true;

	return false;
}

int
xbps_check_is_installed_pkg_by_pattern(const char *pattern)
{
	prop_dictionary_t dict;
	pkg_state_t state;

	assert(pattern != NULL);

	dict = xbps_find_virtualpkg_dict_installed(pattern, true);
	if (dict == NULL) {
		dict = xbps_find_pkg_dict_installed(pattern, true);
		if (dict == NULL) {
			if (errno == ENOENT) {
				errno = 0;
				return 0; /* not installed */
			}
			return -1; /* error */
		}
	}
	/*
	 * Check that package state is fully installed, not
	 * unpacked or something else.
	 */
	if (xbps_pkg_state_dictionary(dict, &state) != 0) {
		prop_object_release(dict);
		return -1; /* error */
	}
	if (state != XBPS_PKG_STATE_INSTALLED) {
		prop_object_release(dict);
		return 0; /* not fully installed */
	}
	prop_object_release(dict);

	return 1;
}

bool
xbps_check_is_installed_pkg_by_name(const char *pkgname)
{
	prop_dictionary_t pkgd;

	assert(pkgname != NULL);

	if (((pkgd = xbps_find_pkg_dict_installed(pkgname, false)) == NULL) &&
	    ((pkgd = xbps_find_virtualpkg_dict_installed(pkgname, false)) == NULL))
		return false;

	prop_object_release(pkgd);
	return true;
}

const char *
xbps_pkg_version(const char *pkg)
{
	const char *p;

	if ((p = strrchr(pkg, '-')) == NULL)
		return NULL;

	if (strrchr(p, '_') == NULL)
		return NULL;

	return p + 1; /* skip first '_' */
}

const char *
xbps_pkg_revision(const char *pkg)
{
	const char *p;

	assert(pkg != NULL);

	/* Get the required revision */
	if ((p = strrchr(pkg, '_')) == NULL)
		return NULL;

	return p + 1; /* skip first '_' */
}

char *
xbps_pkg_name(const char *pkg)
{
	const char *p;
	char *buf;
	size_t len;

	if ((p = strrchr(pkg, '-')) == NULL)
		return NULL;

	if (strrchr(p, '_') == NULL)
		return NULL;

	len = strlen(pkg) - strlen(p) + 1;
	buf = malloc(len);
	assert(buf != NULL);
	strlcpy(buf, pkg, len);

	return buf;
}

char *
xbps_pkgpattern_name(const char *pkg)
{
	char *res, *pkgname;
	size_t len;

	assert(pkg != NULL);

	if ((res = strpbrk(pkg, "><*?[]")) == NULL)
		return NULL;

	len = strlen(pkg) - strlen(res) + 1;
	if (pkg[len-2] == '-')
		len--;

	pkgname = malloc(len);
	assert(pkgname != NULL);
	strlcpy(pkgname, pkg, len);

	return pkgname;
}

const char *
xbps_pkgpattern_version(const char *pkg)
{
	assert(pkg != NULL);

	return strpbrk(pkg, "><*?[]");
}

static char *
get_pkg_index_remote_plist(const char *uri, const char *plistf)
{
	struct xbps_handle *xhp;
	char *uri_fixed, *repodir;

	assert(uri != NULL);

	xhp = xbps_handle_get();
	uri_fixed = xbps_get_remote_repo_string(uri);
	if (uri_fixed == NULL)
		return NULL;

	repodir = xbps_xasprintf("%s/%s/%s", xhp->metadir, uri_fixed, plistf);
	free(uri_fixed);
	return repodir;
}

char *
xbps_pkg_index_plist(const char *uri)
{
	assert(uri != NULL);

	if (xbps_check_is_repository_uri_remote(uri))
		return get_pkg_index_remote_plist(uri, XBPS_PKGINDEX);

	return xbps_xasprintf("%s/%s", uri, XBPS_PKGINDEX);
}

char *
xbps_pkg_index_files_plist(const char *uri)
{
	assert(uri != NULL);
	if (xbps_check_is_repository_uri_remote(uri))
		return get_pkg_index_remote_plist(uri, XBPS_PKGINDEX_FILES);

	return xbps_xasprintf("%s/%s", uri, XBPS_PKGINDEX_FILES);
}

char *
xbps_path_from_repository_uri(prop_dictionary_t pkg_repod, const char *repoloc)
{
	struct xbps_handle *xhp;
	const char *filen, *arch;
	char *lbinpkg = NULL;

	assert(prop_object_type(pkg_repod) == PROP_TYPE_DICTIONARY);
	assert(repoloc != NULL);

	if (!prop_dictionary_get_cstring_nocopy(pkg_repod,
	    "filename", &filen))
		return NULL;

	xhp = xbps_handle_get();
	/*
	 * First check if binpkg is available in cachedir.
	 */
	lbinpkg = xbps_xasprintf("%s/%s", xhp->cachedir, filen);
	if (lbinpkg == NULL)
		return NULL;

	if (access(lbinpkg, R_OK) == 0)
		return lbinpkg;

	free(lbinpkg);
	if (!prop_dictionary_get_cstring_nocopy(pkg_repod,
	    "architecture", &arch))
		return NULL;
	/*
	 * Local and remote repositories use the same path.
	 */
	return xbps_xasprintf("%s/%s/%s", repoloc, arch, filen);
}

bool
xbps_pkg_has_rundeps(prop_dictionary_t pkgd)
{
	prop_array_t array;

	assert(prop_object_type(pkgd) == PROP_TYPE_DICTIONARY);

	array = prop_dictionary_get(pkgd, "run_depends");
	if ((prop_object_type(array) == PROP_TYPE_ARRAY) &&
	     prop_array_count(array) > 0)
		return true;

	return false;
}

bool
xbps_pkg_arch_match(const char *orig, const char *target)
{
	struct utsname un;

	if (target == NULL) {
		uname(&un);
		if (strcmp(orig, "noarch") && strcmp(orig, un.machine))
			return false;
	} else {
		if (strcmp(orig, "noarch") && strcmp(orig, target))
			return false;
	}
	return true;
}

char *
xbps_xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *buf;

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) == -1) {
		va_end(ap);
		return NULL;
	}
	va_end(ap);

	return buf;
}

/*
 * Match pkg against pattern, return 1 if matching, 0 otherwise or -1 on error.
 */
int
xbps_pkgpattern_match(const char *pkg, const char *pattern)
{
	/* simple match on "pkg" against "pattern */
	if (strcmp(pattern, pkg) == 0)
		return 1;

	/* perform relational dewey match on version number */
	if (strpbrk(pattern, "<>") != NULL)
		return dewey_match(pattern, pkg);

	/* glob match */
	if (strpbrk(pattern, "*?[]") != NULL)
		if (fnmatch(pattern, pkg, FNM_PERIOD) == 0)
			return 1;

	/* no match */
	return 0;
}
