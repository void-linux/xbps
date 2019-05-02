/*-
 * Copyright (c) 2008-2015 Juan Romero Pardines.
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

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

static bool
question(bool preset, const char *fmt, va_list ap)
{
	int response;
	bool rv = false;

	vfprintf(stderr, fmt, ap);
	if (preset)
		fputs(" [Y/n] ", stderr);
	else
		fputs(" [y/N] ", stderr);

	response = fgetc(stdin);
	if (response == '\n')
		rv = preset;
	else if (response == 'y' || response == 'Y')
		rv = true;
	else if (response == 'n' || response == 'N')
		rv = false;

	/* read the rest of the line */
	while (response != EOF && response != '\n')
		response = fgetc(stdin);

	return rv;
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
