/*-
 * Copyright (c) 2008-2016 Juan Romero Pardines.
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

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>

#include "xbps_api_impl.h"

static xbps_dictionary_t
get_pkg_in_array(xbps_array_t array, const char *str, xbps_trans_type_t tt, bool virtual)
{
	xbps_object_t obj = NULL;
	xbps_trans_type_t ttype;
	bool found = false;

	assert(array);
	assert(str);

	for (unsigned int i = 0; i < xbps_array_count(array); i++) {
		const char *pkgver = NULL;
		char pkgname[XBPS_NAME_SIZE] = {0};

		obj = xbps_array_get(array, i);
		if (!xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver))
			abort();
		if (virtual) {
			/*
			 * Check if package pattern matches
			 * any virtual package version in dictionary.
			 */
			found = xbps_match_virtual_pkg_in_dict(obj, str);
			if (found)
				break;
		} else if (xbps_pkgpattern_version(str)) {
			/* match by pattern against pkgver */
			if (xbps_pkgpattern_match(pkgver, str)) {
				found = true;
				break;
			}
		} else if (xbps_pkg_version(str)) {
			/* match by exact pkgver */
			if (strcmp(str, pkgver) == 0) {
				found = true;
				break;
			}
		} else {
			if (!xbps_pkg_name(pkgname, sizeof(pkgname), pkgver)) {
				abort();
			}
			/* match by pkgname */
			if (strcmp(pkgname, str) == 0) {
				found = true;
				break;
			}
		}
	}

	ttype = xbps_transaction_pkg_type(obj);
	if (found && tt && (ttype != tt)) {
		found = false;
	}
	if (!found) {
		errno = ENOENT;
		return NULL;
	}
	return obj;
}

xbps_dictionary_t HIDDEN
xbps_find_pkg_in_array(xbps_array_t a, const char *s, xbps_trans_type_t tt)
{
	assert(xbps_object_type(a) == XBPS_TYPE_ARRAY);
	assert(s);

	return get_pkg_in_array(a, s, tt, false);
}

xbps_dictionary_t HIDDEN
xbps_find_virtualpkg_in_array(struct xbps_handle *xhp,
			      xbps_array_t a,
			      const char *s,
			      xbps_trans_type_t tt)
{
	xbps_dictionary_t pkgd;
	const char *vpkg;

	assert(xhp);
	assert(xbps_object_type(a) == XBPS_TYPE_ARRAY);
	assert(s);

	if ((vpkg = vpkg_user_conf(xhp, s))) {
		if ((pkgd = get_pkg_in_array(a, vpkg, tt, true)))
			return pkgd;
	}

	return get_pkg_in_array(a, s, tt, true);
}

static xbps_dictionary_t
match_pkg_by_pkgver(xbps_dictionary_t repod, const char *p)
{
	xbps_dictionary_t d = NULL;
	const char *pkgver = NULL;
	char pkgname[XBPS_NAME_SIZE] = {0};

	assert(repod);
	assert(p);

	/* exact match by pkgver */
	if (!xbps_pkg_name(pkgname, sizeof(pkgname), p)) {
		xbps_error_printf("invalid pkgver: %s\n", p);
		errno = EINVAL;
		return NULL;
	}

	d = xbps_dictionary_get(repod, pkgname);
	if (!d) {
		errno = ENOENT;
		return NULL;
	}
	if (!xbps_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver)) {
		xbps_error_printf("missing `pkgver` property\n");
		errno = EINVAL;
		return NULL;
	}
	if (strcmp(pkgver, p) != 0) {
		errno = ENOENT;
		return NULL;
	}

	return d;
}

static xbps_dictionary_t
match_pkg_by_pattern(xbps_dictionary_t repod, const char *p)
{
	xbps_dictionary_t d = NULL;
	const char *pkgver = NULL;
	char pkgname[XBPS_NAME_SIZE] = {0};

	assert(repod);
	assert(p);

	/* match by pkgpattern in pkgver */
	if (!xbps_pkgpattern_name(pkgname, sizeof(pkgname), p)) {
		if (xbps_pkg_name(pkgname, sizeof(pkgname), p)) {
			return match_pkg_by_pkgver(repod, p);
		}
		xbps_error_printf("invalid pkgpattern: %s\n", p);
		errno = EINVAL;
		return NULL;
	}

	d = xbps_dictionary_get(repod, pkgname);
	if (!d) {
		errno = ENOENT;
		return NULL;
	}
	if (!xbps_dictionary_get_cstring_nocopy(d, "pkgver", &pkgver)) {
		xbps_error_printf("missing `pkgver` property`\n");
		errno = EINVAL;
		return NULL;
	}
	if (!xbps_pkgpattern_match(pkgver, p)) {
		errno = ENOENT;
		return NULL;
	}

	return d;
}

const char HIDDEN *
vpkg_user_conf(struct xbps_handle *xhp, const char *vpkg)
{
	char namebuf[XBPS_NAME_SIZE];
	xbps_dictionary_t providers;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	const char *pkg = NULL;
	const char *pkgname;
	bool found = false;
	enum { PKGPATTERN, PKGVER, PKGNAME } match;

	assert(vpkg);


	if (xbps_pkgpattern_name(namebuf, sizeof(namebuf), vpkg)) {
		match = PKGPATTERN;
		pkgname = namebuf;
	} else if (xbps_pkg_name(namebuf, sizeof(namebuf), vpkg)) {
		match = PKGVER;
		pkgname = namebuf;
	} else {
		match = PKGNAME;
		pkgname = vpkg;
	}

	providers = xbps_dictionary_get(xhp->vpkgd, pkgname);
	if (!providers)
		return NULL;

	iter = xbps_dictionary_iterator(providers);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_string_t rpkg;
		char buf[XBPS_NAME_SIZE] = {0};
		const char *vpkg_conf = NULL, *vpkgname = NULL;

		vpkg_conf = xbps_dictionary_keysym_cstring_nocopy(obj);
		rpkg = xbps_dictionary_get_keysym(providers, obj);
		pkg = xbps_string_cstring_nocopy(rpkg);

		if (xbps_pkg_version(vpkg_conf)) {
			if (!xbps_pkg_name(buf, sizeof(buf), vpkg_conf)) {
				abort();
			}
			vpkgname = buf;
		} else {
			vpkgname = vpkg_conf;
		}

		switch (match) {
		case PKGPATTERN:
			if (xbps_pkg_version(vpkg_conf)) {
				if (!xbps_pkgpattern_match(vpkg_conf, vpkg)) {
					continue;
				}
			} else {
				xbps_warn_printf("invalid: %s\n", vpkg_conf);
			}
		break;
		case PKGVER:
			if (strcmp(buf, vpkgname) != 0) {
				continue;
			}
			break;
		case PKGNAME:
			if (strcmp(vpkg, vpkgname) != 0) {
				continue;
			}
		break;
		}
		xbps_dbg_printf("%s: vpkg_conf %s pkg %s vpkgname %s\n", __func__, vpkg_conf, pkg, vpkgname);
		found = true;
		break;
	}
	xbps_object_iterator_release(iter);

	return found ? pkg : NULL;
}

xbps_dictionary_t HIDDEN
xbps_find_virtualpkg_in_conf(struct xbps_handle *xhp,
			xbps_dictionary_t d,
			const char *pkg)
{
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	xbps_dictionary_t providers;
	xbps_dictionary_t pkgd = NULL;
	const char *cur;

	if (!xhp->vpkgd_conf)
		return NULL;

	providers = xbps_dictionary_get(xhp->vpkgd_conf, pkg);
	if (!providers)
		return NULL;

	iter = xbps_dictionary_iterator(providers);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_string_t rpkg;
		char buf[XBPS_NAME_SIZE] = {0};
		const char *vpkg_conf = NULL, *vpkgname = NULL;

		vpkg_conf = xbps_dictionary_keysym_cstring_nocopy(obj);
		rpkg = xbps_dictionary_get_keysym(providers, obj);
		cur = xbps_string_cstring_nocopy(rpkg);
		assert(cur);
		if (xbps_pkg_version(vpkg_conf)) {
			if (!xbps_pkg_name(buf, sizeof(buf), vpkg_conf)) {
				abort();
			}
			vpkgname = buf;
		} else {
			vpkgname = vpkg_conf;
		}

		if (xbps_pkgpattern_version(pkg)) {
			if (xbps_pkg_version(vpkg_conf)) {
				if (!xbps_pkgpattern_match(vpkg_conf, pkg)) {
					continue;
				}
			} else {
				char vpkgver[XBPS_NAME_SIZE + sizeof("-999999_1")];
				snprintf(buf, sizeof(buf), "%s-999999_1", vpkg_conf);
				if (!xbps_pkgpattern_match(vpkgver, pkg)) {
					continue;
				}
			}
		} else if (xbps_pkg_version(pkg)) {
			// XXX: this is the old behaviour of only matching pkgname's,
			// this is kinda wrong when compared to matching patterns
			// where all variants are tried.
			if (!xbps_pkg_name(buf, sizeof(buf), pkg)) {
				abort();
			}
			if (strcmp(buf, vpkgname)) {
				continue;
			}
		} else {
			if (strcmp(pkg, vpkgname)) {
				continue;
			}
		}
		xbps_dbg_printf("%s: found: %s %s %s\n", __func__, vpkg_conf, cur, vpkgname);

		/* Try matching vpkg from configuration files */
		if (xbps_pkgpattern_version(cur))
			pkgd = match_pkg_by_pattern(d, cur);
		else if (xbps_pkg_version(cur))
			pkgd = match_pkg_by_pkgver(d, cur);
		else
			pkgd = xbps_dictionary_get(d, cur);
		break;
	}
	xbps_object_iterator_release(iter);

	return pkgd;
}

xbps_dictionary_t HIDDEN
xbps_find_virtualpkg_in_dict(struct xbps_handle *xhp,
			     xbps_dictionary_t d,
			     const char *pkg)
{
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_dictionary_t pkgd = NULL;
	const char *vpkg;

	// XXX: this is bad, dict != pkgdb,
	/* Try matching vpkg via xhp->vpkgd */
	vpkg = vpkg_user_conf(xhp, pkg);
	if (vpkg != NULL) {
		if (xbps_pkgpattern_version(vpkg))
			pkgd = match_pkg_by_pattern(d, vpkg);
		else if (xbps_pkg_version(vpkg))
			pkgd = match_pkg_by_pkgver(d, vpkg);
		else
			pkgd = xbps_dictionary_get(d, vpkg);

		if (pkgd)
			return pkgd;
	}
	/* ... otherwise match the first one in dictionary */
	iter = xbps_dictionary_iterator(d);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		pkgd = xbps_dictionary_get_keysym(d, obj);
		if (xbps_match_virtual_pkg_in_dict(pkgd, pkg)) {
			xbps_object_iterator_release(iter);
			return pkgd;
		}
	}
	xbps_object_iterator_release(iter);

	return NULL;
}

xbps_dictionary_t HIDDEN
xbps_find_pkg_in_dict(xbps_dictionary_t d, const char *pkg)
{
	xbps_dictionary_t pkgd = NULL;

	if (xbps_pkgpattern_version(pkg))
		pkgd = match_pkg_by_pattern(d, pkg);
	else if (xbps_pkg_version(pkg))
		pkgd = match_pkg_by_pkgver(d, pkg);
	else
		pkgd = xbps_dictionary_get(d, pkg);

	return pkgd;
}
