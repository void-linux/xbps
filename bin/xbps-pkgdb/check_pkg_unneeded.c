/*-
 * Copyright (c) 2014 Juan Romero Pardines.
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
 * 	o Check if pkg dictionary from pkgdb contains "unneeded" objects,
 * 	  and remove them if that was true.
 */
int
check_pkg_unneeded(struct xbps_handle *xhp UNUSED, const char *pkgname, void *arg)
{
	xbps_array_t replaces;
	xbps_dictionary_t pkgd = arg;
	const char *repo = NULL;
	char *buf;

	xbps_dictionary_remove(pkgd, "download");
	xbps_dictionary_remove(pkgd, "remove-and-update");
	xbps_dictionary_remove(pkgd, "transaction");
	xbps_dictionary_remove(pkgd, "skip-obsoletes");
	xbps_dictionary_remove(pkgd, "packaged-with");
	if (xbps_dictionary_get_cstring_nocopy(pkgd, "repository-origin", &repo)) {
		xbps_dictionary_set_cstring(pkgd, "repository", repo);
		xbps_dictionary_remove(pkgd, "repository-origin");
	}
	/*
	 * Remove self replacement when applicable.
	 */
	if ((replaces = xbps_dictionary_get(pkgd, "replaces"))) {
		buf = xbps_xasprintf("%s>=0", pkgname);
		xbps_remove_string_from_array(replaces, buf);
		free(buf);
		if (!xbps_array_count(replaces))
			xbps_dictionary_remove(pkgd, "replaces");
	}

	return 0;
}
