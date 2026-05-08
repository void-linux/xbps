/*
 * Copyright (c) 2011-2015 Juan Romero Pardines.
 * Copyright (c) 2014 Enno Boland.
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

#include <sys/types.h>
#ifdef __FreeBSD__
#define _WITH_GETLINE   /* getline() */
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <glob.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "xbps_api_impl.h"

/**
 * @file lib/conf.c
 * @brief Configuration parsing
 * @defgroup conf Configuration parsing
 *
 * Functions for parsing xbps configuration files.
 */

static int
vpkg_map_add(xbps_dictionary_t d, const char *pkgname, const char *vpkgver, const char *provider)
{
	xbps_dictionary_t providers;
	bool alloc;

	providers = xbps_dictionary_get(d, pkgname);
	if (!providers) {
		providers = xbps_dictionary_create();
		if (!providers)
			return xbps_error_oom();

		if (!xbps_dictionary_set(d, pkgname, providers)) {
			xbps_object_release(providers);
			return xbps_error_oom();
		}
		alloc = true;
	}

	if (!xbps_dictionary_set_cstring(providers, vpkgver, provider)) {
		if (alloc)
			xbps_object_release(providers);
		return xbps_error_oom();
	}

	if (alloc)
		xbps_object_release(providers);

	return 0;
}

static int
store_virtualpkg(struct xbps_handle *xhp, const char *path, size_t line, char *val)
{
	char namebuf[XBPS_NAME_SIZE];
	char pkgverbuf[XBPS_NAME_SIZE + sizeof("-99999_1")];
	const char *vpkgname, *vpkgver, *provider;
	char *p;
	int r;


	/*
	 * Parse strings delimited by ':' i.e
	 * 	<left>:<right>
	 */
	p = strchr(val, ':');
	if (p == NULL || p[1] == '\0') {
		xbps_dbg_printf("%s: ignoring invalid "
		    "virtualpkg option at line %zu\n", path, line);
		return 0;
	}
	*p++ = '\0';
	provider = p;

	if (xbps_pkg_name(namebuf, sizeof(namebuf), val)) {
		vpkgname = namebuf;
		vpkgver = val;
	} else {
		vpkgname = val;
		snprintf(pkgverbuf, sizeof(pkgverbuf), "%s-99999_1", vpkgname);
		vpkgver = pkgverbuf;
	}

	r = vpkg_map_add(xhp->vpkgd, vpkgname, vpkgver, provider);
	if (r < 0)
		return r;
	r = vpkg_map_add(xhp->vpkgd_conf, vpkgname, vpkgver, provider);
	if (r < 0)
		return r;
	xbps_dbg_printf("%s: added virtualpkg %s for %s\n", path, val, p);
	return 0;
}

static int
store_preserved_file(struct xbps_handle *xhp, const char *file)
{
	char path[PATH_MAX];
	glob_t globbuf;
	char *p = NULL;
	size_t len;
	int r;

	if (!xhp->preserved_files) {
		xhp->preserved_files = xbps_array_create();
		if (!xhp->preserved_files)
			return xbps_error_oom();
	}

	if (xbps_path_join(path, sizeof(path), xhp->rootdir, file, (char *)NULL) == -1)
		return -errno;

	r = glob(path, 0, NULL, &globbuf);
	if (r == GLOB_NOMATCH) {
		if (xbps_match_string_in_array(xhp->preserved_files, file))
			goto out;
		xbps_array_add_cstring(xhp->preserved_files, file);
		xbps_dbg_printf("Added preserved file: %s\n", file);
		r = 0;
		goto out;
	} else if (r == GLOB_NOSPACE) {
		r = xbps_error_oom();
		goto out;
	} else if (r != 0) {
		r = xbps_error_errno(errno, "glob failed: %s\n", strerror(errno));
		goto out;
	}
	r = 0;
	for (size_t i = 0; i < globbuf.gl_pathc; i++) {
		if (xbps_match_string_in_array(xhp->preserved_files, globbuf.gl_pathv[i]))
			continue;

		// XXX: clean this up
		len = strlen(globbuf.gl_pathv[i]) - strlen(xhp->rootdir) + 1;
		p = malloc(len);
		assert(p);
		xbps_strlcpy(p, globbuf.gl_pathv[i] + strlen(xhp->rootdir), len);
		xbps_array_add_cstring(xhp->preserved_files, p);
		xbps_dbg_printf("Added preserved file: %s (expanded from %s)\n", p, file);
		free(p);
	}
out:
	globfree(&globbuf);
	return r;
}

static bool
store_repo(struct xbps_handle *xhp, const char *repo)
{
	if (xhp->flags & XBPS_FLAG_IGNORE_CONF_REPOS)
		return false;

	return xbps_repo_store(xhp, repo);
}

static int
store_ignored_pkg(struct xbps_handle *xhp, const char *pkgname)
{
	if (!xhp->ignored_pkgs) {
		xhp->ignored_pkgs = xbps_array_create();
		if (!xhp->ignored_pkgs)
			return xbps_error_oom();
	}
	if (!xbps_array_add_cstring(xhp->ignored_pkgs, pkgname))
		return xbps_error_oom();
	xbps_dbg_printf("Added ignored package: %s\n", pkgname);
	return 0;
}

static int
store_noextract(struct xbps_handle *xhp, const char *value)
{
	if (*value == '\0')
		return 0;
	if (!xhp->noextract) {
		xhp->noextract = xbps_array_create();
		if (!xhp->noextract)
			return xbps_error_oom();
	}
	if (!xbps_array_add_cstring(xhp->noextract, value))
		return xbps_error_oom();
	xbps_dbg_printf("Added noextract pattern: %s\n", value);
	return 0;
}

enum {
	KEY_ERROR = 0,
	KEY_ARCHITECTURE,
	KEY_BESTMATCHING,
	KEY_CACHEDIR,
	KEY_IGNOREPKG,
	KEY_INCLUDE,
	KEY_NOEXTRACT,
	KEY_PRESERVE,
	KEY_REPOSITORY,
	KEY_ROOTDIR,
	KEY_STAGING,
	KEY_SYSLOG,
	KEY_VIRTUALPKG,
	KEY_KEEPCONF,
};

static const struct key {
	const char *str;
	size_t len;
	int key;
} keys[] = {
	{ "architecture", 12, KEY_ARCHITECTURE },
	{ "bestmatching", 12, KEY_BESTMATCHING },
	{ "cachedir",      8, KEY_CACHEDIR },
	{ "ignorepkg",     9, KEY_IGNOREPKG },
	{ "include",       7, KEY_INCLUDE },
	{ "keepconf",      8, KEY_KEEPCONF },
	{ "noextract",     9, KEY_NOEXTRACT },
	{ "preserve",      8, KEY_PRESERVE },
	{ "repository",   10, KEY_REPOSITORY },
	{ "rootdir",       7, KEY_ROOTDIR },
	{ "staging",       7, KEY_STAGING },
	{ "syslog",        6, KEY_SYSLOG },
	{ "virtualpkg",   10, KEY_VIRTUALPKG },
};

static int
cmpkey(const void *a, const void *b)
{
	const struct key *ka = a;
	const struct key *kb = b;
	return strncmp(ka->str, kb->str, ka->len);
}

static int
parse_option(char *line, size_t linelen, char **valp, size_t *vallen)
{
	struct key needle;
	const struct key *result;
	char *p;
	size_t len;

	p = strpbrk(line, " \t=");
	if (p == NULL)
		return KEY_ERROR;
	needle.str = line;
	needle.len = p-line;

	while (*p && isblank((unsigned char)*p))
		p++;
	if (*p != '=')
		return KEY_ERROR;

	result = bsearch(&needle, keys, __arraycount(keys), sizeof(struct key), cmpkey);
	if (result == NULL)
		return KEY_ERROR;

	p++;
	while (isblank((unsigned char)*p))
		p++;

	len = linelen-(p-line);
	/* eat trailing spaces, len - 1 here because \0 should be set -after- the first non-space
	 * if len points at the actual current character, we can never make it an empty string
	 * because than end needs to be set to -1, but len is a unsigned type thus would result in underflow */
	while (len > 0 && isblank((unsigned char)p[len-1]))
		len--;

	p[len] = '\0';
	*valp = p;
	*vallen = len;

	return result->key;
}

static int parse_file(struct xbps_handle *, const char *, bool);

static int
parse_files_glob(struct xbps_handle *xhp, xbps_dictionary_t seen,
    const char *cwd, const char *pat, bool nested)
{
	char path[PATH_MAX];
	glob_t globbuf;
	int r = 0;

	if (xbps_path_join(path, sizeof(path),
	        pat[0] == '/' ? xhp->rootdir : cwd, pat, (char *)NULL) == -1)
		return -ENAMETOOLONG;

	switch (glob(path, 0, NULL, &globbuf)) {
	case 0:
		break;
	case GLOB_NOSPACE:
		r = xbps_error_oom();
		goto out;
	case GLOB_NOMATCH:
	default:
		goto out;
	}
	for (size_t i = 0; i < globbuf.gl_pathc; i++) {
		if (seen != NULL) {
			const char *fname;
			bool mask = false;
			fname = basename(globbuf.gl_pathv[i]);
			if (xbps_dictionary_get_bool(seen, fname, &mask) && mask)
				continue;
			if (!xbps_dictionary_set_bool(seen, fname, true)) {
				r = xbps_error_oom();
				goto out;
			}
		}
		r = parse_file(xhp, globbuf.gl_pathv[i], nested);
		if (r < 0)
			goto out;
	}
	r = 0;
out:
	globfree(&globbuf);
	return r;
}

static int
store_string(char *dst, size_t dstsz, const char *src)
{
	size_t n = strlcpy(dst, src, dstsz);
	if (n >= dstsz)
		return -ENOBUFS;
	return 0;
}

static int
parse_file(struct xbps_handle *xhp, const char *path, bool nested)
{
	FILE *fp;
	size_t len, nlines = 0;
	ssize_t rd;
	char *linebuf = NULL;
	int r = 0;
	char *dir;

	fp = fopen(path, "r");
	if (!fp) {
		return xbps_error_errno(errno,
		    "cannot read configuration file %s: %s\n", path,
		    strerror(errno));
	}

	xbps_dbg_printf("Parsing configuration file: %s\n", path);

	while ((rd = getline(&linebuf, &len, fp)) != -1) {
		char *line = linebuf;
		char *val = NULL;
		size_t vallen;

		if (line[rd-1] == '\n') {
			line[rd-1] = '\0';
			rd--;
		}

		nlines++;

		/* eat blanks */
		while (isblank((unsigned char)*line))
			line++;
		/* ignore comments or empty lines */
		if (line[0] == '#' || line[0] == '\0')
			continue;

		switch (parse_option(line, rd, &val, &vallen)) {
		case KEY_ERROR:
			xbps_dbg_printf("%s: ignoring invalid option at "
			    "line %zu\n", path, nlines);
			continue;
		case KEY_ROOTDIR:
			r = store_string(xhp->rootdir, sizeof(xhp->rootdir), val);
			if (r < 0)
				break;
			xbps_dbg_printf("%s: rootdir set to %s\n", path, val);
			break;
		case KEY_CACHEDIR:
			r = store_string(xhp->cachedir, sizeof(xhp->cachedir), val);
			if (r < 0)
				break;
			xbps_dbg_printf("%s: cachedir set to %s\n", path, val);
			break;
		case KEY_ARCHITECTURE:
			r = store_string(xhp->native_arch, sizeof(xhp->native_arch), val);
			if (r < 0)
				break;
			xbps_dbg_printf("%s: native architecture set to %s\n", path,
			    val);
			break;
		case KEY_STAGING:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags |= XBPS_FLAG_USE_STAGE;
				xbps_dbg_printf("%s: repository stage enabled\n", path);
			} else {
				xhp->flags &= ~XBPS_FLAG_USE_STAGE;
				xbps_dbg_printf("%s: repository stage disabled\n", path);
			}
			break;
		case KEY_SYSLOG:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags &= ~XBPS_FLAG_DISABLE_SYSLOG;
				xbps_dbg_printf("%s: syslog enabled\n", path);
			} else {
				xhp->flags |= XBPS_FLAG_DISABLE_SYSLOG;
				xbps_dbg_printf("%s: syslog disabled\n", path);
			}
			break;
		case KEY_REPOSITORY:
			if (store_repo(xhp, val))
				xbps_dbg_printf("%s: added repository %s\n", path, val);
			break;
		case KEY_VIRTUALPKG:
			r = store_virtualpkg(xhp, path, nlines, val);
			break;
		case KEY_PRESERVE:
			r = store_preserved_file(xhp, val);
			break;
		case KEY_KEEPCONF:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags |= XBPS_FLAG_KEEP_CONFIG;
				xbps_dbg_printf("%s: config preservation enabled\n", path);
			} else {
				xhp->flags &= ~XBPS_FLAG_KEEP_CONFIG;
				xbps_dbg_printf("%s: config preservation disabled\n", path);
			}
			break;
		case KEY_BESTMATCHING:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags |= XBPS_FLAG_BESTMATCH;
				xbps_dbg_printf("%s: pkg best matching enabled\n", path);
			} else {
				xhp->flags &= ~XBPS_FLAG_BESTMATCH;
				xbps_dbg_printf("%s: pkg best matching disabled\n", path);
			}
			break;
		case KEY_IGNOREPKG:
			r = store_ignored_pkg(xhp, val);
			break;
		case KEY_NOEXTRACT:
			r = store_noextract(xhp, val);
			break;
		case KEY_INCLUDE:
			/* Avoid double-nested parsing, only allow it once */
			if (nested) {
				xbps_dbg_printf("%s: ignoring nested include\n", path);
				continue;
			}
			dir = strdup(path);
			r = parse_files_glob(xhp, NULL, dirname(dir), val, true);
			free(dir);
			break;
		}
	}
	free(linebuf);
	fclose(fp);

	return r;
}

int HIDDEN
xbps_conf_init(struct xbps_handle *xhp)
{
	xbps_dictionary_t seen;
	int r = 0;

	assert(xhp);

	seen = xbps_dictionary_create();
	if (!seen)
		return xbps_error_oom();

	if (xhp->confdir[0]) {
		xbps_dbg_printf("Processing configuration directory: %s\n", xhp->confdir);
		r = parse_files_glob(xhp, seen, xhp->confdir, "*.conf", false);
		if (r < 0)
			goto out;
	}
	if (xhp->sysconfdir[0]) {
		xbps_dbg_printf("Processing system configuration directory: %s\n", xhp->sysconfdir);
		r = parse_files_glob(
		    xhp, seen, xhp->sysconfdir, "*.conf", false);
		if (r < 0)
			goto out;
	}

out:
	xbps_object_release(seen);
	return r;
}
