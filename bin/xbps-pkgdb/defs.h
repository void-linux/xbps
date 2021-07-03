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

enum checks_to_run {
	CHECK_FILES = 1,
	CHECK_DEPENDENCIES = 1 << 1,
	CHECK_ALTERNATIVES = 1 << 2,
	CHECK_PKGDB = 1 << 3,
};

/* from check.c */
int	check_pkg_integrity(struct xbps_handle *, xbps_dictionary_t, const char *, unsigned);
int	check_pkg_integrity_all(struct xbps_handle *, unsigned);

#define CHECK_PKG_DECL(type)			\
int check_pkg_##type (struct xbps_handle *, const char *, void *)

CHECK_PKG_DECL(unneeded);
CHECK_PKG_DECL(files);
CHECK_PKG_DECL(rundeps);
CHECK_PKG_DECL(symlinks);
CHECK_PKG_DECL(alternatives);

int get_checks_to_run(unsigned *, char *);

/* from convert.c */
void	convert_pkgdb_format(struct xbps_handle *);

#endif /* !_XBPS_PKGDB_DEFS_H_ */
