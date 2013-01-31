/*-
 * Copyright (c) 2011-2012 Juan Romero Pardines.
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
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>

#include "xbps_api_impl.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

/**
 * @file lib/initend.c
 * @brief Initialization and finalization routines
 * @defgroup initend Initialization and finalization functions
 *
 * Use these functions to initialize some parameters before start
 * using libxbps and finalize usage to release resources at the end.
 */
static char *
set_cachedir(struct xbps_handle *xh)
{
	if (xh->cachedir[0] == '/') {
		/* full path */
		return strdup(xh->cachedir);
	} else {
		/* relative to rootdir */
		if (strcmp(xh->rootdir, "/") == 0)
			return xbps_xasprintf("/%s", xh->cachedir);
		else
			return xbps_xasprintf("%s/%s", xh->rootdir,
			    xh->cachedir);
	}
}

static char *
set_metadir(struct xbps_handle *xh)
{
	if (xh->metadir == NULL) {
		if (strcmp(xh->rootdir, "/") == 0)
			return xbps_xasprintf("/%s", XBPS_META_PATH);
		else
			return xbps_xasprintf("%s/%s", xh->rootdir, XBPS_META_PATH);
	} else {
		return strdup(xh->metadir);
	}
}

static void
config_inject_vpkgs(struct xbps_handle *xh)
{
	DIR *dirp;
	struct dirent *dp;
	char *ext, *vpkgdir;
	FILE *fp;

	if (strcmp(xh->rootdir, "/"))
		vpkgdir = xbps_xasprintf("%s/etc/xbps/virtualpkg.d",
		    xh->rootdir);
	else
		vpkgdir = strdup("/etc/xbps/virtualpkg.d");

	if ((dirp = opendir(vpkgdir)) == NULL) {
		xbps_dbg_printf(xh, "cannot access to %s: %s\n",
		    vpkgdir, strerror(errno));
		return;
	}

	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, "..") == 0) ||
		    (strcmp(dp->d_name, ".") == 0))
			continue;
		/* only process .conf files, ignore something else */
		if ((ext = strrchr(dp->d_name, '.')) == NULL)
			continue;
		if (strcmp(ext, ".conf") == 0) {
			char *path;

			path = xbps_xasprintf("%s/%s", vpkgdir, dp->d_name);
			fp = fopen(path, "r");
			assert(fp);
			free(path);
			if (cfg_parse_fp(xh->cfg, fp) != 0) {
				xbps_error_printf("Failed to parse "
				    "vpkg conf file %s:\n", dp->d_name);
			}
			fclose(fp);
		}
	}
	closedir(dirp);
	free(vpkgdir);
}

static int
cb_validate_virtual(cfg_t *cfg, cfg_opt_t *opt)
{
	unsigned int i;

	for (i = 0; i < cfg_size(cfg, "virtual-package"); i++) {
		cfg_t *sec = cfg_opt_getnsec(opt, i);
		if (cfg_getstr(sec, "targets") == 0) {
			cfg_error(cfg, "targets must be set for "
			     "virtual-package %s", cfg_title(sec));
			return -1;
		}
	}
	return 0;
}

int
xbps_init(struct xbps_handle *xhp)
{
	cfg_opt_t vpkg_opts[] = {
		CFG_STR_LIST(__UNCONST("targets"), NULL, CFGF_NONE),
		CFG_END()
	};
	cfg_opt_t opts[] = {
		/* Defaults if not set in configuration file */
		CFG_STR(__UNCONST("rootdir"), __UNCONST("/"), CFGF_NONE),
		CFG_STR(__UNCONST("cachedir"),
		    __UNCONST(XBPS_CACHE_PATH), CFGF_NONE),
		CFG_INT(__UNCONST("FetchCacheConnections"),
		    XBPS_FETCH_CACHECONN, CFGF_NONE),
		CFG_INT(__UNCONST("FetchCacheConnectionsPerHost"),
		    XBPS_FETCH_CACHECONN_HOST, CFGF_NONE),
		CFG_INT(__UNCONST("FetchTimeoutConnection"),
		    XBPS_FETCH_TIMEOUT, CFGF_NONE),
		CFG_BOOL(__UNCONST("syslog"), true, CFGF_NONE),
		CFG_STR_LIST(__UNCONST("repositories"), NULL, CFGF_MULTI),
		CFG_STR_LIST(__UNCONST("PackagesOnHold"), NULL, CFGF_MULTI),
		CFG_SEC(__UNCONST("virtual-package"),
		    vpkg_opts, CFGF_MULTI|CFGF_TITLE),
		CFG_FUNC(__UNCONST("include"), &cfg_include),
		CFG_END()
	};
	struct utsname un;
	int rv, cc, cch;
	bool syslog_enabled = false;

	assert(xhp != NULL);

	if (xhp->initialized)
		return 0;

	if (xhp->conffile == NULL)
		xhp->conffile = XBPS_CONF_DEF;

	/* parse configuration file */
	xhp->cfg = cfg_init(opts, CFGF_NOCASE);
	cfg_set_validate_func(xhp->cfg, "virtual-package", &cb_validate_virtual);

	if ((rv = cfg_parse(xhp->cfg, xhp->conffile)) != CFG_SUCCESS) {
		if (rv == CFG_FILE_ERROR) {
			/*
			 * Don't error out if config file not found.
			 * If a default repository is set, use it; otherwise
			 * use defaults (no repos and no virtual packages).
			 */
			if (errno != ENOENT)
				return rv;

			xhp->conffile = NULL;
			if (xhp->repository) {
				char *buf;

				buf = xbps_xasprintf("repositories = { %s }",
				    xhp->repository);
				if ((rv = cfg_parse_buf(xhp->cfg, buf)) != 0)
					return rv;
				free(buf);
			}
		} else if (rv == CFG_PARSE_ERROR) {
			/*
			 * Parser error from configuration file.
			 */
			return ENOTSUP;
		}
	}

	xbps_dbg_printf(xhp, "Configuration file: %s\n",
	    xhp->conffile ? xhp->conffile : "not found");
	/*
	 * Respect client setting in struct xbps_handle for {root,cache}dir;
	 * otherwise use values from configuration file or defaults if unset.
	 */
	if (xhp->rootdir == NULL) {
		if (xhp->cfg == NULL)
			xhp->rootdir = "/";
		else
			xhp->rootdir = cfg_getstr(xhp->cfg, "rootdir");
	} else {
		if (xhp->rootdir[0] != '/') {
			/* relative path */
			char *buf, path[PATH_MAX-1];

			if (getcwd(path, sizeof(path)) == NULL)
				return ENOTSUP;

			buf = xbps_xasprintf("%s/%s", path, xhp->rootdir);
			xhp->rootdir = buf;
		}
	}

	if (xhp->cachedir == NULL) {
		if (xhp->cfg == NULL)
			xhp->cachedir = XBPS_CACHE_PATH;
		else
			xhp->cachedir = cfg_getstr(xhp->cfg, "cachedir");
	}
	if ((xhp->cachedir_priv = set_cachedir(xhp)) == NULL)
		return ENOMEM;
	xhp->cachedir = xhp->cachedir_priv;

	if ((xhp->metadir_priv = set_metadir(xhp)) == NULL)
		return ENOMEM;
	xhp->metadir = xhp->metadir_priv;

	uname(&un);
	xhp->un_machine = strdup(un.machine);
	assert(xhp->un_machine);

	if (xhp->cfg == NULL) {
		xhp->flags |= XBPS_FLAG_SYSLOG;
		xhp->fetch_timeout = XBPS_FETCH_TIMEOUT;
		cc = XBPS_FETCH_CACHECONN;
		cch = XBPS_FETCH_CACHECONN_HOST;
	} else {
		if (cfg_getbool(xhp->cfg, "syslog"))
			xhp->flags |= XBPS_FLAG_SYSLOG;
		xhp->fetch_timeout = cfg_getint(xhp->cfg, "FetchTimeoutConnection");
		cc = cfg_getint(xhp->cfg, "FetchCacheConnections");
		cch = cfg_getint(xhp->cfg, "FetchCacheConnectionsPerHost");
	}
	if (xhp->flags & XBPS_FLAG_SYSLOG)
		syslog_enabled = true;

	/* Inject virtual packages from virtualpkg.d files */
	config_inject_vpkgs(xhp);

	xbps_fetch_set_cache_connection(cc, cch);

	xbps_dbg_printf(xhp, "Rootdir=%s\n", xhp->rootdir);
	xbps_dbg_printf(xhp, "Metadir=%s\n", xhp->metadir);
	xbps_dbg_printf(xhp, "Cachedir=%s\n", xhp->cachedir);
	xbps_dbg_printf(xhp, "FetchTimeout=%u\n", xhp->fetch_timeout);
	xbps_dbg_printf(xhp, "FetchCacheconn=%u\n", cc);
	xbps_dbg_printf(xhp, "FetchCacheconnHost=%u\n", cch);
	xbps_dbg_printf(xhp, "Syslog=%u\n", syslog_enabled);
	xbps_dbg_printf(xhp, "Architecture: %s\n", xhp->un_machine);

	xhp->initialized = true;

	return 0;
}

void
xbps_end(struct xbps_handle *xhp)
{
	assert(xhp);

	if (!xhp->initialized)
		return;

	xbps_pkgdb_release(xhp);
	xbps_rpool_release(xhp);
	xbps_fetch_unset_cache_connection();
	if (xhp->pkgdb_revdeps != NULL)
		prop_object_release(xhp->pkgdb_revdeps);

	cfg_free(xhp->cfg);
	free(xhp->cachedir_priv);
	free(xhp->metadir_priv);
	free(xhp->un_machine);

	xhp->initialized = false;
	xhp = NULL;
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
