/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#ifndef _XBPS_PKGDB_DEFS_H_
#define _XBPS_PKGDB_DEFS_H_

#include <sys/time.h>
#include <xbps.h>

/* from check.c */
int	check_pkg_integrity(struct xbps_handle *, xbps_dictionary_t, const char *);
int	check_pkg_integrity_all(struct xbps_handle *);

#define CHECK_PKG_DECL(type)			\
int check_pkg_##type (struct xbps_handle *, const char *, void *)

CHECK_PKG_DECL(unneeded);
CHECK_PKG_DECL(files);
CHECK_PKG_DECL(rundeps);
CHECK_PKG_DECL(symlinks);
CHECK_PKG_DECL(alternatives);

/* from convert.c */
void	convert_pkgdb_format(struct xbps_handle *);

#endif /* !_XBPS_PKGDB_DEFS_H_ */
