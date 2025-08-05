/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
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

#ifndef _XBPS_QUERY_DEFS_H_
#define _XBPS_QUERY_DEFS_H_

#include <xbps.h>

#include "../xbps-install/defs.h"

/* from show-deps.c */
int	show_pkg_deps(struct xbps_handle *, const char *, bool, bool);
int	show_pkg_revdeps(struct xbps_handle *, const char *, bool);

/* from show-info-files.c */
void	show_pkg_info(xbps_dictionary_t);
void	show_pkg_info_one(xbps_dictionary_t, const char *);
int	show_pkg_info_from_metadir(struct xbps_handle *, const char *,
		const char *);
int	show_pkg_files(xbps_dictionary_t);
int	show_pkg_files_from_metadir(struct xbps_handle *, const char *);
int	repo_show_pkg_files(struct xbps_handle *, const char *);
int	cat_file(struct xbps_handle *, const char *, const char *);
int	repo_cat_file(struct xbps_handle *, const char *, const char *);
int	repo_show_pkg_info(struct xbps_handle *, const char *, const char *);
int 	repo_show_pkg_namedesc(struct xbps_handle *, xbps_object_t, void *,
		bool *);

/* from ownedby.c */
int	ownedby(struct xbps_handle *, const char *, bool, bool);

/* From list.c */
unsigned int	find_longest_pkgver(struct xbps_handle *, xbps_object_t);

int	list_pkgs_in_dict(struct xbps_handle *, xbps_object_t, const char *, void *, bool *);
int	list_manual_pkgs(struct xbps_handle *, xbps_object_t, const char *, void *, bool *);
int	list_hold_pkgs(struct xbps_handle *, xbps_object_t, const char *, void *, bool *);
int	list_repolock_pkgs(struct xbps_handle *, xbps_object_t, const char *, void *, bool *);
int	list_orphans(struct xbps_handle *);
int	list_pkgs_pkgdb(struct xbps_handle *);

int	repo_list(struct xbps_handle *);

/* from search.c */
int search(
    struct xbps_handle *xhp, bool repo_mode, const char *pattern, bool regex);
int search_prop(struct xbps_handle *xhp, bool repo_mode, const char *prop,
    const char *pattern, bool regex);

#endif /* !_XBPS_QUERY_DEFS_H_ */
