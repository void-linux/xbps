/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "xbps_api_impl.h"

/**
 * @file lib/initend.c
 * @brief Initialization and finalization routines
 * @defgroup initend Initialization and finalization functions
 *
 * Use these functions to initialize some parameters before start
 * using libxbps and finalize usage to release resources at the end.
 */

int
xbps_init(struct xbps_handle *xhp)
{
	char *buf = NULL;
	const char *repodir = NULL, *native_arch = NULL;
	int rv = 0;
	size_t size;

	assert(xhp != NULL);

	/* Set rootdir */
	if (xhp->rootdir[0] == '\0') {
		xhp->rootdir[0] = '/';
		xhp->rootdir[1] = '\0';
	} else if (xhp->rootdir[0] != '/') {
		char cwd[PATH_MAX-1];
		/* get cwd */
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			return ENOTSUP;
		buf = strdup(xhp->rootdir);
		if (!buf)
			return ENOMEM;
		size = sizeof(xhp->rootdir);
		rv = snprintf(xhp->rootdir, size, "%s/%s", cwd, buf);
		free(buf);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	}

	xbps_dbg_printf(xhp, "%s\n", XBPS_RELVER);

	/* set confdir */
	if (xhp->confdir[0] == '\0') {
		size = sizeof(xhp->confdir);
		rv = snprintf(xhp->confdir, size,
		    "%s%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
		    XBPS_SYSCONF_PATH);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	} else if (xhp->confdir[0] != '/') {
		/* relative path */
		buf = strdup(xhp->confdir);
		if (!buf)
			return ENOMEM;
		size = sizeof(xhp->confdir);
		rv = snprintf(xhp->confdir, size, "%s/%s",
		    strcmp(xhp->rootdir, "/") ? xhp->rootdir : "", buf);
		free(buf);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	}

	/* set sysconfdir */
	if (xhp->sysconfdir[0] == '\0') {
		size = sizeof(xhp->sysconfdir);
		snprintf(xhp->sysconfdir, size,
			"%s%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
			XBPS_SYSDEFCONF_PATH);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	}

	/* set architecture and target architecture */
	xhp->target_arch = getenv("XBPS_TARGET_ARCH");
	if ((native_arch = getenv("XBPS_ARCH")) != NULL) {
		xbps_strlcpy(xhp->native_arch, native_arch, sizeof (xhp->native_arch));
	} else {
#if defined(__linux__) && !defined(__GLIBC__)
		/* musl libc on linux */
		char *s = NULL;
#endif
		struct utsname un;
		if (uname(&un) == -1)
			return ENOTSUP;
#if defined(__linux__) && !defined(__GLIBC__)
		/* musl libc on linux */
		s = xbps_xasprintf("%s-musl", un.machine);
		assert(s);
		xbps_strlcpy(xhp->native_arch, s, sizeof(xhp->native_arch));
		free(s);
#else
		/* glibc or any other os */
		xbps_strlcpy(xhp->native_arch, un.machine, sizeof (xhp->native_arch));
#endif
	}
	assert(xhp->native_arch);

	xbps_fetch_set_cache_connection(XBPS_FETCH_CACHECONN, XBPS_FETCH_CACHECONN_HOST);

	/* process xbps.d directories */
	if ((rv = xbps_conf_init(xhp)) != 0)
		return rv;

	/* Set cachedir */
	if (xhp->cachedir[0] == '\0') {
		size = sizeof(xhp->cachedir);
		rv = snprintf(xhp->cachedir, size,
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
		    XBPS_CACHE_PATH);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	} else if (xhp->cachedir[0] != '/') {
		/* relative path */
		buf = strdup(xhp->cachedir);
		if (!buf)
			return ENOMEM;
		size = sizeof(xhp->cachedir);
		rv = snprintf(xhp->cachedir, size,
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "", buf);
		free(buf);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	}
	/* Set metadir */
	if (xhp->metadir[0] == '\0') {
		size = sizeof(xhp->metadir);
		rv = snprintf(xhp->metadir, size,
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "",
		    XBPS_META_PATH);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	} else if (xhp->metadir[0] != '/') {
		/* relative path */
		buf = strdup(xhp->metadir);
		if (!buf)
			return ENOMEM;
		size = sizeof(xhp->metadir);
		rv = snprintf(xhp->metadir, size,
		    "%s/%s", strcmp(xhp->rootdir, "/") ? xhp->rootdir : "", buf);
		free(buf);
		if (rv < 0 || (size_t)rv >= size)
			return 1;
	}

	xbps_dbg_printf(xhp, "rootdir=%s\n", xhp->rootdir);
	xbps_dbg_printf(xhp, "metadir=%s\n", xhp->metadir);
	xbps_dbg_printf(xhp, "cachedir=%s\n", xhp->cachedir);
	xbps_dbg_printf(xhp, "confdir=%s\n", xhp->confdir);
	xbps_dbg_printf(xhp, "sysconfdir=%s\n", xhp->sysconfdir);
	xbps_dbg_printf(xhp, "syslog=%s\n", xhp->flags & XBPS_FLAG_DISABLE_SYSLOG ? "false" : "true");
	xbps_dbg_printf(xhp, "bestmatching=%s\n", xhp->flags & XBPS_FLAG_BESTMATCH ? "true" : "false");
	xbps_dbg_printf(xhp, "keepconf=%s\n", xhp->flags & XBPS_FLAG_KEEP_CONFIG ? "true" : "false");
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
}
