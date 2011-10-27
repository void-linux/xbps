/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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

#include <xbps_api.h>
#include "defs.h"

#define RUN_PKG_CHECK(name)					\
do {								\
	rv = check_pkg_##name(pkgd, propsd, filesd);		\
	if (rv)							\
		broken = true;					\
	else if (rv == -1) {					\
		xbps_error_printf("%s: the %s test "		\
		    "returned error!\n", pkgname, #name);	\
		goto out;					\
	}							\
} while (0)

int
check_pkg_integrity_all(void)
{
	const struct xbps_handle *xhp;
	prop_object_t obj;
	prop_object_iterator_t iter = NULL;
	const char *pkgname, *version;
	size_t npkgs = 0, nbrokenpkgs = 0;

	xhp = xbps_handle_get();
	iter = xbps_array_iter_from_dict(xhp->regpkgdb_dictionary, "packages");
	if (iter == NULL)
		return -1;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);
		prop_dictionary_get_cstring_nocopy(obj, "version", &version);
		printf("Checking %s-%s ...\n", pkgname, version);
		if (check_pkg_integrity(pkgname) != 0)
			nbrokenpkgs++;

		npkgs++;
	}
	prop_object_iterator_release(iter);

	printf("%zu package%s processed: %zu broken.\n", npkgs,
	    npkgs == 1 ? "" : "s", nbrokenpkgs);

	return 0;
}

int
check_pkg_integrity(const char *pkgname)
{
	prop_dictionary_t pkgd, propsd, filesd;
	int rv = 0;
	bool broken = false;

	/* find real pkg by name */
	pkgd = xbps_find_pkg_dict_installed(pkgname, false);
	if (pkgd == NULL) {
		/* find virtual pkg by name */
		pkgd = xbps_find_virtualpkg_dict_installed(pkgname, false);
	}
	if (pkgd == NULL) {
		printf("Package %s is not installed.\n", pkgname);
		return 0;
	}
	/*
	 * Check for props.plist metadata file.
	 */
	propsd = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGPROPS);
	if (prop_object_type(propsd) != PROP_TYPE_DICTIONARY) {
		xbps_error_printf("%s: unexistent %s or invalid metadata "
		    "file.\n", pkgname, XBPS_PKGPROPS);
		broken = true;
		goto out;
	} else if (prop_dictionary_count(propsd) == 0) {
		xbps_error_printf("%s: incomplete %s metadata file.\n",
		    pkgname, XBPS_PKGPROPS);
		broken = true;
		goto out;
	}
	/*
	 * Check for files.plist metadata file.
	 */
	filesd = xbps_dictionary_from_metadata_plist(pkgname, XBPS_PKGFILES);
	if (prop_object_type(filesd) != PROP_TYPE_DICTIONARY) {
		xbps_error_printf("%s: unexistent %s or invalid metadata "
		    "file.\n", pkgname, XBPS_PKGFILES);
		broken = true;
		goto out;
	} else if (prop_dictionary_count(filesd) == 0) {
		xbps_error_printf("%s: incomplete %s metadata file.\n",
		    pkgname, XBPS_PKGFILES);
		broken = true;
		goto out;
	}

	/* Execute pkg checks */
	RUN_PKG_CHECK(autoinstall);
	RUN_PKG_CHECK(files);
	RUN_PKG_CHECK(symlinks);
	RUN_PKG_CHECK(rundeps);

out:
	if (prop_object_type(filesd) == PROP_TYPE_DICTIONARY)
		prop_object_release(filesd);
	if (prop_object_type(propsd) == PROP_TYPE_DICTIONARY)
		prop_object_release(propsd);
	if (prop_object_type(pkgd) == PROP_TYPE_DICTIONARY)
		prop_object_release(pkgd);
	if (broken)
		return 1;

	return 0;
}
