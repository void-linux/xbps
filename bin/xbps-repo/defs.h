/*-
 * Copyright (c) 2009-2010 Juan Romero Pardines.
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

#ifndef __UNCONST
#define __UNCONST(a)    ((void *)(unsigned long)(const void *)(a))
#endif

/* From index.c */
int	xbps_repo_genindex(const char *);
/* From repository.c */
int	register_repository(const char *);
int	unregister_repository(const char *);
int	show_pkg_info_from_repolist(const char *);
int	show_pkg_deps_from_repolist(const char *);
int	repository_sync(void);
/* From util.c */
int 	show_pkg_files(prop_dictionary_t);
void	show_pkg_info(prop_dictionary_t);
void	show_pkg_info_only_repo(prop_dictionary_t);
int	show_pkg_namedesc(prop_object_t, void *, bool *);
int	list_strings_in_array(prop_object_t, void *, bool *);
int	list_strings_sep_in_array(prop_object_t, void *, bool *);
size_t	find_longest_pkgver(prop_dictionary_t);
void	print_package_line(const char *);
/* From find-files.c */
int	repo_find_files_in_packages(const char *);

struct repo_search_data {
	char *pattern;
	size_t pkgver_len;
};

#endif /* !_XBPS_REPO_DEFS_H_ */
