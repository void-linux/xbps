/*	$NetBSD: opattern.c,v 1.5 2009/02/02 12:35:01 joerg Exp $	*/

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 18 July 1993
 *
 * Miscellaneous string utilities.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <ctype.h>
#include <sys/param.h>

#include "xbps_api_impl.h"

/*
 * Perform glob match on "pkg" against "pattern".
 * Return 1 on match, 0 otherwise.
 */
static int
glob_match(const char *pattern, const char *pkg)
{
	return fnmatch(pattern, pkg, FNM_PERIOD) == 0;
}

/*
 * Perform simple match on "pkg" against "pattern". 
 * Return 1 on match, 0 otherwise.
 */
static int
simple_match(const char *pattern, const char *pkg)
{
	return strcmp(pattern, pkg) == 0;
}

/*
 * Performs a fast check if pattern can ever match pkg.
 * Returns 1 if a match is possible and 0 otherwise.
 */
static int
quick_pkg_match(const char *pattern, const char *pkg)
{
#define simple(x) (isalnum((unsigned char)(x)) || (x) == '-')
	if (!simple(pattern[0]))
		return 1;
	if (pattern[0] != pkg[0])
		return 0;

	if (!simple(pattern[1]))
		return 1;
	if (pattern[1] != pkg[1])
		return 0;
	return 1;
#undef simple
}

/*
 * Match pkg against pattern, return 1 if matching, 0 otherwise or -1 on error.
 */
int
xbps_pkgpattern_match(const char *pkg, const char *pattern)
{
	if (!quick_pkg_match(pattern, pkg))
		return 0;

	if (strpbrk(pattern, "<>") != NULL) {
		/* perform relational dewey match on version number */
		return dewey_match(pattern, pkg);
	}
	if (strpbrk(pattern, "*?[]") != NULL) {
		/* glob match */
		if (glob_match(pattern, pkg))
			return 1;
	}

	/* no dewey or glob match -> simple compare */
	if (simple_match(pattern, pkg))
		return 1;

	/* no match */
	return 0;
}
