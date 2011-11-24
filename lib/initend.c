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

#define _CONFFILE	XBPS_SYSCONF_PATH "/" XBPS_CONF_PLIST
#define _REPOFILE	XBPS_SYSCONF_PATH "/" XBPS_CONF_REPOS_PLIST

int
xbps_init(struct xbps_handle *xh)
{
	prop_dictionary_t confd;
	prop_string_t conffile = NULL, repofile = NULL;
	const char *conf_rootdir = NULL, *conf_cachedir = NULL;
	uint16_t fetch_cache_conn = 0, fetch_cache_conn_host = 0;
	int rv;

	assert(xh != NULL);

	xhp = xh;
	debug = xhp->debug;

	/* If confdir not set, defaults to XBPS_SYSCONF_PATH */
	if (prop_object_type(xhp->confdir) != PROP_TYPE_STRING) {
		conffile = prop_string_create_cstring(_CONFFILE);
		repofile = prop_string_create_cstring(_REPOFILE);
	} else {
		conffile = prop_string_copy(xhp->confdir);
		prop_string_append_cstring(conffile, "/");
		prop_string_append_cstring(conffile, XBPS_CONF_PLIST);

		repofile = prop_string_copy(xhp->confdir);
		prop_string_append_cstring(repofile, "/");
		prop_string_append_cstring(repofile, XBPS_CONF_REPOS_PLIST);
	}
	/*
	 * Internalize the XBPS_CONF_REPOS_PLIST array.
	 */
	xhp->repos_array =
	    prop_array_internalize_from_file(prop_string_cstring_nocopy(repofile));
	/*
	 * Internalize the XBPS_CONF_PLIST dictionary.
	 */
	confd = prop_dictionary_internalize_from_file(
	    prop_string_cstring_nocopy(conffile));
	if (confd == NULL) {
		if (errno != ENOENT) {
			xbps_dbg_printf("%s: cannot internalize conf "
			    "dictionary: %s\n", strerror(errno));
			xbps_end(xh);
			return errno;
		}
		xbps_dbg_printf("%s: conf_dictionary not internalized.\n",
		    __func__);
	} else {
		/*
		 * Get defaults from configuration file.
		 */
		prop_dictionary_get_cstring_nocopy(confd,
		    "root-directory", &conf_rootdir);
		prop_dictionary_get_cstring_nocopy(confd,
		    "cache-directory", &conf_cachedir);
		prop_dictionary_get_uint16(confd,
		    "fetch-cache-connections", &fetch_cache_conn);
		prop_dictionary_get_uint16(confd,
		    "fetch-cache-connections-per-host", &fetch_cache_conn_host);
		prop_dictionary_get_uint16(confd,
		    "fetch-timeout-connection", &xhp->fetch_timeout);
	}

	/*
	 * Client supplied values in xbps_handle will be choosen over the
	 * same values in configuration file. If not specified, use defaults.
	 */
	if (prop_object_type(xhp->rootdir) != PROP_TYPE_STRING) {
		if (conf_rootdir != NULL)
			xhp->rootdir = prop_string_create_cstring(conf_rootdir);
		else {
			/* If rootdir not set, defaults to '/' */
			xhp->rootdir = prop_string_create_cstring("/");
		}
	}
	if (prop_object_type(xhp->cachedir) != PROP_TYPE_STRING) {
		if (conf_cachedir != NULL) {
			if (conf_cachedir[0] == '/') {
				/* full path */
				xhp->cachedir =
				    prop_string_create_cstring(conf_cachedir);
			} else {
				/* relative to rootdir */
				xhp->cachedir = prop_string_copy(xhp->rootdir);
				prop_string_append_cstring(
				    xhp->cachedir, "/");
				prop_string_append_cstring(
				    xhp->cachedir, conf_cachedir);
			}
		} else {
			/* If cachedir not set, defaults to XBPS_CACHE_PATH */
			xhp->cachedir =
			    prop_string_create_cstring(XBPS_CACHE_PATH);
		}
	}
	if (fetch_cache_conn == 0)
		fetch_cache_conn = XBPS_FETCH_CACHECONN;
	if (fetch_cache_conn_host == 0)
		fetch_cache_conn_host = XBPS_FETCH_CACHECONN_HOST;

	xbps_fetch_set_cache_connection(fetch_cache_conn,
	    fetch_cache_conn_host);

	xbps_dbg_printf("rootdir: %s\n",
	    prop_string_cstring_nocopy(xhp->rootdir));
	xbps_dbg_printf("cachedir: %s\n",
	    prop_string_cstring_nocopy(xhp->cachedir));
	xbps_dbg_printf("conffile: %s\n",
	    prop_string_cstring_nocopy(conffile));
	xbps_dbg_printf("repofile: %s\n",
	    prop_string_cstring_nocopy(repofile));
	xbps_dbg_printf("fetch_cache_conn: %zu\n",
	    fetch_cache_conn);
	xbps_dbg_printf("fetch_cache_conn_host: %zu\n",
	    fetch_cache_conn_host);
	xbps_dbg_printf("fetch_timeout: %zu\n",
	    xhp->fetch_timeout);

	/*
	 * Initialize regpkgdb dictionary.
	 */
        if ((rv = xbps_regpkgdb_dictionary_init(xhp)) != 0) {
               if (rv != ENOENT) {
		       xbps_dbg_printf("%s: couldn't initialize "
			    "regpkgdb: %s\n", strerror(rv));
		       xbps_end(xh);
		       return rv;
	       }
	}
	if (prop_object_type(confd) == PROP_TYPE_DICTIONARY)
		prop_object_release(confd);
	if (prop_object_type(conffile) == PROP_TYPE_STRING)
		prop_object_release(conffile);
	if (prop_object_type(repofile) == PROP_TYPE_STRING)
		prop_object_release(repofile);

	/* Initialize virtual package settings */
	xbps_init_virtual_pkgs(xhp);

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
	if (prop_object_type(xh->confdir) == PROP_TYPE_STRING)
		prop_object_release(xh->confdir);
	if (prop_object_type(xh->rootdir) == PROP_TYPE_STRING)
		prop_object_release(xh->rootdir);
	if (prop_object_type(xh->cachedir) == PROP_TYPE_STRING)
		prop_object_release(xh->cachedir);
	if (prop_object_type(xh->virtualpkgs_array) == PROP_TYPE_ARRAY)
		prop_object_release(xh->virtualpkgs_array);

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
