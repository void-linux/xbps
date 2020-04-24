/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
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
