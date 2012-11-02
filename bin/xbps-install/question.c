/*-
 * Copyright (c) 2008-2010 Juan Romero Pardines.
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
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#include "defs.h"

static char *
strtrim(char *str)
{
	char *pch = str;

	if (str == NULL || *str == '\0')
		return str;

	while (isspace((unsigned char)*pch))
		pch++;

	if (pch != str)
		memmove(str, pch, (strlen(pch) + 1));

	if (*str == '\0')
		return str;

	pch = (str + (strlen(str) - 1));
	while (isspace((unsigned char)*pch))
		pch--;

	*++pch = '\0';

	return str;
}

static bool
question(bool preset, const char *fmt, va_list ap)
{
	char response[32];

	vfprintf(stderr, fmt, ap);
	if (preset)
		fprintf(stderr, " %s ", "[YES/no]");
	else
		fprintf(stderr, " %s ", "[yes/NO]");

	if (fgets(response, sizeof(response), stdin)) {
		(void)strtrim(response);
		if (strlen(response) == 0)
			return preset;

		if (strcasecmp(response, "yes") == 0)
			return true;
		else if (strcasecmp(response, "no") == 0)
			return false;
	}
	return false;
}

bool
yesno(const char *fmt, ...)
{
	va_list ap;
	bool res;

	va_start(ap, fmt);
	res = question(1, fmt, ap);
	va_end(ap);

	return res;
}

bool
noyes(const char *fmt, ...)
{
	va_list ap;
	bool res;

	va_start(ap, fmt);
	res = question(0, fmt, ap);
	va_end(ap);

	return res;
}
