/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef _XBPS_INSTALL_DEFS_H_
#define _XBPS_INSTALL_DEFS_H_

#include <sys/time.h>
#include <xbps.h>

struct xferstat {
	struct timeval start;
	struct timeval last;
};

struct transaction {
	struct xbps_handle *xhp;
	xbps_dictionary_t d;
	xbps_object_iterator_t iter;
	uint32_t inst_pkgcnt;
	uint32_t up_pkgcnt;
	uint32_t cf_pkgcnt;
	uint32_t rm_pkgcnt;
	uint32_t dl_pkgcnt;
	uint32_t hold_pkgcnt;
};

/* from transaction.c */
int	install_new_pkg(struct xbps_handle *, const char *, bool);
int	update_pkg(struct xbps_handle *, const char *, bool);
int	dist_upgrade(struct xbps_handle *, unsigned int, bool, bool);
int	exec_transaction(struct xbps_handle *, unsigned int, bool, bool);

/* from question.c */
bool	yesno(const char *, ...);
bool	noyes(const char *, ...);

/* from fetch_cb.c */
void	fetch_file_progress_cb(const struct xbps_fetch_cb_data *, void *);

/* from state_cb.c */
int	state_cb(const struct xbps_state_cb_data *, void *);

/* From util.c */
void	print_package_line(const char *, unsigned int, bool);
bool	print_trans_colmode(struct transaction *, unsigned int);
int	get_maxcols(void);
const char	*ttype2str(xbps_dictionary_t);

#endif /* !_XBPS_INSTALL_DEFS_H_ */
