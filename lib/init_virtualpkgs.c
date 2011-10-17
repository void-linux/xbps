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
#include <dirent.h>

#include "xbps_api_impl.h"

/**
 * @file lib/init_virtualpkg.c
 * @brief Initialization of virtual package settings.
 */

void HIDDEN
xbps_init_virtual_pkgs(struct xbps_handle *xh)
{
	struct dirent *dp;
	DIR *dirp;
	prop_dictionary_t vpkgd;
	prop_string_t vpkgdir;
	char *vpkgfile;

	if (prop_object_type(xh->confdir) != PROP_TYPE_STRING) {
		vpkgdir = prop_string_create_cstring(XBPS_SYSCONF_PATH);
		prop_string_append_cstring(vpkgdir, "/");
		prop_string_append_cstring(vpkgdir, XBPS_VIRTUALPKGD_PATH);
	} else {
		vpkgdir = prop_string_copy(xh->confdir);
		prop_string_append_cstring(vpkgdir, "/");
		prop_string_append_cstring(vpkgdir, XBPS_VIRTUALPKGD_PATH);
	}

	/*
	 * Internalize all plist files from vpkgdir and add them
	 * into xhp->virtualpkgs_array.
	 */
	dirp = opendir(prop_string_cstring_nocopy(vpkgdir));
	if (dirp == NULL) {
		xbps_dbg_printf("%s: cannot access to %s for virtual "
		    "packages: %s\n", __func__,
		    prop_string_cstring_nocopy(vpkgdir),
		    strerror(errno));
		prop_object_release(vpkgdir);
		return;
	}
	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") == 0) ||
		    (strcmp(dp->d_name, "..") == 0))
			continue;

		if (strstr(dp->d_name, ".plist") == NULL)
			continue;

		vpkgfile = xbps_xasprintf("%s/%s",
		    prop_string_cstring_nocopy(vpkgdir), dp->d_name);
		if (vpkgfile == NULL) {
			(void)closedir(dirp);
			xbps_dbg_printf("%s: failed to alloc mem for %s\n",
			    __func__, dp->d_name);
			continue;
		}
		vpkgd = prop_dictionary_internalize_from_file(vpkgfile);
		free(vpkgfile);

		if (vpkgd == NULL) {
			xbps_dbg_printf("%s: failed to internalize %s: %s\n",
			    __func__, dp->d_name, strerror(errno));
			(void)closedir(dirp);
			continue;
		}
		if (prop_object_type(xh->virtualpkgs_array) == PROP_TYPE_UNKNOWN)
			xh->virtualpkgs_array = prop_array_create();

		if (!xbps_add_obj_to_array(xh->virtualpkgs_array, vpkgd)) {
			xbps_dbg_printf("%s: failed to add %s virtualpkg "
			    "dictionary!\n", __func__, dp->d_name);
			prop_object_release(vpkgd);
			(void)closedir(dirp);
			continue;
		}
		xbps_dbg_printf("%s: added virtualpkg from: %s\n",
		    __func__, dp->d_name);
	}
	(void)closedir(dirp);
	prop_object_release(vpkgdir);
}
