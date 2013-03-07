/*-
 * Copyright (c) 2009-2013 Juan Romero Pardines.
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

#ifndef _XBPS_INSTALL_DEFS_H_
#define _XBPS_INSTALL_DEFS_H_

#include <sys/time.h>
#include <xbps_api.h>

struct xferstat {
	struct timeval start;
	struct timeval last;
};

/* from transaction.c */
int	install_new_pkg(struct xbps_handle *, const char *, bool);
int	update_pkg(struct xbps_handle *, const char *);
int	dist_upgrade(struct xbps_handle *, size_t, bool, bool);
int	exec_transaction(struct xbps_handle *, size_t, bool, bool);

/* from question.c */
bool	yesno(const char *, ...);
bool	noyes(const char *, ...);

/* from fetch_cb.c */
void	fetch_file_progress_cb(struct xbps_fetch_cb_data *, void *);

/* from state_cb.c */
void	state_cb(struct xbps_state_cb_data *, void *);

/* From util.c */
void	print_package_line(const char *, size_t, bool);
size_t	get_maxcols(void);

#endif /* !_XBPS_INSTALL_DEFS_H_ */
