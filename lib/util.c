/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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
#include <sys/mman.h>
#include <limits.h>

#include "config.h"
#include <openssl/sha.h>
#include <xbps_api.h>
#include "xbps_api_impl.h"

/**
 * @file lib/util.c
 * @brief Utility routines
 * @defgroup util Utility functions
 */

static const char *rootdir;
static const char *cachedir;
static int flags;

static void
digest2string(const uint8_t *digest, char *string, size_t len)
{
	while (len--) {
		if (*digest / 16 < 10)
			*string++ = '0' + *digest / 16;
		else
			*string++ = 'a' + *digest / 16 - 10;
		if (*digest % 16 < 10)
			*string++ = '0' + *digest % 16;
		else
			*string++ = 'a' + *digest % 16 - 10;
		++digest;
	}
	*string = '\0';
}

char *
xbps_get_file_hash(const char *file)
{
	struct stat st;
	size_t pgsize = (size_t)sysconf(_SC_PAGESIZE);
	size_t pgmask = pgsize - 1, mapsize;
	char hash[SHA256_DIGEST_LENGTH * 2 + 1];
	unsigned char *buf = NULL, digest[SHA256_DIGEST_LENGTH];
	int fd;
	bool need_guard = false;

	if ((fd = open(file, O_RDONLY)) == -1) {
		free(buf);
		return NULL;
	}
	memset(&st, 0, sizeof(st));
	if (fstat(fd, &st) == -1) {
		(void)close(fd);
		return NULL;
	}
	if (st.st_size > SSIZE_MAX - 1) {
		(void)close(fd);
		return NULL;
	}

	mapsize = ((size_t)st.st_size + pgmask) & ~pgmask;
	if (mapsize < (size_t)st.st_size) {
		(void)close(fd);
		return NULL;
	}
	/*
	 * If the file length is an integral number of pages, then we
	 * need to map a guard page at the end in order to provide the
	 * necessary NUL-termination of the buffer.
	 */
	if ((st.st_size & pgmask) == 0)
		need_guard = true;

	buf = mmap(NULL, need_guard ? mapsize + pgsize : mapsize,
		PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);
	(void)close(fd);
	if (buf == MAP_FAILED)
		return NULL;

	if (SHA256(buf, st.st_size, digest) == NULL) {
		munmap(buf, mapsize);
		return NULL;
	}
	munmap(buf, mapsize);
	digest2string(digest, hash, SHA256_DIGEST_LENGTH);

	return strdup(hash);
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
	const char *pkgver = NULL;

	assert(d != NULL);

	prop_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver);
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
	if (pkgname == NULL)
		return NULL;

	strlcpy(pkgname, pkg, len);

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

	strlcpy(pkgname, pkg, len);

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
	free(uri_fixed);

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
xbps_get_binpkg_repo_uri(prop_dictionary_t pkg_repod, const char *repoloc)
{
	const char *filen, *arch, *cdir;
	char *lbinpkg = NULL;

	if (!prop_dictionary_get_cstring_nocopy(pkg_repod,
	    "filename", &filen))
		return NULL;
	if (!prop_dictionary_get_cstring_nocopy(pkg_repod,
	    "architecture", &arch))
		return NULL;

	cdir = xbps_get_cachedir();
	if (cdir == NULL)
		return NULL;

	/*
	 * First check if binpkg is available in cachedir.
	 */
	lbinpkg = xbps_xasprintf("%s/%s", cdir, filen);
	if (lbinpkg == NULL)
		return NULL;

	if (access(lbinpkg, R_OK) == 0)
		return lbinpkg;

	free(lbinpkg);
	/*
	 * Local and remote repositories use the same path.
	 */
	return xbps_xasprintf("%s/%s/%s", repoloc, arch, filen);
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
		rootdir = "/";

	return rootdir;
}

void
xbps_set_cachedir(const char *dir)
{
	static char res[PATH_MAX];
	int r = 0;

	assert(dir != NULL);

	r = snprintf(res, sizeof(res) - 1, "%s/%s", xbps_get_rootdir(), dir);
	if (r == -1 || r >= (int)sizeof(res) - 1) {
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
		r = snprintf(res, sizeof(res) - 1, "%s/%s",
		    xbps_get_rootdir(), XBPS_CACHE_PATH);
		if (r == -1 || r >= (int)sizeof(res) - 1)
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
