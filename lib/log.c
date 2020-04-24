/*-
 * Licensed under the SPDX BSD-2-Clause identifier.
 * Use is subject to license terms, as specified in the LICENSE file.
 */

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>

#include "xbps_api_impl.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

/**
 * @file lib/log.c
 * @brief Logging functions
 * @defgroup log Logging functions
 *
 * Use these functions to log errors, warnings and debug messages.
 */

static void
common_printf(FILE *f, const char *msg, const char *fmt, va_list ap)
{
	if (msg != NULL)
		fprintf(f, "%s", msg);

	vfprintf(f, fmt, ap);
}

void
xbps_dbg_printf_append(struct xbps_handle *xhp, const char *fmt, ...)
{
	va_list ap;

	if (!xhp)
		return;

	if ((xhp->flags & XBPS_FLAG_DEBUG) == 0)
		return;

	va_start(ap, fmt);
	common_printf(stderr, NULL, fmt, ap);
	va_end(ap);
}

void
xbps_dbg_printf(struct xbps_handle *xhp, const char *fmt, ...)
{
	va_list ap;

	if (!xhp)
		return;

	if ((xhp->flags & XBPS_FLAG_DEBUG) == 0)
		return;

	va_start(ap, fmt);
	common_printf(stderr, "[DEBUG] ", fmt, ap);
	va_end(ap);
}

void
xbps_error_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	common_printf(stderr, "ERROR: ", fmt, ap);
	va_end(ap);
}

void
xbps_warn_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	common_printf(stderr, "WARNING: ", fmt, ap);
	va_end(ap);
}
