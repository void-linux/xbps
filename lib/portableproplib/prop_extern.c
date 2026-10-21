/*	$NetBSD: prop_extern.c,v 1.3 2025/12/10 21:33:01 andvar Exp $	*/

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

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>

#define	BUF_EXPAND	256
#define	PLISTTMP	"/.plistXXXXXX"

static prop_format_t	_prop_format_default = PROP_FORMAT_XML;

/*
 * _prop_extern_append_char --
 *	Append a single character to the externalize buffer.
 */
bool
_prop_extern_append_char(
    struct _prop_object_externalize_context *ctx, unsigned char c)
{

	_PROP_ASSERT(ctx->poec_capacity != 0);
	_PROP_ASSERT(ctx->poec_buf != NULL);
	_PROP_ASSERT(ctx->poec_len <= ctx->poec_capacity);

	if (ctx->poec_len == ctx->poec_capacity) {
		char *cp = _PROP_REALLOC(ctx->poec_buf,
					 ctx->poec_capacity + BUF_EXPAND,
					 M_TEMP);
		if (cp == NULL) {
			return false;
		}
		ctx->poec_capacity = ctx->poec_capacity + BUF_EXPAND;
		ctx->poec_buf = cp;
	}

	ctx->poec_buf[ctx->poec_len++] = c;

	return true;
}

/*
 * _prop_extern_append_cstring --
 *	Append a C string to the externalize buffer.
 */
bool
_prop_extern_append_cstring(
    struct _prop_object_externalize_context *ctx, const char *cp)
{

	while (*cp != '\0') {
		if (_prop_extern_append_char(ctx,
					     (unsigned char)*cp) == false) {
			return false;
		}
		cp++;
	}
	return true;
}

/*
 * _prop_json_extern_append_encoded_cstring --
 *	Append a C string to the externalize buffer, JSON-encoded.
 */
static bool
_prop_json_extern_append_escu(
    struct _prop_object_externalize_context *ctx, uint16_t val)
{
	char tmpstr[sizeof("\\uXXXX")];

	snprintf(tmpstr, sizeof(tmpstr), "\\u%04X", val);
	return _prop_extern_append_cstring(ctx, tmpstr);
}

static bool
_prop_json_extern_append_encoded_cstring(
    struct _prop_object_externalize_context *ctx, const char *cp)
{
	bool esc;
	unsigned char ch;

	for (; (ch = *cp) != '\0'; cp++) {
		esc = true;
		switch (ch) {
		/*
		 * First, the two explicit exclusions.  They must be
		 * escaped.
		 */
		case '"':	/* U+0022 quotation mark */
			goto emit;

		case '\\':	/* U+005C reverse solidus */
			goto emit;

		/*
		 * And some special cases that are explcit in the grammar.
		 */
		case '/':	/* U+002F solidus (XXX this one seems silly) */
			goto emit;

		case 0x08:	/* U+0008 backspace */
			ch = 'b';
			goto emit;

		case 0x0c:	/* U+000C form feed */
			ch = 'f';
			goto emit;

		case 0x0a:	/* U+000A line feed */
			ch = 'n';
			goto emit;

		case 0x0d:	/* U+000D carriage return */
			ch = 'r';
			goto emit;

		case 0x09:	/* U+0009 tab */
			ch = 't';
			goto emit;

		default:
			/*
			 * \u-escape all other single-byte ASCII control
			 * characters, per RFC 8259:
			 *
			 * <quote>
			 * All Unicode characters may be placed within the
			 * quotation marks, except for the characters that
			 * MUST be escaped: quotation mark, reverse solidus,
			 * and the control characters (U+0000 through U+001F).
			 * </quote>
			 */
			if (ch < 0x20) {
				if (_prop_json_extern_append_escu(ctx,
							ch) == false) {
					return false;
				}
				break;
			}
			/*
			 * We're going to just treat everything else like
			 * UTF-8 (we've been handed a C-string, after all)
			 * and pretend it will be OK.
			 */
			esc = false;
		emit:
			if ((esc && _prop_extern_append_char(ctx,
							'\\') == false) ||
			    _prop_extern_append_char(ctx, ch) == false) {
				return false;
			}
			break;
		}
	}

	return true;
}

/*
 * _prop_xml_extern_append_encoded_cstring --
 *	Append a C string to the externalize buffer, XML-encoded.
 */
static bool
_prop_xml_extern_append_encoded_cstring(
    struct _prop_object_externalize_context *ctx, const char *cp)
{
	bool rv;
	unsigned char ch;

	for (rv = true; rv && (ch = *cp) != '\0'; cp++) {
		switch (ch) {
		case '<':
			rv = _prop_extern_append_cstring(ctx, "&lt;");
			break;
		case '>':
			rv = _prop_extern_append_cstring(ctx, "&gt;");
			break;
		case '&':
			rv = _prop_extern_append_cstring(ctx, "&amp;");
			break;
		default:
			rv = _prop_extern_append_char(ctx, ch);
			break;
		}
	}

	return rv;
}

/*
 * _prop_extern_append_encoded_cstring --
 *	Append a C string to the externalize buffer, encoding it for
 *	the selected format.
 */
bool
_prop_extern_append_encoded_cstring(
    struct _prop_object_externalize_context *ctx, const char *cp)
{
	_PROP_ASSERT(ctx->poec_format == PROP_FORMAT_XML ||
		     ctx->poec_format == PROP_FORMAT_JSON);

	switch (ctx->poec_format) {
	case PROP_FORMAT_JSON:
		return _prop_json_extern_append_encoded_cstring(ctx, cp);

	default:		/* PROP_FORMAT_XML */
		return _prop_xml_extern_append_encoded_cstring(ctx, cp);
	}
}

/*
 * _prop_extern_start_line --
 *	Append the start-of-line character sequence.
 */
bool
_prop_extern_start_line(
    struct _prop_object_externalize_context *ctx)
{
	unsigned int i;

	for (i = 0; i < ctx->poec_depth; i++) {
		if (_prop_extern_append_char(ctx, '\t') == false) {
			return false;
		}
	}
	return true;
}

/*
 * _prop_extern_end_line --
 *	Append the end-of-line character sequence.
 */
bool
_prop_extern_end_line(
    struct _prop_object_externalize_context *ctx, const char *trailer)
{
	if (trailer != NULL &&
	    _prop_extern_append_cstring(ctx, trailer) == false) {
		return false;
	}
	return _prop_extern_append_char(ctx, '\n');
}

/*
 * _prop_extern_append_start_tag --
 *	Append an item's start tag to the externalize buffer.
 */
bool
_prop_extern_append_start_tag(
    struct _prop_object_externalize_context *ctx,
    const struct _prop_object_type_tags *tags,
    const char *tagattrs)
{
	bool rv;

	_PROP_ASSERT(ctx->poec_format == PROP_FORMAT_XML ||
		     ctx->poec_format == PROP_FORMAT_JSON);

	switch (ctx->poec_format) {
	case PROP_FORMAT_JSON:
		rv = tags->json_open_tag == NULL ||
		     _prop_extern_append_cstring(ctx, tags->json_open_tag);
		break;

	default:		/* PROP_FORMAT_XML */
		rv = _prop_extern_append_char(ctx, '<') &&
		     _prop_extern_append_cstring(ctx, tags->xml_tag) &&
		     (tagattrs == NULL ||
		      (_prop_extern_append_char(ctx, ' ') &&
		       _prop_extern_append_cstring(ctx, tagattrs))) &&
		      _prop_extern_append_char(ctx, '>');
		break;
	}

	return rv;
}

/*
 * _prop_extern_append_end_tag --
 *	Append an item's end tag to the externalize buffer.
 */
bool
_prop_extern_append_end_tag(
    struct _prop_object_externalize_context *ctx,
    const struct _prop_object_type_tags *tags)
{
	bool rv;

	_PROP_ASSERT(ctx->poec_format == PROP_FORMAT_XML ||
		     ctx->poec_format == PROP_FORMAT_JSON);

	switch (ctx->poec_format) {
	case PROP_FORMAT_JSON:
		rv = tags->json_close_tag == NULL ||
		     _prop_extern_append_cstring(ctx, tags->json_close_tag);
		break;

	default:		/* PROP_FORMAT_XML */
		rv = _prop_extern_append_char(ctx, '<') &&
		     _prop_extern_append_char(ctx, '/') &&
		     _prop_extern_append_cstring(ctx, tags->xml_tag) &&
		     _prop_extern_append_char(ctx, '>');
		break;
	}

	return rv;
}

/*
 * _prop_extern_append_empty_tag --
 *	Append an item's empty tag to the externalize buffer.
 */
bool
_prop_extern_append_empty_tag(
    struct _prop_object_externalize_context *ctx,
    const struct _prop_object_type_tags *tags)
{
	bool rv;

	_PROP_ASSERT(ctx->poec_format == PROP_FORMAT_XML ||
		     ctx->poec_format == PROP_FORMAT_JSON);

	switch (ctx->poec_format) {
	case PROP_FORMAT_JSON:
		if (tags->json_open_tag == NULL ||
		    _prop_extern_append_cstring(ctx,
					tags->json_open_tag) == false) {
			return false;
		}
		if (tags->json_empty_sep != NULL &&
		    _prop_extern_append_cstring(ctx,
					tags->json_empty_sep) == false) {
			return false;
		}
		if (tags->json_close_tag != NULL) {
			rv = _prop_extern_append_cstring(ctx,
					tags->json_close_tag);
		} else {
			rv = true;
		}
		break;

	default:		/* PROP_FORMAT_XML */
		rv = _prop_extern_append_char(ctx, '<') &&
		     _prop_extern_append_cstring(ctx, tags->xml_tag) &&
		     _prop_extern_append_char(ctx, '/') &&
		     _prop_extern_append_char(ctx, '>');
		break;
	}

	return rv;
}

static const struct _prop_object_type_tags _plist_type_tags = {
	.xml_tag	=	"plist",
};

/*
 * _prop_extern_append_header --
 *	Append the header to the externalize buffer.
 */
static bool
_prop_extern_append_header(struct _prop_object_externalize_context *ctx)
{
	static const char _plist_xml_header[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";

	if (ctx->poec_format != PROP_FORMAT_XML) {
		return true;
	}

	if (_prop_extern_append_cstring(ctx, _plist_xml_header) == false ||
	    _prop_extern_append_start_tag(ctx,
					&_plist_type_tags,
					"version=\"1.0\"") == false ||
	    _prop_extern_append_char(ctx, '\n') == false) {
		return false;
	}

	return true;
}

/*
 * _prop_extern_append_footer --
 *	Append the footer to the externalize buffer.  This also
 *	NUL-terminates the buffer.
 */
static bool
_prop_extern_append_footer(struct _prop_object_externalize_context *ctx)
{
	if (_prop_extern_end_line(ctx, NULL) == false) {
		return false;
	}

	if (ctx->poec_format == PROP_FORMAT_XML) {
		if (_prop_extern_append_end_tag(ctx,
					&_plist_type_tags) == false ||
		    _prop_extern_end_line(ctx, NULL) == false) {
			return false;
		}
	}

	return _prop_extern_append_char(ctx, '\0');
}

/*
 * _prop_extern_context_alloc --
 *	Allocate an externalize context.
 */
static struct _prop_object_externalize_context *
_prop_extern_context_alloc(prop_format_t fmt)
{
	struct _prop_object_externalize_context *ctx;

	ctx = _PROP_MALLOC(sizeof(*ctx), M_TEMP);
	if (ctx != NULL) {
		ctx->poec_buf = _PROP_MALLOC(BUF_EXPAND, M_TEMP);
		if (ctx->poec_buf == NULL) {
			_PROP_FREE(ctx, M_TEMP);
			return NULL;
		}
		ctx->poec_len = 0;
		ctx->poec_capacity = BUF_EXPAND;
		ctx->poec_depth = 0;
		ctx->poec_format = fmt;
	}
	return ctx;
}

/*
 * _prop_extern_context_free --
 *	Free an externalize context.
 */
static void
_prop_extern_context_free(struct _prop_object_externalize_context *ctx)
{
	/* Buffer is always freed by the caller. */
	_PROP_FREE(ctx, M_TEMP);
}

/*
 * _prop_object_externalize --
 *	Externalize an object, returning a NUL-terminated buffer
 *	containing the serialized data in either XML or JSON format.
 *	The buffer is allocated with the M_TEMP memory type.
 */
char *
_prop_object_externalize(struct _prop_object *obj, prop_format_t fmt)
{
	struct _prop_object_externalize_context *ctx;
	char *cp = NULL;

	if (obj == NULL || obj->po_type->pot_extern == NULL) {
		return NULL;
	}
	if (fmt != PROP_FORMAT_XML && fmt != PROP_FORMAT_JSON) {
		return NULL;
	}

	ctx = _prop_extern_context_alloc(fmt);
	if (ctx == NULL) {
		return NULL;
	}

	if (_prop_extern_append_header(ctx) == false ||
	    obj->po_type->pot_extern(ctx, obj) == false ||
	    _prop_extern_append_footer(ctx) == false) {
		/* We are responsible for releasing the buffer. */
		_PROP_FREE(ctx->poec_buf, M_TEMP);
		goto bad;
	}

	cp = ctx->poec_buf;
 bad:
	_prop_extern_context_free(ctx);
	return cp;
}

/*
 * _prop_extern_file_dirname --
 *	dirname(3), basically.  We have to roll our own because the
 *	system dirname(3) isn't reentrant.
 */
static void
_prop_extern_file_dirname(const char *path, char *result)
{
	const char *lastp;
	size_t len;

	/*
	 * If `path' is a NULL pointer or points to an empty string,
	 * return ".".
	 */
	if (path == NULL || *path == '\0') {
		goto singledot;
	}

	/* Strip trailing slashes, if any. */
	lastp = path + strlen(path) - 1;
	while (lastp != path && *lastp == '/') {
		lastp--;
	}

	/* Terminate path at the last occurrence of '/'. */
	do {
		if (*lastp == '/') {
			/* Strip trailing slashes, if any. */
			while (lastp != path && *lastp == '/') {
				lastp--;
			}

			/* ...and copy the result into the result buffer. */
			len = (lastp - path) + 1 /* last char */;
			if (len > (PATH_MAX - 1)) {
				len = PATH_MAX - 1;
			}

			memcpy(result, path, len);
			result[len] = '\0';
			return;
		}
	} while (--lastp >= path);

	/* No /'s found, return ".". */
 singledot:
	strcpy(result, ".");
}

/*
 * _prop_extern_write_file --
 *	Write an externalized object to the specified file.
 *	The file is written atomically from the caller's perspective,
 *	and the mode set to 0666 modified by the caller's umask.
 */
static bool
_prop_extern_write_file(const char *fname, const char *data, size_t len)
{
	char tname_store[PATH_MAX];
	char *tname = NULL;
	int fd = -1;
	int save_errno;
	mode_t myumask;
	bool rv = false;

	if (len > SSIZE_MAX) {
		errno = EFBIG;
		return false;
	}

	/*
	 * Get the directory name where the file is to be written
	 * and create the temporary file.
	 */
	_prop_extern_file_dirname(fname, tname_store);
	if (strlen(tname_store) + strlen(PLISTTMP) >= sizeof(tname_store)) {
		errno = ENAMETOOLONG;
		return false;
	}
	strcat(tname_store, PLISTTMP);

	if ((fd = mkstemp(tname_store)) == -1) {
		return false;
	}
	tname = tname_store;

	if (write(fd, data, len) != (ssize_t)len) {
		goto bad;
	}

	if (fsync(fd) == -1) {
		goto bad;
	}

	myumask = umask(0);
	(void)umask(myumask);
	if (fchmod(fd, 0666 & ~myumask) == -1) {
		goto bad;
	}

	if (rename(tname, fname) == -1) {
		goto bad;
	}
	tname = NULL;

	rv = true;

 bad:
	save_errno = errno;
	if (fd != -1) {
		(void) close(fd);
	}
	if (tname != NULL) {
		(void) unlink(tname);
	}
	errno = save_errno;
	return rv;
}

/*
 * _prop_object_externalize_to_file --
 *	Externalize an object to the specified file.
 */
bool
_prop_object_externalize_to_file(struct _prop_object *obj, const char *fname,
    prop_format_t fmt)
{
	bool rv;
	int save_errno;
	char *data = _prop_object_externalize(obj, fmt);
	if (data == NULL) {
		return false;
	}
	rv = _prop_extern_write_file(fname, data, strlen(data));
	save_errno = errno;
	_PROP_FREE(data, M_TEMP);
	errno = save_errno;

	return rv;
}

/*
 * prop_object_externalize_to_file --
 *	Externalize an object to the specified file in the default format.
 */
_PROP_EXPORT bool
prop_object_externalize_to_file(prop_object_t po, const char *fname)
{
	return _prop_object_externalize_to_file((struct _prop_object *)po,
	    fname, _prop_format_default);
}

/*
 * prop_object_externalize_to_file_with_format --
 *	Externalize an object to the specified file in the specified format.
 */
_PROP_EXPORT bool
prop_object_externalize_to_file_with_format(prop_object_t po,
    const char *fname, prop_format_t fmt)
{
	return _prop_object_externalize_to_file((struct _prop_object *)po,
	    fname, fmt);
}

/*
 * prop_object_externalize --
 *	Externalize an object in the default format.
 */
_PROP_EXPORT char *
prop_object_externalize(prop_object_t po)
{
	return _prop_object_externalize((struct _prop_object *)po,
	    _prop_format_default);
}

/*
 * prop_object_externalize_with_format --
 *	Externalize an object in the specified format.
 */
_PROP_EXPORT char *
prop_object_externalize_with_format(prop_object_t po, prop_format_t fmt)
{
	return _prop_object_externalize((struct _prop_object *)po, fmt);
}
