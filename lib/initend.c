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

int
xbps_init(struct xbps_handle *xh)
{
	const char *conf_rootdir = NULL, *conf_cachedir = NULL;
	uint16_t fetch_cache_conn = 0, fetch_cache_conn_host = 0;
	int rv;

	assert(xh != NULL);

	xhp = xh;
	debug = xhp->with_debug;

	/* If conffile not set, defaults to XBPS_CONF_PATH */
	if (xhp->conffile == NULL)
		xhp->conffile = XBPS_CONF_PATH "/" XBPS_CONF_PLIST;

	/*
	 * Internalize the XBPS_CONF_PLIST dictionary.
	 */
	xhp->conf_dictionary =
	    prop_dictionary_internalize_from_file(xhp->conffile);
	if (xhp->conf_dictionary == NULL) {
		if (errno != ENOENT) {
			xbps_dbg_printf("%s: cannot internalize conf "
			    "dictionary: %s\n", strerror(errno));
			xbps_end();
			return errno;
		}
		xbps_dbg_printf("%s: conf_dictionary not internalized.\n",
		    __func__);
	} else {
		/*
		 * Get defaults from configuration file.
		 */
		prop_dictionary_get_cstring_nocopy(xhp->conf_dictionary,
		    "root-directory", &conf_rootdir);
		prop_dictionary_get_cstring_nocopy(xhp->conf_dictionary,
		    "cache-directory", &conf_cachedir);
		prop_dictionary_get_uint16(xhp->conf_dictionary,
		    "fetch-cache-connections", &fetch_cache_conn);
		prop_dictionary_get_uint16(xhp->conf_dictionary,
		    "fetch-cache-connections-per-host", &fetch_cache_conn_host);
	}

	/*
	 * Client supplied values in xbps_handle will be choosen over the
	 * same values in configuration file. If not specified, use defaults.
	 */
	if (xhp->rootdir == NULL) {
		if (conf_rootdir != NULL)
			xhp->rootdir = conf_rootdir;
		else {
			/* If rootdir not set, defaults to '/' */
			xhp->rootdir = "/";
		}
	}
	if (xhp->cachedir == NULL) {
		if (conf_cachedir != NULL)
			xhp->cachedir = conf_cachedir;
		else {
			/* If cachedir not set, defaults to XBPS_CACHE_PATH */
			xhp->cachedir = XBPS_CACHE_PATH;
		}
	}
	if (fetch_cache_conn == 0)
		fetch_cache_conn = XBPS_FETCH_CACHECONN;
	if (fetch_cache_conn_host != 0)
		fetch_cache_conn_host = XBPS_FETCH_CACHECONN_HOST;

	xbps_fetch_set_cache_connection(fetch_cache_conn,
	    fetch_cache_conn_host);

	xbps_dbg_printf("%s: rootdir: `%s' cachedir: `%s' conf: `%s'\n",
	    __func__, xhp->rootdir, xhp->cachedir, xhp->conffile);
	xbps_dbg_printf("%s: fetch_cache_conn: %zu fetch_cache_host: %zu\n",
	    __func__, fetch_cache_conn, fetch_cache_conn_host);

	/*
	 * Initialize repository pool.
	 */
	if ((rv = xbps_repository_pool_init()) != 0) {
		if (rv == ENOTSUP) {
			xbps_dbg_printf("%s: empty repository list.\n",
			    __func__);
		} else if (rv != ENOENT && rv != ENOTSUP) {
			xbps_dbg_printf("%s: couldn't initialize "
			    "repository pool: %s\n", strerror(rv));
			xbps_end();
			return rv;
		}
	}
	/*
	 * Initialize regpkgdb dictionary.
	 */
        if ((rv = xbps_regpkgdb_dictionary_init(xhp)) != 0) {
               if (rv != ENOENT) {
		       xbps_dbg_printf("%s: couldn't initialize "
			    "regpkgdb: %s\n", strerror(rv));
		       xbps_end();
		       return rv;
	       }
	}

	return 0;
}

void
xbps_end(void)
{
	xbps_regpkgdb_dictionary_release();
	xbps_repository_pool_release();
	xbps_fetch_unset_cache_connection();
	if (xhp == NULL)
		return;

	if (prop_object_type(xhp->conf_dictionary) == PROP_TYPE_DICTIONARY)
		prop_object_release(xhp->conf_dictionary);

	xhp = NULL;
}

const struct xbps_handle *
xbps_handle_get(void)
{
	assert(xhp != NULL);
	return xhp;
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

void
xbps_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	common_printf(stdout, NULL, fmt, ap);
	va_end(ap);
}
