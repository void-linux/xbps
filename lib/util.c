/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#include "compat.h"

#include <sys/stat.h>
#include <sys/utsname.h>

#include <ctype.h>
#include <errno.h>
#include <fnmatch.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xbps_api_impl.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

static bool
is_revision(const char *str)
{
	if (str == NULL || *str == '\0')
		return false;
	/* allow underscore for accepting perl-Digest-1.17_01_1 etc. */
	while (isdigit((unsigned char)*str) || *str == '_')
		++str;
	return *str == '\0';
}

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

bool
xbps_pkg_is_ignored(struct xbps_handle *xhp, const char *pkg)
{
	char pkgname[XBPS_NAME_SIZE];
	bool rv = false;

	assert(xhp);
	assert(pkg);

	if (!xhp->ignored_pkgs)
		return false;

	if (xbps_pkgpattern_name(pkgname, XBPS_NAME_SIZE, pkg) ||
	    xbps_pkg_name(pkgname, XBPS_NAME_SIZE, pkg)) {
		rv = xbps_match_string_in_array(xhp->ignored_pkgs, pkgname);
		return rv;
	}

	return xbps_match_string_in_array(xhp->ignored_pkgs, pkg);
}

const char *
xbps_pkg_version(const char *pkg)
{
	const char *p, *r;
	size_t p_len;

	assert(pkg);

	if ((p = strrchr(pkg, '-')) == NULL)
		return NULL;

	++p; /* skip first '-' */
	p_len = strlen(p);
	for (unsigned int i = 0; i < p_len; i++) {
		if (p[i] == '_')
			break;
		if (isdigit((unsigned char)p[i]) && (r = strchr(p + i + 1, '_'))) {
			if (!is_revision(r + 1)) {
				break;
			}
			return p;
		}
	}
	return NULL;
}

char *
xbps_binpkg_pkgver(const char *pkg)
{
	const char *fname;
	char *p, *p1, *res;
	unsigned int len;

	assert(pkg);

	/* skip path if found, only interested in filename */
	if ((fname = strrchr(pkg, '/')))
		fname++;
	else
		fname = pkg;

	/* 5 == .xbps */
	if ((len = strlen(fname)) < 5)
		return NULL;
	len -= 5;

	p = malloc(len+1);
	assert(p);
	(void)memcpy(p, fname, len);
	p[len] = '\0';
	if (!(p1 = strrchr(p, '.'))) {
		free(p);
		return NULL;
	}
	p[strlen(p)-strlen(p1)] = '\0';

	/* sanity check it's a proper pkgver string */
	if (xbps_pkg_version(p) == NULL) {
		free(p);
		return NULL;
	}
	res = strdup(p);
	assert(res);

	free(p);
	return res;
}

char *
xbps_binpkg_arch(const char *pkg)
{
	const char *fname;
	char *p, *p1, *res;
	unsigned int len;

	assert(pkg);

	/* skip path if found, only interested in filename */
	if ((fname = strrchr(pkg, '/')))
		fname++;
	else
		fname = pkg;

	/* 5 == .xbps */
	if ((len = strlen(fname)) < 5)
		return NULL;
	len -= 5;

	p = malloc(len+1);
	assert(p);
	(void)memcpy(p, fname, len);
	p[len] = '\0';
	if (!(p1 = strrchr(p, '.'))) {
		free(p);
		return NULL;
	}
	res = strdup(p1 + 1);
	assert(res);

	free(p);
	return res;
}

const char *
xbps_pkg_revision(const char *pkg)
{
	const char *p, *r;
	size_t p_len;

	assert(pkg);

	if ((p = strrchr(pkg, '-')) == NULL)
		return NULL;

	++p; /* skip first '-' */
	p_len = strlen(p);
	for (unsigned int i = 0; i < p_len; i++) {
		if (p[i] == '_')
			break;
		if (isdigit((unsigned char)p[i]) && (r = strchr(p + i + 1, '_'))) {
			if (!is_revision(r + 1)) {
				break;
			}
			return strrchr(r, '_') + 1;
		}
	}
	return NULL;
}

bool
xbps_pkg_name(char *dst, size_t len, const char *pkg)
{
	const char *p, *r;
	size_t plen;
	bool valid = false;

	assert(dst);
	assert(pkg);

	if ((p = strrchr(pkg, '-')) == NULL)
		return false;

	plen = strlen(p);
	/* i = 1 skips first '-' */
	for (unsigned int i = 1; i < plen; i++) {
		if (p[i] == '_')
			break;
		if (isdigit((unsigned char)p[i]) && (r = strchr(p + i + 1, '_'))) {
			valid = is_revision(r + 1);
			break;
		}
	}
	if (!valid)
		return false;

	plen = strlen(pkg) - strlen(p) + 1;
	if (plen > len)
	       return false;

	memcpy(dst, pkg, plen-1);
	dst[plen-1] = '\0';

	return true;
}

bool
xbps_pkgpattern_name(char *dst, size_t len, const char *pkg)
{
	const char *res;
	size_t plen;

	assert(dst);
	assert(pkg);

	if ((res = strpbrk(pkg, "><*?[]")) == NULL)
		return false;

	plen = strlen(pkg) - strlen(res) + 1;
	if (strlen(pkg) < plen-2)
		return false;

	if (pkg[plen-2] == '-')
		plen--;

	if (plen > len)
		return false;

	memcpy(dst, pkg, plen-1);
	dst[plen-1] = '\0';

	return true;
}

const char *
xbps_pkgpattern_version(const char *pkg)
{
	assert(pkg != NULL);

	return strpbrk(pkg, "><*?[]");
}

ssize_t
xbps_pkg_path(struct xbps_handle *xhp, char *dst, size_t dstsz, xbps_dictionary_t pkgd)
{
	const char *pkgver = NULL, *arch = NULL, *repoloc = NULL;
	int l;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver) ||
	    !xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch) ||
	    !xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc))
		return -EINVAL;

	if (xbps_repository_is_remote(repoloc))
		repoloc = xhp->cachedir;

	l = snprintf(dst, dstsz, "%s/%s.%s.xbps", repoloc, pkgver, arch);
	if (l < 0 || (size_t)l >= dstsz)
		return -ENOBUFS;

	return l;
}

ssize_t
xbps_pkg_url(struct xbps_handle *xhp UNUSED, char *dst, size_t dstsz, xbps_dictionary_t pkgd)
{
	const char *pkgver = NULL, *arch = NULL, *repoloc = NULL;
	int l;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver) ||
	    !xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch) ||
	    !xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc))
		return -EINVAL;

	l = snprintf(dst, dstsz, "%s/%s.%s.xbps", repoloc, pkgver, arch);
	if (l < 0 || (size_t)l >= dstsz)
		return -ENOBUFS;

	return l;
}

ssize_t
xbps_pkg_path_or_url(struct xbps_handle *xhp UNUSED, char *dst, size_t dstsz, xbps_dictionary_t pkgd)
{
	const char *pkgver = NULL, *arch = NULL, *repoloc = NULL;
	int l;

	if (!xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &pkgver) ||
	    !xbps_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch) ||
	    !xbps_dictionary_get_cstring_nocopy(pkgd, "repository", &repoloc))
		return -EINVAL;

	if (xbps_repository_is_remote(repoloc)) {
		l = snprintf(dst, dstsz, "%s/%s.%s.xbps", xhp->cachedir,
		    pkgver, arch);
		if (l < 0 || (size_t)l >= dstsz)
			return -ENOBUFS;
		if (access(dst, R_OK) == 0)
			return l;
		if (errno != ENOENT)
			return -errno;
	}

	l = snprintf(dst, dstsz, "%s/%s.%s.xbps", repoloc, pkgver, arch);
	if (l < 0 || (size_t)l >= dstsz)
		return -ENOBUFS;

	return l;
}

char *
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
	char path[PATH_MAX];
	const char *pkgver, *arch, *repoloc;

	assert(xhp);
	assert(xbps_object_type(pkgd) == XBPS_TYPE_DICTIONARY);

	if (!xbps_dictionary_get_cstring_nocopy(pkgd,
	    "pkgver", &pkgver))
		return NULL;
	if (!xbps_dictionary_get_cstring_nocopy(pkgd,
	    "architecture", &arch))
		return NULL;
	if (!xbps_dictionary_get_cstring_nocopy(pkgd,
	    "repository", &repoloc))
		return NULL;

	snprintf(path, sizeof(path), "%s/%s.%s.xbps",
	    xbps_repository_is_remote(repoloc) ? xhp->cachedir : repoloc,
	    pkgver, arch);

	return access(path, R_OK) == 0;
}

bool
xbps_remote_binpkg_exists(struct xbps_handle *xhp, xbps_dictionary_t pkgd)
{
	char path[PATH_MAX];
	const char *pkgver, *arch;

	assert(xhp);
	assert(xbps_object_type(pkgd) == XBPS_TYPE_DICTIONARY);

	if (!xbps_dictionary_get_cstring_nocopy(pkgd,
	    "pkgver", &pkgver))
		return NULL;
	if (!xbps_dictionary_get_cstring_nocopy(pkgd,
	    "architecture", &arch))
		return NULL;

	snprintf(path, sizeof(path), "%s/%s.%s.xbps.sig2", xhp->cachedir,
	    pkgver, arch);

	/* check if the signature file exists */
	if (access(path, R_OK) != 0)
		return false;

	/* strip the .sig2 suffix and check if binpkg file exists */
	path[strlen(path)-sizeof (".sig2")+1] = '\0';

	return access(path, R_OK) == 0;
}

bool
xbps_pkg_arch_match(struct xbps_handle *xhp, const char *orig,
		const char *target)
{
	const char *arch;

	assert(xhp);
	assert(orig);

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
	assert(pkg);
	assert(pattern);

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

size_t
xbps_strlcat(char *dest, const char *src, size_t siz)
{
	assert(dest);
	assert(src);

	return strlcat(dest, src, siz);
}

size_t
xbps_strlcpy(char *dest, const char *src, size_t siz)
{
	assert(dest);
	assert(src);

	return strlcpy(dest, src, siz);
}

/*
 * Check if pkg is explicitly marked to replace a specific installed version.
 */
bool
xbps_pkg_reverts(xbps_dictionary_t pkg, const char *pkgver)
{
	unsigned int i;
	xbps_array_t reverts;
	const char *version = xbps_pkg_version(pkgver);
	const char *revertver = NULL;

	assert(pkg);
	assert(pkgver);

	if ((reverts = xbps_dictionary_get(pkg, "reverts")) == NULL)
		return false;

	for (i = 0; i < xbps_array_count(reverts); i++) {
		xbps_array_get_cstring_nocopy(reverts, i, &revertver);
		if (strcmp(version, revertver) == 0) {
			return true;
		}
	}

	return false;
}

char *
xbps_sanitize_path(const char *src)
{
	const char *s = src;
	char *d, *dest;
	size_t len;

	assert(src);
	len = strlen(src);
	assert(len != 0);

	dest = malloc(len+1);
	assert(dest);
	d = dest;

	while ((*d = *s)) {
		if (*s == '/' && *(s+1) == '/') {
			s++;
			continue;
		}
		d++, s++;
	}
	*d = '\0';

	return dest;
}

char *
xbps_symlink_target(struct xbps_handle *xhp, const char *path, const char *tgt)
{
	struct stat sb;
	char *res = NULL, *lnk = NULL, *p = NULL, *p1 = NULL, *dname = NULL;
	char *rootdir = NULL;
	ssize_t r;

	assert(xhp);
	assert(path);
	assert(tgt);

	if (lstat(path, &sb) == -1)
		return NULL;

	lnk = malloc(sb.st_size + 1);
	assert(lnk);

	r = readlink(path, lnk, sb.st_size + 1);
	if (r < 0 || r > sb.st_size) {
		free(lnk);
		return NULL;
	}
	lnk[sb.st_size] = '\0';

	if (tgt[0] != '/') {
		/*
		 * target file is relative and wasn't converted to absolute by
		 * xbps-create(8), just compare it as is.
		 */
		return lnk;
	}

	rootdir = realpath(xhp->rootdir, NULL);
	if (rootdir == NULL) {
		free(lnk);
		return NULL;
	}

	if (strstr(lnk, "./")) {
		/* contains references to relative paths */
		p = realpath(path, NULL);
		if (p == NULL) {
			/* dangling symlink, use target */
			free(rootdir);
			free(lnk);
			return strdup(tgt);
		}
		if (strcmp(rootdir, "/") == 0) {
			res = strdup(p);
		} else {
			p1 = strdup(p + strlen(rootdir));
			assert(p1);
			res = xbps_sanitize_path(p1);
			free(p1);
		}
		free(lnk);
		free(p);
	} else if (lnk[0] != '/') {
		/* relative path */
		p = strdup(path);
		assert(p);
		dname = dirname(p);
		assert(dname);
		if (strcmp(rootdir, "/") == 0) {
			p1 = xbps_xasprintf("%s/%s", dname, lnk);
			assert(p1);
			res = xbps_sanitize_path(p1);
			free(p1);
			free(p);
		} else {
			p1 = strdup(dname + strlen(rootdir));
			assert(p1);
			free(p);
			p = xbps_xasprintf("%s/%s", p1, lnk);
			free(p1);
			res = xbps_sanitize_path(p);
			free(p);
		}
		free(lnk);
	} else {
		/* absolute */
		res = lnk;
	}
	assert(res);
	free(rootdir);

	return res;
}

bool
xbps_patterns_match(xbps_array_t patterns, const char *path)
{
	bool match = false;

	assert(path);

	if (patterns == NULL)
		return false;

	for (unsigned int i = 0; i < xbps_array_count(patterns); i++) {
		const char *pattern = NULL;
		bool negate = false;
		if (!xbps_array_get_cstring_nocopy(patterns, i, &pattern))
			continue;
		if (pattern == NULL)
			continue;
		if ((negate = *pattern == '!') || *pattern == '\\')
			pattern++;
		if (fnmatch(pattern, path, 0) == 0)
			match = !negate;
	}

	return match;
}
