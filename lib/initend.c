/*-
 * Copyright (c) 2011-2014 Juan Romero Pardines.
 * Copyright (c) 2014 Enno Boland.
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

#include <sys/utsname.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#define _BSD_SOURCE	/* required by strlcpy with musl */
#include <string.h>
#undef _BSD_SOURCE
#include <strings.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <ctype.h>
#include <glob.h>
#include <libgen.h>

#include "xbps_api_impl.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

static int parse_file(struct xbps_handle *, const char *, bool, bool);

/**
 * @file lib/initend.c
 * @brief Initialization and finalization routines
 * @defgroup initend Initialization and finalization functions
 *
 * Use these functions to initialize some parameters before start
 * using libxbps and finalize usage to release resources at the end.
 */
static void
store_vpkg(struct xbps_handle *xhp, const char *path, size_t line, char *vpkg_s)
{
	/*
	 * Append virtual package overrides to our vpkgd dictionary:
	 *
	 * <key>vpkgver</key>
	 * <string>realpkgname</string>
	 */
	char *vpkg, *rpkg, *tc;
	size_t vpkglen;

	if (xhp->vpkgd == NULL)
		xhp->vpkgd = xbps_dictionary_create();

	/* real pkg after ':' */
	vpkg = vpkg_s;
	rpkg = strchr(vpkg_s, ':');
	if (rpkg == NULL || *rpkg == '\0') {
		xbps_dbg_printf(xhp, "%s: ignoring invalid "
		    "virtualpkg option at line %zu\n", path, line);
		return;
	}
	/* vpkg until ':' */
	tc = strchr(vpkg_s, ':');
	vpkglen = strlen(vpkg_s) - strlen(tc);
	vpkg[vpkglen] = '\0';

	/* skip ':' */
	rpkg++;
	xbps_dictionary_set_cstring(xhp->vpkgd, vpkg, rpkg);
	xbps_dbg_printf(xhp, "%s: added vpkg %s for %s\n", path, vpkg, rpkg);
}

static bool
store_repo(struct xbps_handle *xhp, const char *repo)
{
	/*
	 * Append repositories to our proplib array.
	 */
	if (xhp->repositories == NULL)
		xhp->repositories = xbps_array_create();

	/*
	 * Do not add duplicates.
	 */
	for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
		const char *srepo;

		xbps_array_get_cstring_nocopy(xhp->repositories, i, &srepo);
		if (strcmp(repo, srepo) == 0)
			return false;
	}
	xbps_array_add_cstring(xhp->repositories, repo);
	return true;
}

static bool
parse_option(char *buf, char **k, char **v)
{
	size_t klen;
	char *key, *value;
	const char *keys[] = {
		"rootdir",
		"cachedir",
		"syslog",
		"repository",
		"virtualpkgdir",
		"virtualpkg",
		"include"
	};
	bool found = false;

	for (unsigned int i = 0; i < __arraycount(keys); i++) {
		key = __UNCONST(keys[i]);
		klen = strlen(key);
		if (strncmp(buf, key, klen) == 0) {
			found = true;
			break;
		}
	}
	/* non matching option */
	if (!found)
		return false;

	/* check if next char is the equal sign */
	if (buf[klen] != '=')
		return false;

	/* skip equal sign */
	value = buf + klen + 1;
	/* eat blanks */
	while (isblank((unsigned char)*value))
		value++;
	/* eat final newline */
	value[strlen(value)-1] = '\0';
	/* option processed successfully */
	*k = key;
	*v = value;

	return true;
}

static int
parse_files_glob(struct xbps_handle *xhp, const char *path, bool nested, bool vpkgconf) {
	glob_t globbuf = { 0 };
	int i, rv = 0;

	glob(path, 0, NULL, &globbuf);
	for(i = 0; globbuf.gl_pathv[i]; i++) {
		if((rv = parse_file(xhp, globbuf.gl_pathv[i], nested, vpkgconf)) != 0)
			break;
	}
	globfree(&globbuf);

	return rv;
}

static int
parse_file(struct xbps_handle *xhp, const char *path, bool nested, bool vpkgconf)
{
	FILE *fp;
	size_t len, nlines = 0;
	ssize_t read;
	char *line = NULL;
	char ocwd[XBPS_MAXPATH], tmppath[XBPS_MAXPATH];
	char *cwd;
	int rv = 0;

	if ((fp = fopen(path, "r")) == NULL) {
		rv = errno;
		xbps_dbg_printf(xhp, "cannot read configuration file %s: %s\n", path, strerror(rv));
		return rv;
	}

	if (!vpkgconf) {
		xbps_dbg_printf(xhp, "Parsing configuration file: %s\n", path);
	}

	/* cwd to the dir containing the config file */
	strncpy(tmppath, path, sizeof(tmppath));
	cwd = dirname(tmppath);
	if(getcwd(ocwd, sizeof(ocwd)) == NULL) {
		rv = errno;
		xbps_dbg_printf(xhp, "cannot get cwd: %s\n", strerror(rv));
		return rv;
	}
	if(chdir(cwd)) {
		rv = errno;
		xbps_dbg_printf(xhp, "cannot chdir to %s: %s\n", cwd, strerror(rv));
		return rv;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		char *p, *k, *v;

		nlines++;
		p = line;
		/* eat blanks */
		while (isblank((unsigned char)*p))
			p++;
		/* ignore comments or empty lines */
		if (*p == '#' || *p == '\n')
			continue;
		if (!parse_option(p, &k, &v)) {
			xbps_dbg_printf(xhp, "%s: ignoring invalid option at "
			    "line %zu\n", path, nlines);
			continue;
		}
		if (strcmp(k, "rootdir") == 0) {
			xbps_dbg_printf(xhp, "%s: rootdir set to %s\n",
			    path, v);
			snprintf(xhp->rootdir, sizeof(xhp->rootdir), "%s", v);
		} else if (strcmp(k, "cachedir") == 0) {
			xbps_dbg_printf(xhp, "%s: cachedir set to %s\n",
			    path, v);
			snprintf(xhp->cachedir, sizeof(xhp->cachedir), "%s", v);
		} else if (strcmp(k, "virtualpkgdir") == 0) {
			xbps_dbg_printf(xhp, "%s: virtualpkgdir set to %s\n",
			    path, v);
			snprintf(xhp->virtualpkgdir, sizeof(xhp->virtualpkgdir), "%s", v);
		} else if (strcmp(k, "syslog") == 0) {
			if (strcasecmp(v, "true") == 0) {
				xhp->syslog = true;
				xbps_dbg_printf(xhp, "%s: syslog enabled\n", path);
			}
		} else if (strcmp(k, "repository") == 0) {
			if (store_repo(xhp, v))
				xbps_dbg_printf(xhp, "%s: added repository %s\n", path, v);
		} else if (strcmp(k, "virtualpkg") == 0) {
			store_vpkg(xhp, path, nlines, v);
		}
		/* Avoid double-nested parsing, only allow it once */
		if (nested)
			continue;

		if (strcmp(k, "include"))
			continue;

		if ((rv = parse_files_glob(xhp, v, true, false)) != 0)
			break;

	}
	free(line);
	fclose(fp);

	/* Going back to old working directory */
	if(chdir(ocwd)) {
		rv = errno;
		xbps_dbg_printf(xhp, "cannot chdir to %s: %s\n", ocwd, strerror(rv));
		return rv;
	}

	return rv;
}

static int
parse_vpkgdir(struct xbps_handle *xhp)
{
	DIR *dirp;
	struct dirent *dp;
	char *ext;
	int rv = 0;

	if ((dirp = opendir(xhp->virtualpkgdir)) == NULL)
		return 0;

	xbps_dbg_printf(xhp, "Parsing virtualpkg directory: %s\n", xhp->virtualpkgdir);

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, "..") == 0) ||
		    (strcmp(dp->d_name, ".") == 0))
			continue;
		/* only process .vpkg files, ignore something else */
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".vpkg") == 0) {
			char *path;

			path = xbps_xasprintf("%s/%s", xhp->virtualpkgdir, dp->d_name);
			if ((rv = parse_file(xhp, path, false, true)) != 0) {
				free(path);
				break;
			}
			free(path);
		}
	}
	closedir(dirp);
	return rv;
}

int
xbps_init(struct xbps_handle *xhp)
{
	struct utsname un;
	char *buf;
	const char *repodir, *native_arch;
	int rv;

	assert(xhp != NULL);

	if (xhp->conffile == NULL)
		xhp->conffile = XBPS_CONF_DEF;

	/* parse configuration file */
	if ((rv = parse_file(xhp, xhp->conffile, false, false)) != 0) {
		xbps_dbg_printf(xhp, "failed to read configuration file %s: %s\n",
		     xhp->conffile, strerror(rv));
		xbps_dbg_printf(xhp, "Using built-in defaults\n");
	}
	/* Set rootdir */
	if (xhp->rootdir[0] == '\0') {
		xhp->rootdir[0] = '/';
		xhp->rootdir[1] = '\0';
	} else if (xhp->rootdir[0] != '/') {
		/* relative path */
		char path[PATH_MAX-1];

		if (getcwd(path, sizeof(path)) == NULL)
			return ENOTSUP;

		buf = strdup(xhp->rootdir);
		snprintf(xhp->rootdir, sizeof(xhp->rootdir),
		    "%s/%s", path, buf);
		free(buf);
	}
	/* Set cachedir */
	if (xhp->cachedir[0] == '\0') {
		snprintf(xhp->cachedir, sizeof(xhp->cachedir),
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
		    XBPS_CACHE_PATH);
	} else if (xhp->cachedir[0] != '/') {
		/* relative path */
		buf = strdup(xhp->cachedir);
		snprintf(xhp->cachedir, sizeof(xhp->cachedir),
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "", buf);
		free(buf);
	}
	/* Set metadir */
	if (xhp->metadir[0] == '\0') {
		snprintf(xhp->metadir, sizeof(xhp->metadir),
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
		    XBPS_META_PATH);
	} else if (xhp->metadir[0] != '/') {
		/* relative path */
		buf = strdup(xhp->metadir);
		snprintf(xhp->metadir, sizeof(xhp->metadir),
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "", buf);
		free(buf);
	}
	/* Set virtualpkgdir */
	if (xhp->virtualpkgdir[0] == '\0') {
		snprintf(xhp->virtualpkgdir, sizeof(xhp->virtualpkgdir),
		    "%s%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
		    XBPS_VPKG_PATH);
	} else if (xhp->virtualpkgdir[0] != '/') {
		/* relative path */
		buf = strdup(xhp->virtualpkgdir);
		snprintf(xhp->virtualpkgdir, sizeof(xhp->virtualpkgdir),
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "", buf);
		free(buf);
	}
	/* parse virtualpkgdir */
	if ((rv = parse_vpkgdir(xhp)))
		return rv;

	xhp->target_arch = getenv("XBPS_TARGET_ARCH");
	if ((native_arch = getenv("XBPS_ARCH")) != NULL) {
		strlcpy(xhp->native_arch, native_arch, sizeof(xhp->native_arch));
	} else {
		uname(&un);
		strlcpy(xhp->native_arch, un.machine, sizeof(xhp->native_arch));
	}
	assert(xhp->native_arch);

	xbps_fetch_set_cache_connection(XBPS_FETCH_CACHECONN, XBPS_FETCH_CACHECONN_HOST);

	xbps_dbg_printf(xhp, "rootdir=%s\n", xhp->rootdir);
	xbps_dbg_printf(xhp, "metadir=%s\n", xhp->metadir);
	xbps_dbg_printf(xhp, "cachedir=%s\n", xhp->cachedir);
	xbps_dbg_printf(xhp, "virtualpkgdir=%s\n", xhp->virtualpkgdir);
	xbps_dbg_printf(xhp, "syslog=%s\n", xhp->syslog ? "true" : "false");
	xbps_dbg_printf(xhp, "Architecture: %s\n", xhp->native_arch);
	xbps_dbg_printf(xhp, "Target Architecture: %s\n", xhp->target_arch);

	if (xhp->flags & XBPS_FLAG_DEBUG) {
		for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
			xbps_array_get_cstring_nocopy(xhp->repositories, i, &repodir);
			xbps_dbg_printf(xhp, "Repository[%u]=%s\n", i, repodir);
		}
	}
	return 0;
}

void
xbps_end(struct xbps_handle *xhp)
{
	assert(xhp);

	xbps_pkgdb_release(xhp);
	xbps_fetch_unset_cache_connection();
}

static void
common_printf(FILE *f, const char *msg, const char *fmt, va_list ap)
{
	if (msg != NULL)
		fprintf(f, "%s", msg);

	vfprintf(f, fmt, ap);
}

void
xbps_dbg_printf_append(struct xbps_handle *xhp, const char *fmt, ...)
{
	va_list ap;

	if ((xhp->flags & XBPS_FLAG_DEBUG) == 0)
		return;

	va_start(ap, fmt);
	common_printf(stderr, NULL, fmt, ap);
	va_end(ap);
}

void
xbps_dbg_printf(struct xbps_handle *xhp, const char *fmt, ...)
{
	va_list ap;

	if ((xhp->flags & XBPS_FLAG_DEBUG) == 0)
		return;

	va_start(ap, fmt);
	common_printf(stderr, "[DEBUG] ", fmt, ap);
	va_end(ap);
}

void
xbps_error_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	common_printf(stderr, "ERROR: ", fmt, ap);
	va_end(ap);
}

void
xbps_warn_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	common_printf(stderr, "WARNING: ", fmt, ap);
	va_end(ap);
}
