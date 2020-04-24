/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef _XBPS_QUERY_DEFS_H_
#define _XBPS_QUERY_DEFS_H_

#include <xbps.h>

#include "../xbps-install/defs.h"

#ifndef __UNCONST
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))
#endif

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
int	search(struct xbps_handle *, bool, const char *, const char *, bool);


#endif /* !_XBPS_QUERY_DEFS_H_ */
