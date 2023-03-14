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

enum tok {
	TTEXT = 1,
	TVAR,
};

static enum tok
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
parse_d(const char **pos, int64_t *d)
{
	char *e = NULL;
	long v;
	errno = 0;
	v = strtol(*pos, &e, 10);
	if (errno != 0)
		return -errno;
	if (v > UINT_MAX)
		return -ERANGE;
	*d = v;
	*pos = e;
	return 0;
}

static int
parse_default(const char **pos, struct xbps_fmt *fmt, struct strbuf *buf,
		struct xbps_fmt_def *def_storage)
{
	struct strbuf buf2 = {0};
	struct xbps_fmt_def *def;
	const char *p = *pos;
	char *str = NULL;
	int r;

	if (*p++ != '?')
		return 0;
	if (!def_storage) {
		fmt->def = def = calloc(1, sizeof(*def));
		if (!def)
			return -errno;
	} else {
		fmt->def = def = def_storage;
	}

	if ((*p >= '0' && *p <= '9') || *p == '-') {
		r = parse_d(&p, &def->val.num);
		if (r < 0)
			return r;
		def->type = XBPS_FMT_DEF_NUM;
		*pos = p;
		return 0;
	} else if (strncmp(p, "true", sizeof("true") - 1) == 0) {
		*pos = p + sizeof("true") - 1;
		def->type = XBPS_FMT_DEF_BOOL;
		def->val.boolean = true;
		return 0;
	} else if (strncmp(p, "false", sizeof("false") - 1) == 0) {
		*pos = p + sizeof("false") - 1;
		def->type = XBPS_FMT_DEF_BOOL;
		def->val.boolean = false;
		return 0;
	}

	if (*p++ != '"')
		return -EINVAL;

	if (!buf) {
		buf = &buf2;
	} else {
		r = strbuf_putc(buf, '\0');
		if (r < 0)
			return r;
		str = buf->mem + buf->len;
	}
	for (; *p && *p != '"'; p++) {
		switch (*p) {
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
			case '"':  r = strbuf_putc(buf, '"');  break;
			default:   r = -EINVAL;
			}
			break;
		default:
			r = strbuf_putc(buf, *p);
		}
		if (r < 0)
			goto err;
	}
	if (*p++ != '"') {
		r = -EINVAL;
		goto err;
	}
	*pos = p;
	def->type = XBPS_FMT_DEF_STR;
	if (buf == &buf2) {
		def->val.str = strdup(buf2.mem);
		if (!def->val.str) {
			r = -errno;
			goto err;
		}
		strbuf_release(&buf2);
	} else {
		def->val.str = str;
	}
	return 0;
err:
	strbuf_release(&buf2);
	return r;
}

struct xbps_fmt_conv {
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
parse_conversion(const char **pos, struct xbps_fmt *fmt, struct xbps_fmt_conv *conv_storage)
{
	if (**pos != '!') {
		fmt->conv = NULL;
		return 0;
	}
	fmt->conv = conv_storage;
	if (!conv_storage)
		fmt->conv = calloc(1, sizeof(*fmt->conv));
	if (!fmt->conv)
		return -errno;
	if (strncmp(*pos + 1, "strmode", sizeof("strmode") - 1) == 0) {
		*pos += sizeof("strmode");
		fmt->conv->type = STRMODE;
		return 0;
	} else if (strncmp(*pos + 1, "humanize", sizeof("humanize") - 1) == 0) {
		fmt->conv->type = HUMANIZE;
		*pos += sizeof("humanize");
		return parse_humanize(pos, &fmt->conv->humanize);
	}
	return -EINVAL;
}

static int
parse_spec(const char **pos, struct xbps_fmt *fmt, struct xbps_fmt_spec *spec_storage)
{
	bool fill = false;
	struct xbps_fmt_spec *spec;
	const char *p = *pos;
	int r;

	/* format_spec ::= [[fill]align][sign][zero][width][.precision][type] */

	if (*p != ':') {
		fmt->spec = NULL;
		return 0;
	}
	p++;

	if (!spec_storage) {
		spec = fmt->spec = calloc(1, sizeof(*fmt->spec));
		if (!fmt->spec)
			return -errno;
	} else {
		spec = fmt->spec = spec_storage;
	}

	/* defaults */
	spec->fill = ' ';
	spec->align = '>';
	spec->sign = '-';
	spec->width = 0;
	spec->precision = 0;
	spec->type = '\0';

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
parse(const char **pos, struct xbps_fmt *fmt,
		struct strbuf *buf,
		struct xbps_fmt_def *def_storage,
		struct xbps_fmt_conv *conv_storage,
		struct xbps_fmt_spec *spec_storage)
{
	const char *p = *pos;
	const char *e;
	int r;

	if (*p != '{')
		return -EINVAL;
	p++;

	/* var ::= '{' name [default][conversion][format_spec] '}' */

	/* name ::= [a-zA-Z0-9_-]+ */
	for (e = p; (*e >= 'a' && *e <= 'z') ||
	            (*e >= 'A' && *e <= 'Z') ||
	            (*e >= '0' && *e <= '0') ||
	            (*e == '_' || *e == '-'); e++)
		;
	if (e == p)
		return -EINVAL;

	if (buf) {
		strbuf_reset(buf);
		r = strbuf_puts(buf, p, e - p);
		if (r < 0)
			return r;
	} else {
		fmt->var = strndup(p, e - p);
		if (!fmt->var)
			return -errno;
	}
	p = e;

	/* default ::= ['?' ...] */
	r = parse_default(&p, fmt, buf, def_storage);
	if (r < 0)
		return r;

	/* conversion ::= ['!' ...] */
	r = parse_conversion(&p, fmt, conv_storage);
	if (r < 0)
		return r;

	/* format_spec ::= [':' ...] */
	r = parse_spec(&p, fmt, spec_storage);
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
		struct xbps_fmt *tmp;
		enum tok t;

		t = nexttok(&pos, &buf);

		tmp = realloc(fmt, sizeof(*fmt)*(n + 1));
		if (!tmp)
			goto err_errno;
		fmt = tmp;
		memset(&fmt[n], '\0', sizeof(struct xbps_fmt));

		if (t == 0)
			goto out;
		if (t == TTEXT) {
			fmt[n].prefix = strndup(buf.mem, buf.len);
			if (!fmt[n].prefix)
				goto err_errno;
			t = nexttok(&pos, &buf);
		}
		if (t == TVAR) {
			r = parse(&pos, &fmt[n], NULL, NULL, NULL, NULL);
			if (r < 0)
				goto err;
		}
		fprintf(stderr, "fmt: prefix='%s' var='%s'\n", fmt[n].prefix, fmt[n].var);
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
	for (struct xbps_fmt *f = fmt; f->prefix || f->var; f++) {
		free(f->prefix);
		free(f->var);
		if (f->def && f->def->type == XBPS_FMT_DEF_STR)
			free(f->def->val.str);
		free(f->def);
		free(f->spec);
		free(f->conv);
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
		enum tok t;

		t = nexttok(&pos, &buf);
		if (t == 0)
			goto out;
		if (t == TTEXT) {
			fprintf(fp, "%s", buf.mem);
			t = nexttok(&pos, &buf);
		}
		if (t == TVAR) {
			struct xbps_fmt_def def = {0};
			struct xbps_fmt_conv conv = {0};
			struct xbps_fmt_spec spec = {0};
			struct xbps_fmt fmt = { .var = buf.mem };
			r = parse(&pos, &fmt, &buf, &def, &conv, &spec);
			if (r < 0)
				goto out;
			r = cb(fp, &fmt, data);
			if (r != 0)
				goto out;
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
	for (const struct xbps_fmt *f = fmt; f->prefix || f->var; f++) {
		if (f->prefix)
			fprintf(fp, "%s", f->prefix);
		if (f->var) {
			r = cb(fp, f, data);
			if (r != 0)
				return r;
		}
	}
	return 0;
}

struct fmt_dict_cb {
	xbps_dictionary_t dict;
};

int
xbps_fmt_print_string(const struct xbps_fmt *fmt, const char *str, size_t len, FILE *fp)
{
	const struct xbps_fmt_spec *spec = fmt->spec;
	if (len == 0)
		len = strlen(str);
	if (spec && spec->align == '>' && spec->width > (unsigned)len) {
		for (unsigned i = 0; i < spec->width - len; i++)
			fputc(spec->fill, fp);
	}
	fprintf(fp, "%.*s", (int)len, str);
	if (spec && spec->align == '<' && spec->width > (unsigned)len) {
		for (unsigned i = 0; i < spec->width - len; i++)
			fputc(spec->fill, fp);
	}
	return 0;
}

static int
humanize(const struct humanize *h, const struct xbps_fmt *fmt, int64_t d, FILE *fp)
{
	char buf[64];
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
	return xbps_fmt_print_string(fmt, buf, len, fp);
}

static int
tostrmode(const struct xbps_fmt *fmt UNUSED, int64_t d UNUSED, FILE *fp UNUSED)
{
	return -ENOTSUP;
}

int
xbps_fmt_print_number(const struct xbps_fmt *fmt, int64_t d, FILE *fp)
{
	char buf[64];
	struct xbps_fmt_spec strspec = {0};
	struct xbps_fmt strfmt = { .spec = &strspec };
	struct xbps_fmt_spec *spec = fmt->spec;
	const char *p = buf;
	int len;

	if (fmt->conv) {
		switch (fmt->conv->type) {
		case HUMANIZE: return humanize(&fmt->conv->humanize, fmt, d, fp);
		case STRMODE:  return tostrmode(fmt, d, fp);
		}
	}
	if (spec) {
		strspec = *spec;
		if (spec->align == '=')
			strspec.align = '>';
	}

	switch (spec ? spec->type : '\0') {
	default: /* fallthrough */
	case 'd':
		if (spec && spec->sign == '+')
			len = snprintf(buf, sizeof(buf), "%+" PRId64, d);
		else
			len = snprintf(buf, sizeof(buf), "%" PRId64, d);
		if (spec && spec->align == '=' && (buf[0] == '+' || buf[0] == '-')) {
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
	return xbps_fmt_print_string(&strfmt, p, len, fp);
}

int
xbps_fmt_print_object(const struct xbps_fmt *fmt, xbps_object_t obj, FILE *fp)
{
	switch (xbps_object_type(obj)) {
	case XBPS_TYPE_BOOL:
		return xbps_fmt_print_string(fmt, xbps_bool_true(obj) ? "true" : "false", 0, fp);
	case XBPS_TYPE_NUMBER:
		return xbps_fmt_print_number(fmt, xbps_number_integer_value(obj), fp);
	case XBPS_TYPE_STRING:
		return xbps_fmt_print_string(fmt, xbps_string_cstring_nocopy(obj),
		    xbps_string_size(obj), fp);
	case XBPS_TYPE_UNKNOWN:
		if (fmt->def) {
			struct xbps_fmt_def *def = fmt->def;
			switch (fmt->def->type) {
			case XBPS_FMT_DEF_BOOL:
				return xbps_fmt_print_string(fmt, def->val.boolean ?
				    "true" : "false", 0, fp);
			case XBPS_FMT_DEF_STR:
				return xbps_fmt_print_string(fmt, def->val.str, 0, fp);
			case XBPS_FMT_DEF_NUM:
				return xbps_fmt_print_number(fmt, def->val.num, fp);
			}
		}
	default:
		break;
	}
	return 0;
}

static int
fmt_dict_cb(FILE *fp, const struct xbps_fmt *fmt, void *data)
{
	struct fmt_dict_cb *ctx = data;
	xbps_object_t obj = xbps_dictionary_get(ctx->dict, fmt->var);
	return xbps_fmt_print_object(fmt, obj, fp);
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
