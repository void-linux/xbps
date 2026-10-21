/*	$NetBSD: prop_intern.c,v 1.2 2025/05/14 03:25:46 thorpej Exp $	*/

/*-
 * Copyright (c) 2006, 2007, 2025 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "prop_object_impl.h"
#include <prop/prop_object.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * _prop_intern_skip_whitespace --
 *	Skip and span of whitespace.
 */
const char *
_prop_intern_skip_whitespace(const char *cp)
{
	while (_PROP_ISSPACE(*cp)) {
		cp++;
	}
	return cp;
}

/*
 * _prop_intern_match --
 *	Returns true if the two character streams match.
 */
bool
_prop_intern_match(const char *str1, size_t len1,
		   const char *str2, size_t len2)
{
	return (len1 == len2 && memcmp(str1, str2, len1) == 0);
}

/*
 * _prop_xml_intern_skip_comment --
 *	Skip the body and end tag of an XML comment.
 */
static bool
_prop_xml_intern_skip_comment(struct _prop_object_internalize_context *ctx)
{
	const char *cp = ctx->poic_cp;

	for (cp = ctx->poic_cp; !_PROP_EOF(*cp); cp++) {
		if (cp[0] == '-' &&
		    cp[1] == '-' &&
		    cp[2] == '>') {
			ctx->poic_cp = cp + 3;
			return true;
		}
	}

	return false;		/* ran out of buffer */
}

/*
 * _prop_xml_intern_find_tag --
 *	Find the next tag in an XML stream.  Optionally compare the found
 *	tag to an expected tag name.  State of the context is undefined
 *	if this routine returns false.  Upon success, the context points
 *	to the first octet after the tag.
 */
bool
_prop_xml_intern_find_tag(struct _prop_object_internalize_context *ctx,
    const char *tag, _prop_tag_type_t type)
{
	const char *cp;
	size_t taglen;

	taglen = tag != NULL ? strlen(tag) : 0;

 start_over:
	cp = ctx->poic_cp;

	/*
	 * Find the start of the tag.
	 */
	cp = _prop_intern_skip_whitespace(cp);
	if (*cp != '<') {
		return false;
	}

	ctx->poic_tag_start = cp++;
	if (_PROP_EOF(*cp)) {
		return false;
	}

	if (*cp == '!') {
		if (cp[1] != '-' || cp[2] != '-') {
			return false;
		}
		/*
		 * Comment block -- only allowed if we are allowed to
		 * return a start tag.
		 */
		if (type == _PROP_TAG_TYPE_END) {
			return false;
		}
		ctx->poic_cp = cp + 3;
		if (_prop_xml_intern_skip_comment(ctx) == false) {
			return false;
		}
		goto start_over;
	}

	if (*cp == '/') {
		if (type != _PROP_TAG_TYPE_END &&
		    type != _PROP_TAG_TYPE_EITHER) {
			return false;
		}
		cp++;
		if (_PROP_EOF(*cp)) {
			return false;
		}
		ctx->poic_tag_type = _PROP_TAG_TYPE_END;
	} else {
		if (type != _PROP_TAG_TYPE_START &&
		    type != _PROP_TAG_TYPE_EITHER) {
			return false;
		}
		ctx->poic_tag_type = _PROP_TAG_TYPE_START;
	}

	ctx->poic_tagname = cp;

	while (!_PROP_ISSPACE(*cp) && *cp != '/' && *cp != '>') {
		if (_PROP_EOF(*cp)) {
			return false;
		}
		cp++;
	}

	ctx->poic_tagname_len = cp - ctx->poic_tagname;

	/* Make sure this is the tag we're looking for. */
	if (tag != NULL &&
	    (taglen != ctx->poic_tagname_len ||
	     memcmp(tag, ctx->poic_tagname, taglen) != 0)) {
		return false;
	}

	/* Check for empty tag. */
	if (*cp == '/') {
		if (ctx->poic_tag_type != _PROP_TAG_TYPE_START) {
			return false;		/* only valid on start tags */
		}
		ctx->poic_is_empty_element = true;
		cp++;
		if (_PROP_EOF(*cp) || *cp != '>') {
			return false;
		}
	} else {
		ctx->poic_is_empty_element = false;
	}

	/* Easy case of no arguments. */
	if (*cp == '>') {
		ctx->poic_tagattr = NULL;
		ctx->poic_tagattr_len = 0;
		ctx->poic_tagattrval = NULL;
		ctx->poic_tagattrval_len = 0;
		ctx->poic_cp = cp + 1;
		return true;
	}

	_PROP_ASSERT(!_PROP_EOF(*cp));
	cp++;
	if (_PROP_EOF(*cp)) {
		return false;
	}

	cp = _prop_intern_skip_whitespace(cp);
	if (_PROP_EOF(*cp)) {
		return false;
	}

	ctx->poic_tagattr = cp;

	while (!_PROP_ISSPACE(*cp) && *cp != '=') {
		if (_PROP_EOF(*cp)) {
			return false;
		}
		cp++;
	}

	ctx->poic_tagattr_len = cp - ctx->poic_tagattr;

	cp++;
	if (*cp != '\"') {
		return false;
	}
	cp++;
	if (_PROP_EOF(*cp)) {
		return false;
	}

	ctx->poic_tagattrval = cp;
	while (*cp != '\"') {
		if (_PROP_EOF(*cp)) {
			return false;
		}
		cp++;
	}
	ctx->poic_tagattrval_len = cp - ctx->poic_tagattrval;

	cp++;
	if (*cp != '>') {
		return false;
	}

	ctx->poic_cp = cp + 1;
	return true;
}

#define	INTERNALIZER(t, f)			\
{	t,	sizeof(t) - 1,		f	}

static const struct _prop_object_internalizer {
	const char			*poi_tag;
	size_t				poi_taglen;
	prop_object_internalizer_t	poi_intern;
} _prop_object_internalizer_table[] = {
	INTERNALIZER("array", _prop_array_internalize),

	INTERNALIZER("true", _prop_bool_internalize),
	INTERNALIZER("false", _prop_bool_internalize),

	INTERNALIZER("data", _prop_data_internalize),

	INTERNALIZER("dict", _prop_dictionary_internalize),

	INTERNALIZER("integer", _prop_number_internalize),

	INTERNALIZER("string", _prop_string_internalize),

	{ 0, 0, NULL }
};

#undef INTERNALIZER

/*
 * _prop_xml_intern_by_tag --
 *	Determine the object type from the tag in the context and
 *	internalize it.
 */
static prop_object_t
_prop_xml_intern_by_tag(struct _prop_object_internalize_context *ctx)
{
	const struct _prop_object_internalizer *poi;
	prop_object_t obj, parent_obj;
	void *data, *iter;
	prop_object_internalizer_continue_t iter_func;
	struct _prop_stack stack;

	_prop_stack_init(&stack);

match_start:
	for (poi = _prop_object_internalizer_table;
	     poi->poi_tag != NULL; poi++) {
		if (_prop_intern_match(ctx->poic_tagname,
				       ctx->poic_tagname_len,
				       poi->poi_tag,
				       poi->poi_taglen)) {
			break;
		}
	}
	if (poi == NULL || poi->poi_tag == NULL) {
		while (_prop_stack_pop(&stack, &obj, &iter, &data, NULL)) {
			iter_func = (prop_object_internalizer_continue_t)iter;
			(*iter_func)(&stack, &obj, ctx, data, NULL);
		}
		return NULL;
	}

	obj = NULL;
	if (!(*poi->poi_intern)(&stack, &obj, ctx)) {
		goto match_start;
	}

	parent_obj = obj;
	while (_prop_stack_pop(&stack, &parent_obj, &iter, &data, NULL)) {
		iter_func = (prop_object_internalizer_continue_t)iter;
		if (!(*iter_func)(&stack, &parent_obj, ctx, data, obj)) {
			goto match_start;
		}
		obj = parent_obj;
	}

	return parent_obj;
}

#define	ADDCHAR(x)							\
	do {								\
		if (target) {						\
			if (tarindex >= targsize) {			\
				return false;				\
			}						\
			target[tarindex] = (x);				\
		}							\
		tarindex++;						\
	} while (/*CONSTCOND*/0)

/*
 * _prop_json_intern_decode_uesc_getu16 --
 *	Get the 16-bit value from a "u-escape" ("\uXXXX").
 */
static unsigned int
_prop_json_intern_decode_uesc_getu16(const char *src, unsigned int idx,
    uint16_t *valp)
{
	unsigned int i;
	uint16_t val;
	unsigned char c;

	if (src[idx] != '\\' || src[idx + 1] != 'u') {
		return 0;
	}

	for (val = 0, i = 2; i < 6; i++) {
		val <<= 4;
		c = src[idx + i];
		if (c >= 'A' && c <= 'F') {
			val |= 10 + (c - 'A');
		} else if (c >= 'a' && c <= 'f') {
			val |= 10 + (c - 'a');
		} else if (c >= '0' && c <= '9') {
			val |= c - '0';
		} else {
			return 0;
		}
	}

	*valp = val;
	return idx + i;
}

#define	HS_FIRST	0xd800
#define	HS_LAST		0xdbff
#define	HS_SHIFT	10
#define	LS_FIRST	0xdc00
#define	LS_LAST		0xdfff

#define	HIGH_SURROGAGE_P(x)	\
	((x) >= HS_FIRST && (x) <= HS_LAST)
#define	LOW_SURROGATE_P(x)	\
	((x) >= LS_FIRST && (x) <= LS_LAST)
#define	SURROGATE_P(x)		\
	(HIGH_SURROGAGE_P(x) || LOW_SURROGATE_P(x))

/*
 * _prop_json_intern_decode_uesc --
 *	Decode a JSON UTF-16 "u-escape" ("\uXXXX").
 */
static int
_prop_json_intern_decode_uesc(const char *src, char *c, unsigned int *cszp)
{
	unsigned int idx = 0;
	uint32_t code;
	uint16_t code16[2] = { 0, 0 };
	uint8_t u, v, w,x, y, z;

	idx = _prop_json_intern_decode_uesc_getu16(src, idx, &code16[0]);
	if (idx == 0) {
		return 0;
	}
	if (! SURROGATE_P(code16[0])) {
		/* Simple case: not a surrogate pair */
		code = code16[0];
	} else if (HIGH_SURROGAGE_P(code16[0])) {
		idx = _prop_json_intern_decode_uesc_getu16(src, idx,
							   &code16[1]);
		if (idx == 0) {
			return 0;
		}
		/* Next code must be the low surrogate. */
		if (! LOW_SURROGATE_P(code16[1])) {
			return 0;
		}
		code = (((uint32_t)code16[0] - HS_FIRST) << HS_SHIFT) +
		        (          code16[1] - LS_FIRST)              +
		       0x10000;
	} else {
		/* Got the low surrogate first; this is an error. */
		return 0;
	}

	/*
	 * Ok, we have the code point.  Now convert it to UTF-8.
	 * First we'll just split into nybbles.
	 */
	u = (code >> 20) & 0xf;
	v = (code >> 16) & 0xf;
	w = (code >> 12) & 0xf;
	x = (code >>  8) & 0xf;
	y = (code >>  4) & 0xf;
	z = (code      ) & 0xf;

	/*
	 * ...and swizzle the nybbles accordingly.
	 *
	 * N.B. we expcitly disallow inserting a NUL into the string
	 * by way of a \uXXXX escape.
	 */
	if (code == 0) {
		/* Not allowed. */
		return 0;
	} else if (/*code >= 0x0000 &&*/ code <= 0x007f) {
		c[0] = (char)code;	/* == (y << 4) | z */
		*cszp = 1;
	} else if (/*code >= 0x0080 &&*/ code <= 0x07ff) {
		c[0] = 0xc0 | (x << 2) | (y >> 2);
		c[1] = 0x80 | ((y & 3) << 4) | z;
		*cszp = 2;
	} else if (/*code >= 0x0800 &&*/ code <= 0xffff) {
		c[0] = 0xe0 | w;
		c[1] = 0x80 | (x << 2) | (y >> 2);
		c[2] = 0x80 | ((y & 3) << 4) | z;
		*cszp = 3;
	} else if (/*code >= 0x010000 &&*/ code <= 0x10ffff) {
		c[0] = 0xf0 | ((u & 1) << 2) | (v >> 2);
		c[1] = 0x80 | ((v & 3) << 4) | w;
		c[2] = 0x80 | (x << 2) | (y >> 2);
		c[3] = 0x80 | ((y & 3) << 4) | z;
		*cszp = 4;
	} else {
		/* Invalid code. */
		return 0;
	}

	return idx;	/* advance input by this much */
}

#undef HS_FIRST
#undef HS_LAST
#undef LS_FIRST
#undef LS_LAST
#undef HIGH_SURROGAGE_P
#undef LOW_SURROGATE_P
#undef SURROGATE_P

/*
 * _prop_json_intern_decode_string --
 *	Decode a JSON-encoded string.
 */
static bool
_prop_json_intern_decode_string(struct _prop_object_internalize_context *ctx,
    char *target, size_t targsize, size_t *sizep,
    const char **cpp)
{
	const char *src;
	size_t tarindex;
	char c[4];
	unsigned int csz;

	tarindex = 0;
	src = ctx->poic_cp;

	for (;;) {
		if (_PROP_EOF(*src)) {
			return false;
		}
		if (*src == '"') {
			break;
		}

		csz = 1;
		if ((c[0] = *src) == '\\') {
			int advance = 2;

			switch ((c[0] = src[1])) {
			case '"':		/* quotation mark */
			case '\\':		/* reverse solidus */
			case '/':		/* solidus */
				/* identity mapping */
				break;

			case 'b':		/* backspace */
				c[0] = 0x08;
				break;

			case 'f':		/* form feed */
				c[0] = 0x0c;
				break;

			case 'n':		/* line feed */
				c[0] = 0x0a;
				break;

			case 'r':		/* carriage return */
				c[0] = 0x0d;
				break;

			case 't':		/* tab */
				c[0] = 0x09;
				break;

			case 'u':
				advance = _prop_json_intern_decode_uesc(
				    src, c, &csz);
				if (advance == 0) {
					return false;
				}
				break;

			default:
				/* invalid escape */
				return false;
			}
			src += advance;
		} else {
			src++;
		}
		for (unsigned int i = 0; i < csz; i++) {
			ADDCHAR(c[i]);
		}
	}

	_PROP_ASSERT(*src == '"');
	if (sizep != NULL) {
		*sizep = tarindex;
	}
	if (cpp != NULL) {
		*cpp = src;
	}

	return true;
}

/*
 * _prop_xml_intern_decode_string --
 *	Decode an XML-encoded string.
 */
static bool
_prop_xml_intern_decode_string(struct _prop_object_internalize_context *ctx,
    char *target, size_t targsize, size_t *sizep,
    const char **cpp)
{
	const char *src;
	size_t tarindex;
	char c;

	tarindex = 0;
	src = ctx->poic_cp;

	for (;;) {
		if (_PROP_EOF(*src)) {
			return true;
		}
		if (*src == '<') {
			break;
		}

		if ((c = *src) == '&') {
			if (src[1] == 'a' &&
			    src[2] == 'm' &&
			    src[3] == 'p' &&
			    src[4] == ';') {
				c = '&';
				src += 5;
			} else if (src[1] == 'l' &&
				   src[2] == 't' &&
				   src[3] == ';') {
				c = '<';
				src += 4;
			} else if (src[1] == 'g' &&
				   src[2] == 't' &&
				   src[3] == ';') {
				c = '>';
				src += 4;
			} else if (src[1] == 'a' &&
				   src[2] == 'p' &&
				   src[3] == 'o' &&
				   src[4] == 's' &&
				   src[5] == ';') {
				c = '\'';
				src += 6;
			} else if (src[1] == 'q' &&
				   src[2] == 'u' &&
				   src[3] == 'o' &&
				   src[4] == 't' &&
				   src[5] == ';') {
				c = '\"';
				src += 6;
			} else {
				return false;
			}
		} else {
			src++;
		}
		ADDCHAR(c);
	}

	_PROP_ASSERT(*src == '<');
	if (sizep != NULL) {
		*sizep = tarindex;
	}
	if (cpp != NULL) {
		*cpp = src;
	}

	return true;
}

#undef ADDCHAR

/*
 * _prop_intern_decode_string --
 *	Decode an encoded string.
 */
bool
_prop_intern_decode_string(struct _prop_object_internalize_context *ctx,
    char *target, size_t targsize, size_t *sizep,
    const char **cpp)
{
	_PROP_ASSERT(ctx->poic_format == PROP_FORMAT_XML ||
		     ctx->poic_format == PROP_FORMAT_JSON);

	switch (ctx->poic_format) {
	case PROP_FORMAT_JSON:
		return _prop_json_intern_decode_string(ctx, target, targsize,
		    sizep, cpp);

	default:		/* PROP_FORMAT_XML */
		return _prop_xml_intern_decode_string(ctx, target, targsize,
		    sizep, cpp);
	}
}

/*
 * _prop_intern_context_alloc --
 *	Allocate an internalize context.
 */
static struct _prop_object_internalize_context *
_prop_intern_context_alloc(const char *data, prop_format_t fmt)
{
	struct _prop_object_internalize_context *ctx;

	ctx = _PROP_MALLOC(sizeof(*ctx), M_TEMP);
	if (ctx == NULL) {
		return NULL;
	}

	ctx->poic_format = fmt;
	ctx->poic_data = ctx->poic_cp = data;

	/*
	 * If we're digesting JSON, check for a byte order mark and
	 * skip it, if present.  We should never see one, but we're
	 * allowed to detect and ignore it.  (RFC 8259 section 8.1)
	 */
	if (fmt == PROP_FORMAT_JSON) {
		if (((unsigned char)data[0] == 0xff &&
		     (unsigned char)data[1] == 0xfe) ||
		    ((unsigned char)data[0] == 0xfe &&
		     (unsigned char)data[1] == 0xff)) {
			ctx->poic_cp = data + 2;
		}

		/* No additional processing work to do for JSON. */
		return ctx;
	}

	/*
	 * Skip any whitespace and XML preamble stuff that we don't
	 * know about / care about.
	 */
	for (;;) {
		data = _prop_intern_skip_whitespace(data);
		if (_PROP_EOF(*data) || *data != '<') {
			goto bad;
		}

#define	MATCH(str)	(strncmp(&data[1], str, strlen(str)) == 0)

		/*
		 * Skip over the XML preamble that Apple XML property
		 * lists usually include at the top of the file.
		 */
		if (MATCH("?xml ") ||
		    MATCH("!DOCTYPE plist")) {
			while (*data != '>' && !_PROP_EOF(*data)) {
				data++;
			}
			if (_PROP_EOF(*data)) {
				goto bad;
			}
			data++;	/* advance past the '>' */
			continue;
		}

		if (MATCH("<!--")) {
			ctx->poic_cp = data + 4;
			if (_prop_xml_intern_skip_comment(ctx) == false) {
				goto bad;
			}
			data = ctx->poic_cp;
			continue;
		}

#undef MATCH

		/*
		 * We don't think we should skip it, so let's hope we can
		 * parse it.
		 */
		break;
	}

	ctx->poic_cp = data;
	return ctx;
 bad:
	_PROP_FREE(ctx, M_TEMP);
	return NULL;
}

/*
 * _prop_intern_context_free --
 *	Free an internalize context.
 */
static void
_prop_intern_context_free(struct _prop_object_internalize_context *ctx)
{
	_PROP_FREE(ctx, M_TEMP);
}

/*
 * _prop_object_internalize_json --
 *	Internalize a property list from JSON data.
 */
static prop_object_t
_prop_object_internalize_json(struct _prop_object_internalize_context *ctx,
    const struct _prop_object_type_tags *initial_tag _PROP_ARG_UNUSED)
{
	prop_object_t obj, parent_obj;
	void *data, *iter;
	prop_object_internalizer_continue_t iter_func;
	struct _prop_stack stack;
	bool (*intern)(prop_stack_t, prop_object_t *,
		       struct _prop_object_internalize_context *);

	_prop_stack_init(&stack);

 match_start:
	intern = NULL;
	ctx->poic_tagname = ctx->poic_tagattr = ctx->poic_tagattrval = NULL;
	ctx->poic_tagname_len = ctx->poic_tagattr_len =
	    ctx->poic_tagattrval_len = 0;
	ctx->poic_is_empty_element = false;
	ctx->poic_cp = _prop_intern_skip_whitespace(ctx->poic_cp);
	switch (ctx->poic_cp[0]) {
	case '{':
		ctx->poic_cp++;
		intern = _prop_dictionary_internalize;
		break;

	case '[':
		ctx->poic_cp++;
		intern = _prop_array_internalize;
		break;

	case '"':
		ctx->poic_cp++;
		/* XXX Slightly gross. */
		if (*ctx->poic_cp == '"') {
			ctx->poic_cp++;
			ctx->poic_is_empty_element = true;
		}
		intern = _prop_string_internalize;
		break;

	case 't':
		if (ctx->poic_cp[1] == 'r' &&
		    ctx->poic_cp[2] == 'u' &&
		    ctx->poic_cp[3] == 'e') {
			/* XXX Slightly gross. */
			ctx->poic_tagname = ctx->poic_cp;
			ctx->poic_tagname_len = 4;
			ctx->poic_is_empty_element = true;
			intern = _prop_bool_internalize;
			ctx->poic_cp += 4;
		}
		break;

	case 'f':
		if (ctx->poic_cp[1] == 'a' &&
		    ctx->poic_cp[2] == 'l' &&
		    ctx->poic_cp[3] == 's' &&
		    ctx->poic_cp[4] == 'e') {
			/* XXX Slightly gross. */
			ctx->poic_tagname = ctx->poic_cp;
			ctx->poic_tagname_len = 5;
			ctx->poic_is_empty_element = true;
			intern = _prop_bool_internalize;
			ctx->poic_cp += 5;
		}
		break;

	default:
		if (ctx->poic_cp[0] == '+' ||
		    ctx->poic_cp[0] == '-' ||
		    (ctx->poic_cp[0] >= '0' && ctx->poic_cp[0] <= '9')) {
			intern = _prop_number_internalize;
		}
		break;
	}

	if (intern == NULL) {
		while (_prop_stack_pop(&stack, &obj, &iter, &data, NULL)) {
			iter_func = (prop_object_internalizer_continue_t)iter;
			(*iter_func)(&stack, &obj, ctx, data, NULL);
		}
		return NULL;
	}

	obj = NULL;
	if ((*intern)(&stack, &obj, ctx) == false) {
		goto match_start;
	}

	parent_obj = obj;
	while (_prop_stack_pop(&stack, &parent_obj, &iter, &data, NULL)) {
		iter_func = (prop_object_internalizer_continue_t)iter;
		if ((*iter_func)(&stack, &parent_obj, ctx, data,
				 obj) == false) {
			goto match_start;
		}
		obj = parent_obj;
	}

	/* Ensure there's no trailing junk. */
	if (parent_obj != NULL) {
		ctx->poic_cp = _prop_intern_skip_whitespace(ctx->poic_cp);
		if (!_PROP_EOF(*ctx->poic_cp)) {
			prop_object_release(parent_obj);
			parent_obj = NULL;
		}
	}
	return parent_obj;
}

/*
 * _prop_object_internalize_xml --
 *	Internalize a property list from XML data.
 */
static prop_object_t
_prop_object_internalize_xml(struct _prop_object_internalize_context *ctx,
    const struct _prop_object_type_tags *initial_tag)
{
	prop_object_t obj = NULL;

	/* We start with a <plist> tag. */
	if (_prop_xml_intern_find_tag(ctx, "plist",
				      _PROP_TAG_TYPE_START) == false) {
		goto out;
	}

	/* Plist elements cannot be empty. */
	if (ctx->poic_is_empty_element) {
		goto out;
	}

	/*
	 * We don't understand any plist attributes, but Apple XML
	 * property lists often have a "version" attribute.  If we
	 * see that one, we simply ignore it.
	 */
	if (ctx->poic_tagattr != NULL &&
	    !_PROP_TAGATTR_MATCH(ctx, "version")) {
		goto out;
	}

	/* Next we expect to see opening main tag. */
	if (_prop_xml_intern_find_tag(ctx,
				initial_tag != NULL ? initial_tag->xml_tag
						    : NULL,
				_PROP_TAG_TYPE_START) == false) {
		goto out;
	}

	obj = _prop_xml_intern_by_tag(ctx);
	if (obj == NULL) {
		goto out;
	}

	/*
	 * We've advanced past the closing main tag.
	 * Now we want </plist>.
	 */
	if (_prop_xml_intern_find_tag(ctx, "plist",
				      _PROP_TAG_TYPE_END) == false) {
		prop_object_release(obj);
		obj = NULL;
	}
 out:
	return obj;
}

/*
 * _prop_object_internalize --
 *	Internalize a property list from a NUL-terminated data blob.
 */
prop_object_t
_prop_object_internalize(const char *data,
    const struct _prop_object_type_tags *initial_tag)
{
	struct _prop_object_internalize_context *ctx;
	prop_object_t obj;
	prop_format_t fmt;

	/*
	 * Skip all whitespace until and look at the first
	 * non-whitespace character to determine the format:
	 * An XML plist will always have '<' as the first non-ws
	 * character.  If we encounter something else, we assume
	 * it is JSON.
	 */
	data = _prop_intern_skip_whitespace(data);
	if (_PROP_EOF(*data)) {
		return NULL;
	}

	fmt = *data == '<' ? PROP_FORMAT_XML : PROP_FORMAT_JSON;

	ctx = _prop_intern_context_alloc(data, fmt);
	if (ctx == NULL) {
		return NULL;
	}

	switch (fmt) {
	case PROP_FORMAT_JSON:
		obj = _prop_object_internalize_json(ctx, initial_tag);
		break;

	default:		/* PROP_FORMAT_XML */
		obj = _prop_object_internalize_xml(ctx, initial_tag);
		break;
	}

	_prop_intern_context_free(ctx);
	return obj;
}

_PROP_EXPORT prop_object_t
prop_object_internalize(const char *data)
{
	return _prop_object_internalize(data, NULL);
}

struct _prop_intern_mapped_file {
	char *	pimf_data;
	size_t	pimf_mapsize;
};

/*
 * _prop_intern_map_file --
 *	Map a file for the purpose of internalizing it.
 */
static struct _prop_intern_mapped_file *
_prop_intern_map_file(const char *fname)
{
	struct stat sb;
	struct _prop_intern_mapped_file *mf;
	size_t pgsize = (size_t)sysconf(_SC_PAGESIZE);
	size_t pgmask = pgsize - 1;
	bool need_guard;
	int fd;

	mf = _PROP_MALLOC(sizeof(*mf), M_TEMP);
	if (mf == NULL) {
		return NULL;
	}

	fd = open(fname, O_RDONLY, 0400);
	if (fd == -1) {
		_PROP_FREE(mf, M_TEMP);
		return NULL;
	}

	if (fstat(fd, &sb) == -1) {
		(void) close(fd);
		_PROP_FREE(mf, M_TEMP);
		return NULL;
	}
	mf->pimf_mapsize = ((size_t)sb.st_size + pgmask) & ~pgmask;
	if (mf->pimf_mapsize < (size_t)sb.st_size) {
		(void) close(fd);
		_PROP_FREE(mf, M_TEMP);
		return NULL;
	}

	/*
	 * If the file length is an integral number of pages, then we
	 * need to map a guard page at the end in order to provide the
	 * necessary NUL-termination of the buffer.
	 */
	need_guard = (sb.st_size & pgmask) == 0;

	mf->pimf_data = mmap(NULL, need_guard ? mf->pimf_mapsize + pgsize
					      : mf->pimf_mapsize,
			     PROT_READ, MAP_FILE|MAP_SHARED, fd, (off_t)0);
	(void) close(fd);
	if (mf->pimf_data == MAP_FAILED) {
		_PROP_FREE(mf, M_TEMP);
		return (NULL);
	}
#ifdef POSIX_MADV_SEQUENTIAL
	(void) posix_madvise(mf->pimf_data, mf->pimf_mapsize,
	    POSIX_MADV_SEQUENTIAL);
#endif

	if (need_guard) {
		if (mmap(mf->pimf_data + mf->pimf_mapsize,
			 pgsize, PROT_READ,
			 MAP_ANON|MAP_PRIVATE|MAP_FIXED, -1,
			 (off_t)0) == MAP_FAILED) {
			(void) munmap(mf->pimf_data, mf->pimf_mapsize);
			_PROP_FREE(mf, M_TEMP);
			return NULL;
		}
		mf->pimf_mapsize += pgsize;
	}
	return mf;
}

/*
 * _prop_intern_unmap_file --
 *	Unmap a file previously mapped for internalizing.
 */
static void
_prop_intern_unmap_file(struct _prop_intern_mapped_file *mf)
{
#ifdef POSIX_MADV_DONTNEED
	(void) posix_madvise(mf->pimf_data, mf->pimf_mapsize,
	    POSIX_MADV_DONTNEED);
#endif
	(void) munmap(mf->pimf_data, mf->pimf_mapsize);
	_PROP_FREE(mf, M_TEMP);
}

/*
 * _prop_object_internalize_from_file --
 *	Internalize a property list from a file.
 */
prop_object_t
_prop_object_internalize_from_file(const char *fname,
    const struct _prop_object_type_tags *initial_tag)
{
	struct _prop_intern_mapped_file *mf;
	prop_object_t obj;

	mf = _prop_intern_map_file(fname);
	if (mf == NULL) {
		return NULL;
	}
	obj = _prop_object_internalize(mf->pimf_data, initial_tag);
	_prop_intern_unmap_file(mf);

	return obj;
}

_PROP_EXPORT prop_object_t
prop_object_internalize_from_file(const char *fname)
{
	return _prop_object_internalize_from_file(fname, NULL);
}
