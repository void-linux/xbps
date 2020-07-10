/*-
 * Copyright (c) 2012-2013 Juan Romero Pardines.
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

#ifndef _XBPS_PKGDB_DEFS_H_
#define _XBPS_PKGDB_DEFS_H_

#include <sys/time.h>
#include <xbps.h>

enum {
	CHECK_FILES = 1 << 0,
	CHECK_DEPENDENCIES = 1 << 1,
	CHECK_ALTERNATIVES = 1 << 2,
	CHECK_PKGDB = 1 << 3,
};

/* from check.c */
int check_pkg(struct xbps_handle *, xbps_dictionary_t, const char *, unsigned);
int check_all(struct xbps_handle *, unsigned);

int check_pkg_unneeded(
    struct xbps_handle *xhp, const char *pkgname, xbps_dictionary_t pkgd);
int check_pkg_files(
    struct xbps_handle *xhp, const char *pkgname, xbps_dictionary_t filesd);
int check_pkg_symlinks(
    struct xbps_handle *xhp, const char *pkgname, xbps_dictionary_t filesd);
int check_pkg_rundeps(
    struct xbps_handle *xhp, const char *pkgname, xbps_dictionary_t pkgd);
int check_pkg_alternatives(
    struct xbps_handle *xhp, const char *pkgname, xbps_dictionary_t pkgd);

int get_checks_to_run(unsigned *, char *);

/* from convert.c */
void	convert_pkgdb_format(struct xbps_handle *);

#endif /* !_XBPS_PKGDB_DEFS_H_ */
