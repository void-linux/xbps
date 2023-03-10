/*-
 * Copyright (c) 2023 Duncan Overbruck <mail@duncano.de>.
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

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "xbps_api_impl.h"
#include "compat.h"

/**
 * @file lib/format.c
 * @brief Format printing functions
 * @defgroup format Format printing fuctions
 *
 * The format strings are similar to normal printf() format strings,
 * but instead of character to specify types variable names are used.
 *
 */

struct strbuf {
	size_t sz, len;
	char *mem;
};

static int
strbuf_grow(struct strbuf *sb, size_t n)
{
	char *tmp;
	size_t nsz;
	if (sb->len+n+1 < sb->sz)
		return 0;
	nsz = 2*sb->sz + 16;
	tmp = realloc(sb->mem, nsz);
	if (!tmp)
		return -errno;
	sb->mem = tmp;
	sb->sz = nsz;
	return 0;
}

static int
strbuf_putc(struct strbuf *sb, char c)
{
	int r = strbuf_grow(sb, 1);
	if (r < 0)
		return 0;
	sb->mem[sb->len++] = c;
	sb->mem[sb->len] = '\0';
	return 0;
}

static int
strbuf_puts(struct strbuf *sb, const char *s, size_t n)
{
	int r = strbuf_grow(sb, n);
	if (r < 0)
		return 0;
	memcpy(sb->mem+sb->len, s, n);
	sb->len += n;
	sb->mem[sb->len] = '\0';
	return 0;
}

static void
strbuf_reset(struct strbuf *sb)
{
	sb->len = 0;
	if (sb->mem)
		sb->mem[0] = '\0';
}

static void
strbuf_release(struct strbuf *sb)
{
	free(sb->mem);
	sb->mem = NULL;
	sb->len = sb->sz = 0;
}

struct common {
	int type;
};

struct chunk {
	struct common common;
	char s[];
};

struct var {
	struct common common;
	struct xbps_fmt_spec spec;
	char s[];
};

struct xbps_fmt {
	union {
		struct common *common;
		struct chunk *chunk;
		struct var *var;
	};
};

enum {
	TTEXT = 1,
	TVAR,
};

static int
nexttok(const char **pos, struct strbuf *buf)
{
	const char *p;
	int r;

	strbuf_reset(buf);

	for (p = *pos; *p;) {
		switch (*p) {
		case '}':
			return -EINVAL;
		case '{':
			*pos = p;
			if (buf->len > 0)
				return TTEXT;
			return TVAR;
		case '\\':
			switch (*++p) {
			case '\\': r = strbuf_putc(buf, '\\'); break;
			case 'a':  r = strbuf_putc(buf, '\a'); break;
			case 'b':  r = strbuf_putc(buf, '\b'); break;
			case 'f':  r = strbuf_putc(buf, '\f'); break;
			case 'n':  r = strbuf_putc(buf, '\n'); break;
			case 'r':  r = strbuf_putc(buf, '\r'); break;
			case 't':  r = strbuf_putc(buf, '\t'); break;
			case '0':  r = strbuf_putc(buf, '\0'); break;
			case '{':  r = strbuf_putc(buf, '{');  break;
			case '}':  r = strbuf_putc(buf, '}');  break;
			default:
				r = strbuf_putc(buf, '\\');
				if (r < 0)
					break;
				r = strbuf_putc(buf, *p);
			}
			if (r < 0)
				return r;
			p++;
			break;
		default:
			r = strbuf_putc(buf, *p++);
			if (r < 0)
				return r;
		}
	}
	if (buf->len > 0) {
		*pos = p;
		return TTEXT;
	}
	p++;
	return 0;
}

struct conversion {
	enum { HUMANIZE = 1, STRMODE } type;
	union {
		struct humanize {
			unsigned width    : 8;
			unsigned minscale : 8;
			unsigned maxscale : 8;
			bool decimal      : 1;
			int flags;
		} humanize;
	};
};

static int
parse_u(const char **pos, unsigned int *u)
{
	char *e = NULL;
	long v;
	errno = 0;
	v = strtoul(*pos, &e, 10);
	if (errno != 0)
		return -errno;
	if (v > UINT_MAX)
		return -ERANGE;
	*u = v;
	*pos = e;
	return 0;
}

static int
parse_humanize(const char **pos, struct humanize *humanize)
{
	const char *scale = "BKMGTPE";
	const char *p = *pos;
	const char *p1;

	/* default: !humanize .8Ki:8 */
	humanize->width = 8;
	humanize->minscale = 2;
	humanize->flags = HN_DECIMAL|HN_IEC_PREFIXES;
	humanize->flags = HN_NOSPACE;

	/* humanize[ ][.][i][width][minscale[maxscale]] */

	if (*p == ' ') {
		humanize->flags &= ~HN_NOSPACE;
		p++;
	}
	if (*p == '.') {
		humanize->flags |= HN_DECIMAL;
		p++;
	}
	if ((*p >= '0' && *p <= '9')) {
		unsigned width = 0;
		int r = parse_u(&p, &width);
		if (r < 0)
			return r;
		humanize->width = width <= 12 ? width : 12;
	}
	if ((p1 = strchr(scale, *p))) {
		humanize->minscale = p1-scale+1;
		p++;
		if ((p1 = strchr(scale, *p))) {
			humanize->maxscale = p1-scale+1;
			p++;
		}
	}
	if (*p == 'i') {
		humanize->flags |= HN_IEC_PREFIXES;
		p++;
	}
	*pos = p;
	return 0;
}

static int
parse_conversion(const char **pos, struct conversion *conversion)
{
	if (**pos != '!')
		return 0;
	if (strncmp(*pos + 1, "strmode", sizeof("strmode") - 1) == 0) {
		*pos += sizeof("strmode");
		conversion->type = STRMODE;
		return 0;
	} else if (strncmp(*pos + 1, "humanize", sizeof("humanize") - 1) == 0) {
		conversion->type = HUMANIZE;
		*pos += sizeof("humanize");
		return parse_humanize(pos, &conversion->humanize);
	}
	return -EINVAL;
}

static int
parse_spec(const char **pos, struct xbps_fmt_spec *spec)
{
	bool fill = false;
	const char *p = *pos;
	int r;

	/* defaults */
	spec->conversion = NULL;
	spec->fill = ' ';
	spec->align = '>';
	spec->sign = '-';
	spec->width = 0;
	spec->precision = 0;
	spec->type = '\0';

	/* format_spec ::= [[fill]align][sign][zero][width][.precision][type] */

	if (*p != ':')
		return 0;
	p++;

	/* fill ::= .  */
	if (*p && strchr("<>=", p[1])) {
		fill = true;
		spec->fill = *p;
		spec->align = p[1];
		p += 2;
	}

	/* align ::= [<>=] */
	if (strchr("<>=", *p)) {
		spec->align = *p;
		p += 1;
	}

	/* sign ::= [+-] */
	if (strchr("+- ", *p)) {
		spec->sign = *p;
		p += 1;
	}

	/* zero ::= [0] */
	if (*p == '0') {
		if (!fill) {
			spec->fill = '0';
			spec->align = '=';
		}
		p++;
	}

	/* width ::= [[0-9]+] */
	if ((*p >= '0' && *p <= '9')) {
		r = parse_u(&p, &spec->width);
		if (r < 0)
			return r;
	}

	/* precision ::= ['.' [0-9]+] */
	if (*p == '.') {
		p++;
		r = parse_u(&p, &spec->precision);
		if (r < 0)
			return r;
	}

	/* type ::=  [[a-zA-Z]] */
	if ((*p >= 'a' && *p <= 'z') ||
	    (*p >= 'A' && *p <= 'Z'))
		spec->type = *p++;

	*pos = p;
	return 0;
}

static int
parse(const char **pos, struct strbuf *buf, struct xbps_fmt_spec *spec, struct conversion *conversion)
{
	const char *p = *pos;
	const char *e;
	int r;

	if (*p != '{')
		return -EINVAL;
	p++;

	/* var ::= '{' name [conversion][format_spec] '}' */

	/* name ::= [a-zA-Z0-9_-]+ */
	for (e = p; (*e >= 'a' && *e <= 'z') ||
	            (*e >= 'A' && *e <= 'Z') ||
	            (*e >= '0' && *e <= '0') ||
	            (*e == '_' || *e == '-'); e++)
		;
	if (e == p)
		return -EINVAL;

	strbuf_reset(buf);
	r = strbuf_puts(buf, p, e - p);
	if (r < 0)
		return r;
	p = e;

	/* conversion ::= ['!' ...] */
	r = parse_conversion(&p, conversion);
	if (r < 0)
		return r;

	/* format_spec ::= [':' ...] */
	r = parse_spec(&p, spec);
	if (r < 0)
		return r;

	if (*p != '}')
		return -EINVAL;
	*pos = p+1;
	return 0;
}

struct xbps_fmt *
xbps_fmt_parse(const char *format)
{
	struct strbuf buf = {0};
	const char *pos = format;
	struct xbps_fmt *fmt = NULL;
	size_t n = 0;
	int r = 1;

	for (;;) {
		struct xbps_fmt_spec spec = {0};
		struct conversion conversion = {0};
		struct xbps_fmt *tmp;
		r = nexttok(&pos, &buf);
		if (r < 0)
			goto err;
		tmp = realloc(fmt, sizeof(*fmt)*(n + 1));
		if (!tmp)
			goto err_errno;
		fmt = tmp;
		switch (r) {
		case 0:
			fmt[n].common = NULL;
			goto out;
		case TTEXT:
			fmt[n].chunk = calloc(1, sizeof(struct chunk)+buf.len+1);
			fmt[n].common->type = TTEXT;
			if (!fmt[n].chunk)
				goto err_errno;
			memcpy(fmt[n].chunk->s, buf.mem, buf.len+1);
			break;
		case TVAR:
			r = parse(&pos, &buf, &spec, &conversion);
			if (r < 0)
				goto err;
			fmt[n].var = calloc(1, sizeof(struct var)+buf.len+1);
			if (!fmt[n].var)
				goto err_errno;
			fmt[n].common->type = TVAR;
			fmt[n].var->spec = spec;
			memcpy(fmt[n].var->s, buf.mem, buf.len+1);
			if (conversion.type) {
				fmt[n].var->spec.conversion = calloc(1, sizeof(struct conversion));
				if (!fmt[n].var->spec.conversion)
					goto err_errno;
				*fmt[n].var->spec.conversion = conversion;
			}
			break;
		}
		n++;
	}
out:
	strbuf_release(&buf);
	return fmt;
err_errno:
	r = -errno;
err:
	free(fmt);
	strbuf_release(&buf);
	errno = -r;
	return NULL;
}

void
xbps_fmt_free(struct xbps_fmt *fmt)
{
	if (!fmt)
		return;
	for (struct xbps_fmt *f = fmt; f->common; f++)
		switch (f->common->type) {
		case TTEXT: free(f->chunk); break;
		case TVAR:
			free(f->var->spec.conversion);
			free(f->var);
			break;
		}
	free(fmt);
}

int
xbps_fmts(const char *format, xbps_fmt_cb *cb, void *data, FILE *fp)
{
	struct strbuf buf = {0};
	const char *pos = format;
	int r = 0;

	for (;;) {
		struct xbps_fmt_spec spec = {0};
		struct conversion conversion = {0};
		r = nexttok(&pos, &buf);
		if (r <= 0)
			goto out;
		switch (r) {
		case TTEXT:
			fprintf(fp, "%s", buf.mem);
			break;
		case TVAR:
			r = parse(&pos, &buf, &spec, &conversion);
			if (r < 0)
				goto out;
			r = cb(fp, &spec, buf.mem, data);
			if (r != 0)
				goto out;
			break;
		}
	}
out:
	strbuf_release(&buf);
	return r;
}

int
xbps_fmt(const struct xbps_fmt *fmt, xbps_fmt_cb *cb, void *data, FILE *fp)
{
	int r;
	for (const struct xbps_fmt *f = fmt; f->common; f++) {
		switch (f->common->type) {
		case TTEXT:
			fprintf(fp, "%s", f->chunk->s);
			break;
		case TVAR:
			r = cb(fp, &f->var->spec, f->var->s, data);
			if (r != 0)
				return r;
			break;
		}
	}
	return 0;
}

struct fmt_dict_cb {
	xbps_dictionary_t dict;
};

int
xbps_fmt_string(const struct xbps_fmt_spec *spec, const char *str, size_t len, FILE *fp)
{
	if (len == 0)
		len = strlen(str);
	if (spec->align == '>' && spec->width > (unsigned)len) {
		for (unsigned i = 0; i < spec->width - len; i++)
			fputc(spec->fill, fp);
	}
	fprintf(fp, "%.*s", (int)len, str);
	if (spec->align == '<' && spec->width > (unsigned)len) {
		for (unsigned i = 0; i < spec->width - len; i++)
			fputc(spec->fill, fp);
	}
	return 0;
}

static int
humanize(const struct xbps_fmt_spec *spec, int64_t d, FILE *fp)
{
	char buf[64];
	const struct humanize *h = &spec->conversion->humanize;
	int scale = 0;
	int width = h->width ? h->width : 8;
	int len;

	if (h->minscale) {
		scale = humanize_number(buf, width, d, "B", HN_GETSCALE, h->flags);
		if (scale == -1)
			return -EINVAL;
		if (scale < h->minscale - 1)
			scale = h->minscale - 1;
		if (h->maxscale && scale > h->maxscale - 1)
			scale = h->maxscale - 1;
	} else if (scale == 0) {
		scale = HN_AUTOSCALE;
	}
	len = humanize_number(buf, width, d, "B", scale, h->flags);
	if (len == -1)
		return -EINVAL;
	return xbps_fmt_string(spec, buf, len, fp);
}

static int
tostrmode(const struct xbps_fmt_spec *spec UNUSED, int64_t d UNUSED, FILE *fp UNUSED)
{
	return -ENOTSUP;
}

int
xbps_fmt_number(const struct xbps_fmt_spec *spec, int64_t d, FILE *fp)
{
	char buf[64];
	struct xbps_fmt_spec strspec = *spec;
	const char *p = buf;
	int len;

	if (spec->conversion) {
		switch (spec->conversion->type) {
		case HUMANIZE: return humanize(spec, d, fp);
		case STRMODE:  return tostrmode(spec, d, fp);
		}
	}

	if (strspec.align == '=')
		strspec.align = '>';

	switch (spec->type) {
	default: /* fallthrough */
	case 'd':
		if (spec->sign == '+')
			len = snprintf(buf, sizeof(buf), "%+" PRId64, d);
		else
			len = snprintf(buf, sizeof(buf), "%" PRId64, d);
		if (spec->align == '=' && (buf[0] == '+' || buf[0] == '-')) {
			len--, p++;
			strspec.width -= 1;
			fputc(buf[0], fp);
		}
		break;
	case 'o': len = snprintf(buf, sizeof(buf), "%" PRIo64, d); break;
	case 'u': len = snprintf(buf, sizeof(buf), "%" PRIu64, d); break;
	case 'x': len = snprintf(buf, sizeof(buf), "%" PRIx64, d); break;
	case 'X': len = snprintf(buf, sizeof(buf), "%" PRIX64, d); break;
	}
	return xbps_fmt_string(&strspec, p, len, fp);
}

static int
fmt_dict_cb(FILE *fp, const struct xbps_fmt_spec *spec, const char *var, void *data)
{
	struct fmt_dict_cb *ctx = data;
	xbps_object_t val = xbps_dictionary_get(ctx->dict, var);

	switch (xbps_object_type(val)) {
	case XBPS_TYPE_STRING:
		return xbps_fmt_string(spec, xbps_string_cstring_nocopy(val),
		    xbps_string_size(val), fp);
	case XBPS_TYPE_NUMBER:
		return xbps_fmt_number(spec, xbps_number_integer_value(val), fp);
	default:
		break;
	}
	return 0;
}

int
xbps_fmt_dictionary(const struct xbps_fmt *fmt, xbps_dictionary_t dict, FILE *fp)
{
	struct fmt_dict_cb ctx = {.dict = dict};
	return xbps_fmt(fmt, &fmt_dict_cb, &ctx, fp);
}

int
xbps_fmts_dictionary(const char *format, xbps_dictionary_t dict, FILE *fp)
{
	struct fmt_dict_cb ctx = {.dict = dict};
	return xbps_fmts(format, &fmt_dict_cb, &ctx, fp);
}
