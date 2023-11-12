/*-
 * Copyright (c) 2011-2015 Juan Romero Pardines.
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/param.h>

#include <xbps.h>
#include "defs.h"

/*
 * Checks package integrity of an installed package.
 * The following task is accomplished in this file:
 *
 * 	o Check for missing run time dependencies.
 *
 * Returns 0 if test ran successfully, 1 otherwise and -1 on error.
 */

int
check_pkg_rundeps(struct xbps_handle *xhp, const char *pkgname, void *arg)
{
	xbps_dictionary_t pkg_propsd = arg;
	xbps_array_t array;
	const char *reqpkg = NULL;
	int rv = 0;

	array = xbps_dictionary_get(pkg_propsd, "run_depends");
	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		xbps_array_get_cstring_nocopy(array, i, &reqpkg);
		if (xbps_pkg_is_ignored(xhp, reqpkg))
			continue;
		if (xbps_pkg_is_installed(xhp, reqpkg) <= 0) {
			xbps_error_printf("%s: dependency not satisfied: %s\n",
			    pkgname, reqpkg);
			rv = -1;
		}
	}
	return rv;
}
