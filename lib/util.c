/*-
 * Copyright (c) 2008-2014 Juan Romero Pardines.
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
#include <ctype.h>
#include <sys/utsname.h>

#include "xbps_api_impl.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

/**
 * @file lib/util.c
 * @brief Utility routines
 * @defgroup util Utility functions
 */
bool
xbps_repository_is_remote(const char *uri)
{
	assert(uri != NULL);

	if ((strncmp(uri, "http://", 7) == 0) ||
	    (strncmp(uri, "https://", 8) == 0) ||
	    (strncmp(uri, "ftp://", 6) == 0))
		return true;

	return false;
}

int
xbps_pkg_is_installed(struct xbps_handle *xhp, const char *pkg)
{
	xbps_dictionary_t dict;
	pkg_state_t state;

	assert(xhp);
	assert(pkg);

	if (((dict = xbps_pkgdb_get_virtualpkg(xhp, pkg)) == NULL) &&
	    ((dict = xbps_pkgdb_get_pkg(xhp, pkg)) == NULL))
		return 0; /* not installed */
	/*
	 * Check that package state is fully installed, not
	 * unpacked or something else.
	 */
	if (xbps_pkg_state_dictionary(dict, &state) != 0)
		return -1; /* error */
	if (state == XBPS_PKG_STATE_INSTALLED || state == XBPS_PKG_STATE_UNPACKED)
		return 1;

	return 0; /* not fully installed */
}

const char *
xbps_pkg_version(const char *pkg)
{
	const char *p;
	bool valid = false;

	if ((p = strrchr(pkg, '-')) == NULL)
		return NULL;

	for (unsigned int i = 0; i < strlen(p); i++) {
		if (p[i] == '_')
			break;
		if (isdigit((unsigned char)p[i]) && strchr(p, '_')) {
			valid = true;
			break;
		}
	}
	if (!valid)
		return NULL;

	return p + 1; /* skip first '-' */
}

char *
xbps_binpkg_pkgver(const char *pkg)
{
	const char *fname;
	char *p, *res;
	unsigned int i, idx = 0;
	bool valid = false;

	/* skip path if found, only interested in filename */
	if ((fname = strrchr(pkg, '/')))
		fname++;
	else
		fname = pkg;

	/* get the version component first */
	if ((p = strrchr(fname, '-')) == NULL)
		return NULL;

	for (i = 0; i < strlen(p); i++) {
		if (p[i] == '_')
			break;
		if (isdigit((unsigned char)p[i]) && strchr(p, '_')) {
			valid = true;
			break;
		}
	}
	if (!valid)
		return NULL;

	/*
	 * find the index until the architecture component:
	 * this assumes that revision contains 1 or 2 digits!
	 */
	for (i = 0; i < strlen(fname); i++) {
		/* revision with 1 digit: _[digit]. */
		if (fname[i] == '_' &&
		    isdigit((unsigned char)fname[i+1]) &&
		    fname[i+2] == '.') {
			idx = i+2;
			break;
		}
		/* revision with 2 digits: _[digit][digit]. */
		if (fname[i] == '_' &&
		    isdigit((unsigned char)fname[i+1]) &&
		    isdigit((unsigned char)fname[i+2]) &&
		    fname[i+3] == '.') {
			idx = i+3;
			break;
		}
	}
	if (!idx)
		return NULL;

	res = strdup(fname);
	assert(res);
	res[idx] = '\0';

	return res;
}

char *
xbps_binpkg_arch(const char *pkg)
{
	const char *p, *fname;
	char *pkgver, *res;

	if ((pkgver = xbps_binpkg_pkgver(pkg)) == NULL)
		return NULL;

	/* skip path if found, only interested in filename */
	if ((fname = strrchr(pkg, '/')))
		fname++;
	else
		fname = pkg;

	p = fname + strlen(pkgver);
	if (!p || p == '\0') {
		free(pkgver);
		return NULL;
	}
	if (*p == '.')
		p++;

	res = strdup(p);
	assert(res);
	free(pkgver);
	res[strlen(res)-5] = '\0';

	return res;
}

const char *
xbps_pkg_revision(const char *pkg)
{
	const char *p;

	assert(pkg != NULL);

	/* Get the required revision */
	if ((p = strrchr(pkg, '_')) == NULL)
		return NULL;

	if (!isdigit((unsigned char)p[1]))
		return NULL;

	return p + 1; /* skip first '_' */
}

char *
xbps_pkg_name(const char *pkg)
{
	const char *p;
	char *buf;
	unsigned int len;
	bool valid = false;

	if ((p = strrchr(pkg, '-')) == NULL)
		return NULL;

	for (unsigned int i = 0; i < strlen(p); i++) {
		if (p[i] == '_')
			break;
		if (isdigit((unsigned char)p[i]) && strchr(p, '_')) {
			valid = true;
			break;
		}
	}
	if (!valid)
		return NULL;

	len = strlen(pkg) - strlen(p) + 1;
	buf = malloc(len);
	assert(buf != NULL);

	memcpy(buf, pkg, len-1);
	buf[len-1] = '\0';

	return buf;
}

char *
xbps_pkgpattern_name(const char *pkg)
{
	char *res, *pkgname;
	unsigned int len;

	assert(pkg != NULL);

	if ((res = strpbrk(pkg, "><*?[]")) == NULL)
		return NULL;

	len = strlen(pkg) - strlen(res) + 1;
	if (strlen(pkg) < len-2)
		return NULL;

	if (pkg[len-2] == '-')
		len--;

	pkgname = malloc(len);
	assert(pkgname != NULL);

	memcpy(pkgname, pkg, len-1);
	pkgname[len-1] = '\0';

	return pkgname;
}

const char *
xbps_pkgpattern_version(const char *pkg)
{
	assert(pkg != NULL);

	return strpbrk(pkg, "><*?[]");
}

char HIDDEN *
xbps_repository_pkg_path(struct xbps_handle *xhp, xbps_dictionary_t pkg_repod)
{
	const char *pkgver, *arch, *repoloc;
	char *lbinpkg = NULL;

	assert(xhp);
	assert(xbps_object_type(pkg_repod) == XBPS_TYPE_DICTIONARY);

	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod,
	    "pkgver", &pkgver))
		return NULL;
	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod,
	    "architecture", &arch))
		return NULL;
	if (!xbps_dictionary_get_cstring_nocopy(pkg_repod,
	    "repository", &repoloc))
		return NULL;

	if (xbps_repository_is_remote(repoloc)) {
		/*
		 * First check if binpkg is available in cachedir.
		 */
		lbinpkg = xbps_xasprintf("%s/%s.%s.xbps", xhp->cachedir,
				pkgver, arch);
		if (access(lbinpkg, R_OK) == 0)
			return lbinpkg;

		free(lbinpkg);
	}
	/*
	 * Local and remote repositories use the same path.
	 */
	return xbps_xasprintf("%s/%s.%s.xbps", repoloc, pkgver, arch);
}

bool
xbps_binpkg_exists(struct xbps_handle *xhp, xbps_dictionary_t pkgd)
{
	char *binpkg;
	bool exists = true;

	if ((binpkg = xbps_repository_pkg_path(xhp, pkgd)) == NULL)
		return false;

	if (access(binpkg, R_OK) == -1)
		exists = false;

	free(binpkg);
	return exists;
}

bool
xbps_pkg_has_rundeps(xbps_dictionary_t pkgd)
{
	xbps_array_t array;

	assert(xbps_object_type(pkgd) == XBPS_TYPE_DICTIONARY);

	array = xbps_dictionary_get(pkgd, "run_depends");
	if (xbps_array_count(array))
		return true;

	return false;
}

bool
xbps_pkg_arch_match(struct xbps_handle *xhp,
		    const char *orig,
		    const char *target)
{
	const char *arch;

	if (xhp->target_arch)
		arch = xhp->target_arch;
	else
		arch = xhp->native_arch;

	if (target == NULL) {
		if ((strcmp(orig, "noarch") == 0) ||
		    (strcmp(orig, arch) == 0))
			return true;
	} else {
		if ((strcmp(orig, "noarch") == 0) ||
		    (strcmp(orig, target) == 0))
			return true;
	}
	return false;
}

char *
xbps_xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *buf = NULL;

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) == -1) {
		va_end(ap);
		assert(buf);
	}
	va_end(ap);
	assert(buf);

	return buf;
}

/*
 * Match pkg against pattern, return 1 if matching, 0 otherwise or -1 on error.
 */
int
xbps_pkgpattern_match(const char *pkg, const char *pattern)
{
	/* simple match on "pkg" against "pattern" */
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

/*
 * Small wrapper for NetBSD's humanize_number(3) with some
 * defaults set that we care about.
 */
int
xbps_humanize_number(char *buf, int64_t bytes)
{
	assert(buf != NULL);

	return humanize_number(buf, 7, bytes, "B",
	    HN_AUTOSCALE, HN_DECIMAL|HN_NOSPACE);
}

/*
 * Check if pkg is explicitly marked to replace a specific installed version.
 */
int
xbps_pkg_reverts(xbps_dictionary_t pkg, const char *pkgver) {
	unsigned int i;
	xbps_array_t reverts;
	const char *version = xbps_pkg_version(pkgver);
	const char *revertver;

	if ((reverts = xbps_dictionary_get(pkg, "reverts")) == NULL)
		return 0;

	for (i = 0; i < xbps_array_count(reverts); i++) {
		xbps_array_get_cstring_nocopy(reverts, i, &revertver);
		if (strcmp(version, revertver) == 0) {
			return 1;
		}
	}

	return 0;
}
