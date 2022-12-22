/*-
 * Copyright (c) 2015-2019 Juan Romero Pardines.
 * Copyright (c) 2020 Duncan Overbruck <mail@duncano.de>.
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
/*-
 * xbps_path_clean is based on the go filepath.Clean function:
 * - https://github.com/golang/go/blob/cfe2ab42/src/path/filepath/path.go#L88
 *
 * Copyright (c) 2009 The Go Authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "xbps_api_impl.h"

ssize_t
xbps_path_clean(char *dst)
{
	char buf[PATH_MAX];
	const char *p = buf;
	const char *dotdot = dst;
	char *d = dst;
	bool rooted = *dst == '/';

	if (xbps_strlcpy(buf, dst, sizeof buf) >= sizeof buf)
		return -1;

	if (rooted) {
		*d++ = '/';
		p++;
		dotdot++;
	}

	for (; *p;) {
		switch (*p) {
		/* empty path element */
		case '/': p++; break;
		case '.':
			if (p[1] == '\0' || p[1] == '/') {
				/* . element */
				p++;
				continue;
			} else if (p[1] == '.' && (p[2] == '\0' || p[2] == '/')) {
				p += 2;
				/* .. element */
				if (d > dotdot) {
					/* can backtrack */
					d--;
					for (; d > dotdot && *d != '/'; d--)
						;
				} else if (!rooted) {
					/* cannot backtrack, but not rooted,
					 * append .. element. */
					if (d > dst)
						*d++ = '/';
					*d++ = '.';
					*d++ = '.';
					dotdot = d;
				}
				continue;
			}
			/* normal path element starting with . */
			/* FALLTHROUGH */
		default:
			if (d > dst+(rooted ? 1 : 0))
				*d++ = '/';
			for (; *p && *p != '/'; p++)
				*d++ = *p;
		}
	}

	/* Turn empty string into "." */
	if (d == dst)
		*d++ = '.';

	*d = '\0';
	return (d-dst);
}

ssize_t
xbps_path_rel(char *dst, size_t dstlen, const char *from, const char *to)
{
	char frombuf[PATH_MAX], tobuf[PATH_MAX];
	const char *fromp = frombuf, *top = tobuf, *suffix = tobuf;
	size_t len = 0;
	int up = -1;

	*dst = '\0';

	if (xbps_strlcpy(frombuf, from, sizeof frombuf) >= sizeof frombuf ||
	    xbps_strlcpy(tobuf, to, sizeof tobuf) >= sizeof tobuf)
		return -1;

	if (xbps_path_clean(frombuf) == -1 || xbps_path_clean(tobuf) == -1)
		return -1;

	for (; *fromp == *top && *to; fromp++, top++)
		if (*top == '/')
			suffix = top;

	for (up = -1, fromp--; fromp && *fromp; fromp = strchr(fromp+1, '/'), up++)
		;

	while (up--) {
		for (const char *x = "../"; *x; x++) {
			if (len+1 < dstlen)
				dst[len] = *x;
			len++;
		}
	}
	if (*suffix != '\0') {
		for (suffix += 1; *suffix; suffix++) {
			if (len+1 < dstlen)
				dst[len] = *suffix;
			len++;
		}
	}

	dst[len < dstlen ? len : dstlen - 1] = '\0';
	return len;
}

static ssize_t
xbps_path_vjoin(char *dst, size_t dstlen, va_list ap)
{
	size_t len = 0;
	const char *val;
	*dst = '\0';

	if ((val = va_arg(ap, const char *)) == NULL)
		return 0;

	for (;;) {
		size_t n;
		if ((n = xbps_strlcat(dst+len, val, dstlen-len)) >= dstlen-len)
		    goto err;
		len += n;
		if ((val = va_arg(ap, const char *)) == NULL)
			break;
		if (len > 0 && dst[len-1] != '/') {
			if (len+1 > dstlen)
				goto err;
			dst[len] = '/';
			dst[len+1] = '\0';
			len++;
		}
		if (len > 0 && *val == '/')
			val++;
	}

	return (ssize_t)len < 0 ? -1 : (ssize_t)len;
err:
	errno = ENOBUFS;
	return -1;
}

ssize_t
xbps_path_join(char *dst, size_t dstlen, ...)
{
	ssize_t len;
	va_list ap;
	va_start(ap, dstlen);
	len = xbps_path_vjoin(dst, dstlen, ap);
	va_end(ap);
	return len;
}

ssize_t
xbps_path_append(char *dst, size_t dstlen, const char *suffix)
{
	size_t len = strlen(dst);

	if (*suffix == '\0')
		goto out;

	if (*dst == '\0') {
		if ((len = xbps_strlcpy(dst, suffix, dstlen)) >= dstlen)
			goto err;
		goto out;
	}

	if (dst[len-1] != '/' && len+1 < dstlen) {
		dst[len] = '/';
		dst[len+1] = '\0';
	}
	if (*suffix == '/')
		suffix++;

	if ((len = xbps_strlcat(dst, suffix, dstlen)) >= dstlen)
		goto err;
out:
	return (ssize_t)len < 0 ? -1 : (ssize_t)len;
err:
	errno = ENOBUFS;
	return -1;
}

ssize_t
xbps_path_prepend(char *dst, size_t dstlen, const char *prefix)
{
	size_t len, prelen;
	char *p = dst;

	len = strlen(dst);

	if (*prefix == '\0')
		goto out;

	if (*dst == '\0') {
		if ((len = xbps_strlcpy(dst, prefix, dstlen)) >= dstlen)
			goto err;
		goto out;
	}

	prelen = strlen(prefix);
	if (prefix[prelen-1] == '/')
		prelen--;

	if (*dst == '/') {
		len--;
		p++;
	}

	/* prefix + '/' + dst + '\0' */
	if (len+prelen+2 > dstlen)
		goto err;

	memmove(dst+prelen+1, p, len);

	len += prelen+1;

	dst[prelen] = '/';

	memcpy(dst, prefix, prelen);

	dst[len] = '\0';
out:
	return (ssize_t)len < 0 ? -1 : (ssize_t)len;
err:
	errno = ENOBUFS;
	return -1;
}
