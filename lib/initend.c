/*-
 * Copyright (c) 2011 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "xbps_api_impl.h"

/**
 * @file lib/initend.c
 * @brief Initialization and finalization routines
 * @defgroup initend Initialization and finalization functions
 *
 * Use these functions to initialize some parameters before start
 * using libxbps and finalize usage to release resources at the end.
 */
static bool debug;
static struct xbps_handle *xhp;

static void
get_cachedir(struct xbps_handle *xh)
{
	if (xh->cachedir[0] == '/')
		/* full path */
		xh->cachedir_priv = strdup(xh->cachedir);
	else {
		/* relative to rootdir */
		if (strcmp(xh->rootdir, "/") == 0)
			xh->cachedir_priv =
			    xbps_xasprintf("/%s", xh->cachedir);
		else
			xh->cachedir_priv =
			    xbps_xasprintf("%s/%s", xh->rootdir,
			    xh->cachedir);
	}
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
xbps_init(struct xbps_handle *xh)
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
		CFG_INT(__UNCONST("fetch-cache-connections"),
		    XBPS_FETCH_CACHECONN, CFGF_NONE),
		CFG_INT(__UNCONST("fetch-cache-connections-per-host"),
		    XBPS_FETCH_CACHECONN_HOST, CFGF_NONE),
		CFG_INT(__UNCONST("fetch-timeout-connection"),
		    XBPS_FETCH_TIMEOUT, CFGF_NONE),
		CFG_BOOL(__UNCONST("syslog"), true, CFGF_NONE),
		CFG_STR_LIST(__UNCONST("repositories"), NULL, CFGF_MULTI),
		CFG_SEC(__UNCONST("virtual-package"),
		    vpkg_opts, CFGF_MULTI|CFGF_TITLE),
		CFG_FUNC(__UNCONST("include"), &cfg_include),
		CFG_END()
	};
	int rv, cc, cch;

	assert(xh != NULL);

	xhp = xh;
	debug = xhp->debug;

	if (xhp->conffile == NULL)
		xhp->conffile = XBPS_CONF_DEF;

	/* parse configuration file */
	xhp->cfg = cfg_init(opts, CFGF_NOCASE);
	cfg_set_validate_func(xhp->cfg, "virtual-package", &cb_validate_virtual);

	if ((rv = cfg_parse(xhp->cfg, xhp->conffile)) != CFG_SUCCESS) {
		if (rv == CFG_PARSE_ERROR) {
			if (errno != ENOENT) {
				/*
				 * Don't error out if config file not found.
				 * We'll use defaults without any repo or
				 * virtual packages.
				 */
				xbps_end(xh);
				return rv;
			}
			errno = 0;
		} else if (rv == CFG_PARSE_ERROR) {
			/*
			 * Parser error from configuration file.
			 */
			xbps_end(xh);
			return ENOTSUP;
		}
	}
	/*
	 * Respect client setting in struct xbps_handle for {root,cache}dir;
	 * otherwise use values from configuration file or defaults if unset.
	 */
	if (xhp->rootdir == NULL) {
		if (xhp->cfg == NULL)
			xhp->rootdir = "/";
		else
			xhp->rootdir = cfg_getstr(xhp->cfg, "rootdir");
	}
	if (xhp->cachedir == NULL) {
		if (xhp->cfg == NULL)
			xhp->cachedir = XBPS_CACHE_PATH;
		else
			xhp->cachedir = cfg_getstr(xhp->cfg, "cachedir");
	}
	get_cachedir(xhp);
	if (xhp->cachedir_priv == NULL) {
		xbps_end(xh);
		return ENOMEM;
	}
	xhp->cachedir = xhp->cachedir_priv;

	if (xhp->cfg == NULL) {
		xhp->syslog_enabled = true;
		xhp->fetch_timeout = XBPS_FETCH_TIMEOUT;
		cc = XBPS_FETCH_CACHECONN;
		cch = XBPS_FETCH_CACHECONN_HOST;
	} else {
		xhp->syslog_enabled = cfg_getbool(xhp->cfg, "syslog");
		xhp->fetch_timeout = cfg_getint(xhp->cfg, "fetch-timeout-connection");
		cc = cfg_getint(xhp->cfg, "fetch-cache-connections");
		cch = cfg_getint(xhp->cfg, "fetch-cache-connections-per-host");
	}
	xbps_fetch_set_cache_connection(cc, cch);

	xbps_dbg_printf("rootdir=%s\n", xhp->rootdir);
	xbps_dbg_printf("cachedir=%s\n", xhp->cachedir);
	xbps_dbg_printf("fetch-timeout=%u\n", xhp->fetch_timeout);
	xbps_dbg_printf("fetch-cacheconn=%u\n", cc);
	xbps_dbg_printf("fetch-cacheconn-host=%u\n", cch);
	xbps_dbg_printf("syslog=%u\n", xhp->syslog_enabled);

	return 0;
}

void
xbps_end(struct xbps_handle *xh)
{
	xbps_regpkgdb_dictionary_release();
	xbps_repository_pool_release();
	xbps_fetch_unset_cache_connection();

	if (xh == NULL)
		return;
	if (xh->cfg != NULL)
		cfg_free(xh->cfg);
	if (xh->cachedir_priv != NULL)
		free(xh->cachedir_priv);

	free(xh);
	xh = NULL;
	xhp = NULL;
}

struct xbps_handle *
xbps_handle_get(void)
{
	assert(xhp != NULL);
	return xhp;
}

struct xbps_handle *
xbps_handle_alloc(void)
{
	return malloc(sizeof(struct xbps_handle));
}

static void
common_printf(FILE *f, const char *msg, const char *fmt, va_list ap)
{
	if (msg != NULL)
		fprintf(f, "%s", msg);

	vfprintf(f, fmt, ap);
}

void
xbps_dbg_printf_append(const char *fmt, ...)
{
	va_list ap;

	if (!debug)
		return;

	va_start(ap, fmt);
	common_printf(stderr, NULL, fmt, ap);
	va_end(ap);
}

void
xbps_dbg_printf(const char *fmt, ...)
{
	va_list ap;

	if (!debug)
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
