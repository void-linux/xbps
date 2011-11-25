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

#ifndef _XBPS_BIN_DEFS_H_
#define _XBPS_BIN_DEFS_H_

#ifndef __UNCONST
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))
#endif

#include <sys/time.h>
#include <xbps_api.h>

struct xferstat {
	struct timeval start;
	struct timeval last;
};

struct list_pkgver_cb {
	pkg_state_t state;
	size_t pkgver_len;
};

/* from transaction.c */
int	install_new_pkg(const char *);
int	update_pkg(const char *);
int	autoupdate_pkgs(bool, bool);
int	autoremove_pkgs(bool, bool);
int	exec_transaction(bool, bool);

/* from remove.c */
int	remove_installed_pkgs(int, char **, bool, bool, bool, bool);

/* from check.c */
int	check_pkg_integrity(const char *);
int	check_pkg_integrity_all(void);

#define CHECK_PKG_DECL(type)					\
int check_pkg_##type (prop_dictionary_t, prop_dictionary_t, prop_dictionary_t)

CHECK_PKG_DECL(autoinstall);
CHECK_PKG_DECL(files);
CHECK_PKG_DECL(rundeps);
CHECK_PKG_DECL(symlinks);
CHECK_PKG_DECL(requiredby);

/* from show-deps.c */
int	show_pkg_deps(const char *);
int	show_pkg_reverse_deps(const char *);

/* from show-info-files.c */
int	show_pkg_info_from_metadir(const char *, const char *);
int	show_pkg_files_from_metadir(const char *);

/* from show-orphans.c */
int	show_orphans(void);

/* from find-files.c */
int	find_files_in_packages(const char *);

/* from question.c */
bool	yesno(const char *, ...);
bool	noyes(const char *, ...);

/* from fetch_cb.c */
void	fetch_file_progress_cb(const struct xbps_fetch_cb_data *, void *);

/* from state_cb.c */
void	state_cb(const struct xbps_state_cb_data *, void *);

/* from unpack_cb.c */
void	unpack_progress_cb_verbose(const struct xbps_unpack_cb_data *, void *);
void	unpack_progress_cb(const struct xbps_unpack_cb_data *, void *);

/* From util.c */
int	show_pkg_files(prop_dictionary_t);
void	show_pkg_info(prop_dictionary_t);
void	show_pkg_info_one(prop_dictionary_t, const char *);
int	show_pkg_namedesc(prop_object_t, void *, bool *);
int	list_strings_in_array(prop_object_t, void *, bool *);
int	list_strings_sep_in_array(prop_object_t, void *, bool *);
size_t	find_longest_pkgver(prop_dictionary_t);
void	print_package_line(const char *, bool);

/* from list.c */
int	list_pkgs_in_dict(prop_object_t, void *, bool *);
int	list_manual_pkgs(prop_object_t, void *, bool *);

#endif /* !_XBPS_BIN_DEFS_H_ */
