/*-
 * Copyright (c) 2008-2009 Juan Romero Pardines.
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
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/utsname.h>
#include <limits.h>
#include <fnmatch.h>

#include <xbps_api.h>
#include "sha256.h"

/**
 * @file lib/util.c
 * @brief Utility routines
 * @defgroup util Utility functions
 */

static const char *rootdir;
static const char *cachedir;
static int flags;

char *
xbps_get_file_hash(const char *file)
{
	SHA256_CTX ctx;
	char *hash;
	uint8_t buf[BUFSIZ * 20], digest[SHA256_DIGEST_STRING_LENGTH];
	ssize_t bytes;
	int fd;

	if ((fd = open(file, O_RDONLY)) == -1)
		return NULL;

	XBPS_SHA256_Init(&ctx);
	while ((bytes = read(fd, buf, sizeof(buf))) > 0)
		XBPS_SHA256_Update(&ctx, buf, (size_t)bytes);
	hash = strdup(XBPS_SHA256_End(&ctx, digest));
	(void)close(fd);

	return hash;
}

int
xbps_check_file_hash(const char *file, const char *sha256)
{
	char *res;

	res = xbps_get_file_hash(file);
	if (res == NULL)
		return errno;

	if (strcmp(sha256, res)) {
		free(res);
		return ERANGE;
	}
	free(res);

	return 0;
}

bool
xbps_check_is_repo_string_remote(const char *uri)
{
	assert(uri != NULL);

	if ((strncmp(uri, "https://", 8) == 0) ||
	    (strncmp(uri, "http://", 7) == 0) ||
	    (strncmp(uri, "ftp://", 6) == 0))
		return true;

	return false;
}

static const char *
xbps_get_pkgver_from_dict(prop_dictionary_t d)
{
	const char *pkgver;

	assert(d != NULL);

	if (!prop_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver))
		return NULL;

	return pkgver;
}

int
xbps_check_is_installed_pkg(const char *pkg)
{
	prop_dictionary_t dict;
	const char *instpkgver = NULL;
	char *pkgname;
	int rv = 0;
	pkg_state_t state = 0;

	assert(pkg != NULL);

	pkgname = xbps_get_pkgpattern_name(pkg);
	if (pkgname == NULL)
		return -1;

	dict = xbps_find_pkg_dict_installed(pkgname, false);
	if (dict == NULL) {
		free(pkgname);
		if (errno == ENOENT) {
			errno = 0;
			return 0; /* not installed */
		}	
		return -1;
	}

	/*
	 * Check that package state is fully installed, not
	 * unpacked or something else.
	 */
	if (xbps_get_pkg_state_dictionary(dict, &state) != 0) {
		prop_object_release(dict);
		free(pkgname);
		return -1;
	}
	if (state != XBPS_PKG_STATE_INSTALLED) {
		prop_object_release(dict);
		free(pkgname);
		return 0; /* not fully installed */
	}
	free(pkgname);

	/* Check if installed pkg is matched against pkgdep pattern */
	instpkgver = xbps_get_pkgver_from_dict(dict);
	if (instpkgver == NULL) {
		prop_object_release(dict);
		return -1;
	}

	rv = xbps_pkgpattern_match(instpkgver, __UNCONST(pkg));
	prop_object_release(dict);

	return rv;
}

bool
xbps_check_is_installed_pkgname(const char *pkgname)
{
	prop_dictionary_t pkgd;

	assert(pkgname != NULL);

	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd) {
		prop_object_release(pkgd);
		return true;
	}

	return false;
}

const char *
xbps_get_pkg_epoch(const char *pkg)
{
	const char *tmp;

	assert(pkg != NULL);

	tmp = strrchr(pkg, ':');
	if (tmp == NULL)
		return NULL;

	return tmp + 1; /* skip first ':' */
}

const char *
xbps_get_pkg_version(const char *pkg)
{
	const char *tmp;

	assert(pkg != NULL);

	/* Get the required version */
	tmp = strrchr(pkg, '-');
	if (tmp == NULL)
		return NULL;

	return tmp + 1; /* skip first '-' */
}

const char *
xbps_get_pkg_revision(const char *pkg)
{
	const char *tmp;

	assert(pkg != NULL);

	/* Get the required revision */
	tmp = strrchr(pkg, '_');
	if (tmp == NULL)
		return NULL;

	return tmp + 1; /* skip first '_' */
}

char *
xbps_get_pkg_name(const char *pkg)
{
	const char *tmp;
	char *pkgname;
	size_t len = 0;

	assert(pkg != NULL);

	/* Get package name */
	tmp = strrchr(pkg, '-');
	if (tmp == NULL)
		return NULL;

	len = strlen(pkg) - strlen(tmp) + 1;

	pkgname = malloc(len);
	strncpy(pkgname, pkg, len);
	pkgname[len - 1] = '\0';

	return pkgname;
}

char *
xbps_get_pkgpattern_name(const char *pkg)
{
	char *res, *pkgname;
	size_t len;

	assert(pkg != NULL);

	res = strpbrk(pkg, "><=");
	if (res == NULL)
		return NULL;

	len = strlen(pkg) - strlen(res) + 1;
	pkgname = malloc(len);
	if (pkgname == NULL)
		return NULL;

	strncpy(pkgname, pkg, len);
	pkgname[len - 1] = '\0';

	return pkgname;
}

const char *
xbps_get_pkgpattern_version(const char *pkg)
{
	char *res;

	assert(pkg != NULL);

	res = strpbrk(pkg, "><=");
	if (res == NULL)
		return NULL;

	return res;
}

static char *
get_pkg_index_remote_plist(const char *uri)
{
	char *uri_fixed, *repodir;

	uri_fixed = xbps_get_remote_repo_string(uri);
	if (uri_fixed == NULL)
		return NULL;

	repodir = xbps_xasprintf("%s/%s/%s/%s",
	    xbps_get_rootdir(), XBPS_META_PATH, uri_fixed, XBPS_PKGINDEX);
	if (repodir == NULL) {
		free(uri_fixed);
		return NULL;
	}
		
	return repodir;
}

char *
xbps_get_pkg_index_plist(const char *uri)
{
	struct utsname un;

	assert(uri != NULL);

	if (uname(&un) == -1)
		return NULL;

	if (xbps_check_is_repo_string_remote(uri))
		return get_pkg_index_remote_plist(uri);

	return xbps_xasprintf("%s/%s/%s", uri, un.machine, XBPS_PKGINDEX);
}

char *
xbps_get_binpkg_local_path(prop_dictionary_t pkgd, const char *repoloc)
{
	const char *filen, *arch, *cdir;

	if (!prop_dictionary_get_cstring_nocopy(pkgd, "filename", &filen))
		return NULL;
	if (!prop_dictionary_get_cstring_nocopy(pkgd, "architecture", &arch))
		return NULL;
	cdir = xbps_get_cachedir();
	if (cdir == NULL)
		return NULL;

	if (!xbps_check_is_repo_string_remote(repoloc)) {
		/* local repo */
		return xbps_xasprintf("%s/%s/%s", repoloc, arch, filen);
	}
	/* cachedir */
	return xbps_xasprintf("%s/%s", cdir, filen);
}

bool
xbps_pkg_has_rundeps(prop_dictionary_t pkg)
{
	prop_array_t array;

	assert(pkg != NULL);
	array = prop_dictionary_get(pkg, "run_depends");
	if (array && prop_array_count(array) > 0)
		return true;

	return false;
}

void
xbps_set_rootdir(const char *dir)
{
	assert(dir != NULL);
	rootdir = dir;
}

const char *
xbps_get_rootdir(void)
{
	if (rootdir == NULL)
		rootdir = "";

	return rootdir;
}

void
xbps_set_cachedir(const char *dir)
{
	static char res[PATH_MAX];
	int r = 0;

	assert(dir != NULL);

	r = snprintf(res, sizeof(res), "%s/%s", xbps_get_rootdir(), dir);
	if (r == -1 || r >= (int)sizeof(res)) {
		/* If error or truncated set to default */
		cachedir = XBPS_CACHE_PATH;
		return;
	}
	cachedir = res;
}

const char *
xbps_get_cachedir(void)
{
	static char res[PATH_MAX];
	int r = 0;

	if (cachedir == NULL) {
		r = snprintf(res, sizeof(res), "%s/%s",
		    xbps_get_rootdir(), XBPS_CACHE_PATH);
		if (r == -1 || r >= (int)sizeof(res))
			return NULL;

		cachedir = res;
	}
	return cachedir;
}

void
xbps_set_flags(int lflags)
{
	flags = lflags;
}

int
xbps_get_flags(void)
{
	return flags;
}

char *
xbps_xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *buf;

	va_start(ap, fmt);
	if (vasprintf(&buf, fmt, ap) == -1)
		return NULL;
	va_end(ap);

	return buf;
}

static char *
strtrim(char *str)
{
	char *pch = str;

	if (str == NULL || *str == '\0')
		return str;

	while (isspace((unsigned char)*pch))
		pch++;

	if (pch != str)
		memmove(str, pch, (strlen(pch) + 1));

	if (*str == '\0')
		return str;

	pch = (str + (strlen(str) - 1));
	while (isspace((unsigned char)*pch))
		pch--;

	*++pch = '\0';

	return str;
}

static bool
question(bool preset, const char *fmt, va_list ap)
{
	char response[32];

	vfprintf(stderr, fmt, ap);
	if (preset)
		fprintf(stderr, " %s ", "[YES/no]");
	else
		fprintf(stderr, " %s ", "[yes/NO]");

	if (fgets(response, 32, stdin)) {
		(void)strtrim(response);
		if (strlen(response) == 0)
			return preset;

		if (strcasecmp(response, "yes") == 0)
			return true;
		else if (strcasecmp(response, "no") == 0)
			return false;
	}
	return false;
}

bool
xbps_yesno(const char *fmt, ...)
{
	va_list ap;
	bool res;

	va_start(ap, fmt);
	res = question(1, fmt, ap);
	va_end(ap);

	return res;
}

bool
xbps_noyes(const char *fmt, ...)
{
	va_list ap;
	bool res;

	va_start(ap, fmt);
	res = question(0, fmt, ap);
	va_end(ap);

	return res;
}
