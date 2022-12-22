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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <ctype.h>
#include <glob.h>
#include <libgen.h>

#include "xbps_api_impl.h"

/**
 * @file lib/conf.c
 * @brief Configuration parsing
 * @defgroup conf Configuration parsing
 *
 * Functions for parsing xbps configuration files.
 */

static int
store_virtualpkg(struct xbps_handle *xhp, const char *path, size_t line, char *val)
{
	char *p;
	/*
	 * Parse strings delimited by ':' i.e
	 * 	<left>:<right>
	 */
	p = strchr(val, ':');
	if (p == NULL || p[1] == '\0') {
		xbps_dbg_printf(xhp, "%s: ignoring invalid "
		    "virtualpkg option at line %zu\n", path, line);
		return 0;
	}
	*p++ = '\0';

	if (!xbps_dictionary_set_cstring(xhp->vpkgd, val, p))
		return -errno;
	if (!xbps_dictionary_set_cstring(xhp->vpkgd_conf, val, p))
		return -errno;
	xbps_dbg_printf(xhp, "%s: added virtualpkg %s for %s\n", path, val, p);
	return 1;
}

static int
store_preserved_file(struct xbps_handle *xhp, const char *file)
{
	char buf[PATH_MAX];
	glob_t globbuf;
	int rv = 0;
	/*
	 * XXX: the globbing could be moved to transaction_files.c
	 * so that files are first checked against preserved patterns
	 * and then instead of globbing we just check if the file exists
	 * if it matches.
	 */

	if (xhp->preserved_files == NULL) {
		xhp->preserved_files = xbps_array_create();
		if (!xhp->preserved_files)
			return -errno;
	}

	if (xbps_path_join(buf, sizeof(buf), xhp->rootdir, file, (char *)NULL) == -1)
		return -errno;

	rv = glob(buf, 0, NULL, &globbuf);
	if (rv == GLOB_NOMATCH) {
		rv = 0;
		goto out;
	} else if (rv != 0) {
		rv = -errno;
		goto out;
	}
	for (size_t i = 0; i < globbuf.gl_pathc; i++) {
		const char *p = globbuf.gl_pathv[i];
		if (strcmp(xhp->rootdir, "/") != 0)
			p += strlen(xhp->rootdir);
		if (!xbps_array_add_cstring(xhp->preserved_files, p))
			return -errno;
		xbps_dbg_printf(xhp, "Added preserved file: %s (expanded from %s)\n", p, file);
	}
out:
	globfree(&globbuf);
	return rv;
}

static bool
store_repo(struct xbps_handle *xhp, const char *repo)
{
	if (xhp->flags & XBPS_FLAG_IGNORE_CONF_REPOS)
		return false;

	return xbps_repo_store(xhp, repo);
}

static void
store_ignored_pkg(struct xbps_handle *xhp, const char *pkgname)
{
	if (xhp->ignored_pkgs == NULL) {
		xhp->ignored_pkgs = xbps_array_create();
		assert(xhp->ignored_pkgs);
	}
	xbps_array_add_cstring(xhp->ignored_pkgs, pkgname);
	xbps_dbg_printf(xhp, "Added ignored package: %s\n", pkgname);
}

static void
store_noextract(struct xbps_handle *xhp, const char *value)
{
	if (*value == '\0')
		return;
	if (xhp->noextract == NULL) {
		xhp->noextract = xbps_array_create();
		assert(xhp->noextract);
	}
	xbps_array_add_cstring(xhp->noextract, value);
	xbps_dbg_printf(xhp, "Added noextract pattern: %s\n", value);
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
	size_t len;
	char *p;
	struct key needle, *result;

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
	char tmppath[PATH_MAX];
	glob_t globbuf;
	int rs, rv = 0, rv2;

	rs = snprintf(tmppath, PATH_MAX, "%s/%s",
	    pat[0] == '/' ? xhp->rootdir : cwd, pat);
	if (rs < 0 || rs >= PATH_MAX)
		return ENOMEM;

	switch (glob(tmppath, 0, NULL, &globbuf)) {
	case 0: break;
	case GLOB_NOSPACE: return ENOMEM;
	case GLOB_NOMATCH: return 0;
	default: return 0;
	}
	for (size_t i = 0; i < globbuf.gl_pathc; i++) {
		if (seen != NULL) {
			const char *fname;
			bool mask = false;
			fname = basename(globbuf.gl_pathv[i]);
			if (xbps_dictionary_get_bool(seen, fname, &mask) && mask)
				continue;
			xbps_dictionary_set_bool(seen, fname, true);
		}
		if ((rv2 = parse_file(xhp, globbuf.gl_pathv[i], nested)) != 0)
			rv = rv2;
	}
	globfree(&globbuf);

	return rv;
}

static int
parse_file(struct xbps_handle *xhp, const char *path, bool nested)
{
	FILE *fp;
	size_t len, nlines = 0;
	ssize_t rd;
	char *line = NULL;
	int rv = 0;
	int size, rs;
	char *dir;

	if ((fp = fopen(path, "r")) == NULL) {
		rv = errno;
		xbps_error_printf("cannot read configuration file %s: %s\n", path, strerror(rv));
		return rv;
	}

	xbps_dbg_printf(xhp, "Parsing configuration file: %s\n", path);

	while ((rd = getline(&line, &len, fp)) != -1) {
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
			xbps_dbg_printf(xhp, "%s: ignoring invalid option at "
			    "line %zu\n", path, nlines);
			continue;
		case KEY_ROOTDIR:
			size = sizeof xhp->rootdir;
			rs = snprintf(xhp->rootdir, size, "%s", val);
			if (rs < 0 || rs >= size) {
				rv = ENOMEM;
				break;
			}
			xbps_dbg_printf(xhp, "%s: rootdir set to %s\n", path, val);
			break;
		case KEY_CACHEDIR:
			size = sizeof xhp->cachedir;
			rs = snprintf(xhp->cachedir, size, "%s", val);
			if (rs < 0 || rs >= size) {
				rv = ENOMEM;
				break;
			}
			xbps_dbg_printf(xhp, "%s: cachedir set to %s\n", path, val);
			break;
		case KEY_ARCHITECTURE:
			size = sizeof xhp->native_arch;
			rs = snprintf(xhp->native_arch, size, "%s", val);
			if (rs < 0 || rs >= size) {
				rv = ENOMEM;
				break;
			}
			xbps_dbg_printf(xhp, "%s: native architecture set to %s\n", path,
			    val);
			break;
		case KEY_SYSLOG:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags &= ~XBPS_FLAG_DISABLE_SYSLOG;
				xbps_dbg_printf(xhp, "%s: syslog enabled\n", path);
			} else {
				xhp->flags |= XBPS_FLAG_DISABLE_SYSLOG;
				xbps_dbg_printf(xhp, "%s: syslog disabled\n", path);
			}
			break;
		case KEY_REPOSITORY:
			if (store_repo(xhp, val))
				xbps_dbg_printf(xhp, "%s: added repository %s\n", path, val);
			break;
		case KEY_VIRTUALPKG:
			rv = store_virtualpkg(xhp, path, nlines, val);
			if (rv < 0) {
				rv = -rv;
				break;
			}
			rv = 0;
			break;
		case KEY_PRESERVE:
			rv = store_preserved_file(xhp, val);
			if (rv < 0) {
				rv = -rv;
				break;
			}
			break;
		case KEY_KEEPCONF:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags |= XBPS_FLAG_KEEP_CONFIG;
				xbps_dbg_printf(xhp, "%s: config preservation enabled\n", path);
			} else {
				xhp->flags &= ~XBPS_FLAG_KEEP_CONFIG;
				xbps_dbg_printf(xhp, "%s: config preservation disabled\n", path);
			}
			break;
		case KEY_BESTMATCHING:
			if (strcasecmp(val, "true") == 0) {
				xhp->flags |= XBPS_FLAG_BESTMATCH;
				xbps_dbg_printf(xhp, "%s: pkg best matching enabled\n", path);
			} else {
				xhp->flags &= ~XBPS_FLAG_BESTMATCH;
				xbps_dbg_printf(xhp, "%s: pkg best matching disabled\n", path);
			}
			break;
		case KEY_IGNOREPKG:
			store_ignored_pkg(xhp, val);
			break;
		case KEY_NOEXTRACT:
			store_noextract(xhp, val);
			break;
		case KEY_INCLUDE:
			/* Avoid double-nested parsing, only allow it once */
			if (nested) {
				xbps_dbg_printf(xhp, "%s: ignoring nested include\n", path);
				continue;
			}
			dir = strdup(path);
			rv = parse_files_glob(xhp, NULL, dirname(dir), val, true);
			free(dir);
			break;
		}
	}
	free(line);
	fclose(fp);

	return rv;
}

int HIDDEN
xbps_conf_init(struct xbps_handle *xhp)
{
	xbps_dictionary_t seen;
	int rv = 0;

	assert(xhp);
	seen = xbps_dictionary_create();
	if (!seen)
		return errno;

	if (*xhp->confdir) {
		xbps_dbg_printf(xhp, "Processing configuration directory: %s\n", xhp->confdir);
		if ((rv = parse_files_glob(xhp, seen, xhp->confdir, "*.conf", false)))
			goto out;
	}
	if (*xhp->sysconfdir) {
		xbps_dbg_printf(xhp, "Processing system configuration directory: %s\n", xhp->sysconfdir);
		if ((rv = parse_files_glob(xhp, seen, xhp->sysconfdir, "*.conf", false)))
			goto out;
	}

out:
	xbps_object_release(seen);
	return rv;
}
