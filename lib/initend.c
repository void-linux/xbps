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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

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
	const char *native_arch = NULL, *p;
	int rv = 0;

	assert(xhp != NULL);

	if (xhp->flags & XBPS_FLAG_DEBUG)
		xbps_debug_level = 1;

	xbps_dbg_printf("%s\n", XBPS_RELVER);

	/* Set rootdir */
	if (xhp->rootdir[0] == '\0') {
		xhp->rootdir[0] = '/';
		xhp->rootdir[1] = '\0';
	} else if (xhp->rootdir[0] != '/') {
		char cwd[XBPS_MAXPATH];

		if (getcwd(cwd, sizeof(cwd)) == NULL)
			return ENOBUFS;
		if (xbps_path_prepend(xhp->rootdir, sizeof xhp->rootdir, cwd) == -1)
			return ENOBUFS;
	}
	if (xbps_path_clean(xhp->rootdir) == -1)
		return ENOTSUP;

	/* set confdir */
	if (xhp->confdir[0] == '\0') {
		if (xbps_path_join(xhp->confdir, sizeof xhp->confdir,
		    xhp->rootdir, XBPS_SYSCONF_PATH, (char *)NULL) == -1)
			return ENOBUFS;
	} else if (xhp->confdir[0] != '/') {
		/* relative path */
		if (xbps_path_prepend(xhp->confdir, sizeof xhp->confdir,
		    xhp->rootdir) == -1)
			return ENOBUFS;
	}
	if (xbps_path_clean(xhp->confdir) == -1)
		return ENOTSUP;

	/* set sysconfdir */
	if (xhp->sysconfdir[0] == '\0') {
		if (xbps_path_join(xhp->sysconfdir, sizeof xhp->sysconfdir,
		    xhp->rootdir, XBPS_SYSDEFCONF_PATH, (char *)NULL) == -1)
			return ENOBUFS;
	}
	if (xbps_path_clean(xhp->sysconfdir) == -1)
		return ENOTSUP;

	xbps_fetch_set_cache_connection(XBPS_FETCH_CACHECONN, XBPS_FETCH_CACHECONN_HOST);

	xhp->vpkgd = xbps_dictionary_create();
	if (xhp->vpkgd == NULL)
		return errno ? errno : ENOMEM;
	xhp->vpkgd_conf = xbps_dictionary_create();
	if (xhp->vpkgd_conf == NULL)
		return errno ? errno : ENOMEM;

	/* process xbps.d directories */
	if ((rv = xbps_conf_init(xhp)) != 0)
		return rv;

	/* target arch only through env var */
	xhp->target_arch = getenv("XBPS_TARGET_ARCH");
	if (xhp->target_arch && *xhp->target_arch == '\0')
		xhp->target_arch = NULL;

	/* allow to overwrite uname(3) and conf file with env variable */
	if ((native_arch = getenv("XBPS_ARCH")) && *native_arch != '\0') {
		if (xbps_strlcpy(xhp->native_arch, native_arch,
		    sizeof xhp->native_arch) >= sizeof xhp->native_arch)
			return ENOBUFS;
	}

	if (*xhp->native_arch == '\0') {
		struct utsname un;
		if (uname(&un) == -1)
			return ENOTSUP;
		if (xbps_strlcpy(xhp->native_arch, un.machine,
			sizeof xhp->native_arch) >= sizeof xhp->native_arch)
			return ENOBUFS;
	}
	assert(*xhp->native_arch);

	/* Set cachedir */
	if (xhp->cachedir[0] == '\0') {
		if (xbps_path_join(xhp->cachedir, sizeof xhp->cachedir,
		    xhp->rootdir, XBPS_CACHE_PATH, (char *)NULL) == -1)
			return ENOBUFS;
	} else if (xhp->cachedir[0] != '/') {
		/* relative path */
		if (xbps_path_prepend(xhp->cachedir, sizeof xhp->cachedir,
		    xhp->rootdir) == -1)
			return ENOBUFS;
	}
	if (xbps_path_clean(xhp->cachedir) == -1)
		return ENOTSUP;

	/* Set metadir */
	if (xhp->metadir[0] == '\0') {
		if (xbps_path_join(xhp->metadir, sizeof xhp->metadir,
		    xhp->rootdir, XBPS_META_PATH, (char *)NULL) == -1)
			return ENOBUFS;
	} else if (xhp->metadir[0] != '/') {
		/* relative path */
		if (xbps_path_prepend(xhp->metadir, sizeof xhp->metadir,
		    xhp->rootdir) == -1)
			return ENOBUFS;
	}
	if (xbps_path_clean(xhp->metadir) == -1)
		return ENOTSUP;

	p = getenv("XBPS_SYSLOG");
	if (p) {
		if (strcasecmp(p, "true") == 0)
			xhp->flags &= ~XBPS_FLAG_DISABLE_SYSLOG;
		else if (strcasecmp(p, "false") == 0)
			xhp->flags |= XBPS_FLAG_DISABLE_SYSLOG;
	}

	if (xhp->flags & XBPS_FLAG_DEBUG) {
		const char *repodir;
		for (unsigned int i = 0; i < xbps_array_count(xhp->repositories); i++) {
			if (!xbps_array_get_cstring_nocopy(xhp->repositories, i, &repodir))
			    return errno;
			xbps_dbg_printf("Repository[%u]=%s\n", i, repodir);
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
