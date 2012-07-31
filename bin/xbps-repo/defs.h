/*-
 * Copyright (c) 2009-2012 Juan Romero Pardines.
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

#ifndef _XBPS_REPO_DEFS_H_
#define _XBPS_REPO_DEFS_H_

#include <xbps_api.h>

struct repo_search_data {
	int npatterns;
	char **patterns;
	size_t pkgver_len;
	size_t maxcols;
};

/* From common.c */
int	repo_remove_pkg(const char *, const char *, const char *);

/* From index.c */
int	repo_index_add(struct xbps_handle *, int, char **);
int	repo_index_clean(struct xbps_handle *, const char *);

/* From index-files.c */
int	repo_index_files_add(struct xbps_handle *, int, char **);
int	repo_index_files_clean(struct xbps_handle *, const char *);

/* From index-lock.c */
int	acquire_repo_lock(const char *, char **);
void	release_repo_lock(char **, int);

/* From find-files.c */
int	repo_find_files_in_packages(struct xbps_handle *, int, char **);

/* From list.c */
int	repo_pkg_list_cb(struct xbps_handle *,
			 struct xbps_rpool_index *,
			 void *,
			 bool *);
int	repo_list_uri_cb(struct xbps_handle *,
			 struct xbps_rpool_index *,
			 void *,
			 bool *);
int	repo_search_pkgs_cb(struct xbps_handle *,
			    struct xbps_rpool_index *,
			    void *,
			    bool *);

/* From remove-obsoletes.c */
int	repo_remove_obsoletes(struct xbps_handle *, const char *);

/* From show.c */
int	show_pkg_info_from_repolist(struct xbps_handle *,
				    const char *,
				    const char *);
int	show_pkg_deps_from_repolist(struct xbps_handle *, const char *);
int 	show_pkg_namedesc(struct xbps_handle *, prop_object_t, void *, bool *);

/* From clean.c */
int	cachedir_clean(struct xbps_handle *);

#endif /* !_XBPS_REPO_DEFS_H_ */
