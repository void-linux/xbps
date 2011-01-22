/*
 * FreeBSD install - a package for the installation and maintenance
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
 * Maxim Sobolev
 * 31 July 2001
 *
 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <limits.h>
#include <fnmatch.h>

#include <xbps_api.h>
#include "config.h"

/**
 * @file lib/pkgmatch.c
 * @brief Package version matching routines
 * @defgroup vermatch Package version matching functions
 */

static int
csh_match(const char *pattern, const char *string, int flags)
{
	const char *nextchoice = pattern, *current = NULL;
	int ret = FNM_NOMATCH, prefixlen = -1, curlen = 0, level = 0;

	do {
		const char *pos = nextchoice;
		const char *postfix = NULL;
		bool quoted = false;

		nextchoice = NULL;
		do {
			const char *eb;
			if (!*pos) {
				postfix = pos;
			} else if (quoted) {
				quoted = false;
			} else {
				switch (*pos) {
				case '{':
					++level;
					if (level == 1) {
						current = pos + 1;
						prefixlen = pos - pattern;
					}
					break;
				case ',':
					if (level == 1 && !nextchoice) {
						nextchoice = pos + 1;
						curlen = pos - current;
					}
					break;
				case '}':
					if (level == 1) {
						postfix = pos + 1;
						if (!nextchoice)
							curlen = pos - current;
					}
					level--;
					break;
				case '[':
					eb = pos+1;
					if (*eb == '!' || *eb == '^')
						eb++;
					if (*eb == ']')
						eb++;
					while (*eb && *eb != ']')
						eb++;
					if (*eb)
						pos = eb;
					break;
				case '\\':
					quoted = true;
					break;
				default:
					;
				}
			}
			pos++;
		} while (!postfix);

		if (current) {
			char buf[FILENAME_MAX];
			snprintf(buf, sizeof(buf), "%.*s%.*s%s", prefixlen,
			    pattern, curlen, current, postfix);
			ret = csh_match(buf, string, flags);
			if (ret) {
				current = nextchoice;
				level = 1;
			} else
				current = NULL;
		} else
			ret = fnmatch(pattern, string, flags);
	} while (current);

	return ret;
}

int
xbps_pkgpattern_match(const char *instpkg, char *pattern)
{
	const char *fname = instpkg;
	char *basefname = NULL, condchar = '\0', *condition;
	size_t len = 0;
	int rv = 0;

	/* Check for a full match with strcmp, otherwise try csh_match() */
	if (strcmp(instpkg, pattern) == 0)
		return 1;

	condition = strpbrk(pattern, "><=");
	if (condition) {
		const char *ch;
		if (condition > pattern && condition[-1] == '!')
			condition--;
		condchar = *condition;
		*condition = '\0';
		ch = strrchr(fname, '-');
		if (ch && ch - fname < PATH_MAX) {
			len = ch - fname + 1;
			basefname = malloc(len);
			if (basefname == NULL)
				return -1;
			strlcpy(basefname, fname, len);
			fname = basefname;
		}
	}

	rv = (csh_match(pattern, fname, 0) == 0) ? 1 : 0;

	while (condition) {
		*condition = condchar;
		if (rv == 1) {
			char *nextcondition;
			int match = 0;
			if (*++condition == '=') {
				match = 2;
				condition++;
			}
			switch (condchar) {
			case '<':
				match |= 1;
				break;
			case '>':
				match |= 4;
				break;
			case '=':
				match |= 2;
				break;
			case '!':
				match = 5;
				break;
			}
			nextcondition = strpbrk(condition, "<>=!");
			if (nextcondition) {
				condchar = *nextcondition;
				*nextcondition = '\0';
			}
			if ((match &
			    (1 << (xbps_cmpver(instpkg, condition) + 1))) == 0)
				rv = 0;
			condition = nextcondition;
		} else {
			break;
		}
	}
	if (basefname)
		free(basefname);

	return rv;
}
