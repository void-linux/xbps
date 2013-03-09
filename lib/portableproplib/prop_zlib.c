/*-
 * Copyright (c) 2010-2012 Juan Romero Pardines.
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

#include <prop/proplib.h>
#include "prop_object_impl.h"

#include <errno.h>
#include <zlib.h>

#define _READ_CHUNK	8192

#define TEMPLATE(type, objtype)								\
bool											\
prop ## type ## _externalize_to_zfile(prop ## type ## _t obj, const char *fname)	\
{											\
	char *xml;									\
	bool rv;									\
	int save_errno = 0;								\
											\
	xml = prop ## type ## _externalize(obj);					\
	if (xml == NULL)								\
		return false;								\
	rv = _prop_object_externalize_write_file(fname, xml, strlen(xml), true);	\
	if (rv == false)								\
		save_errno = errno;							\
	_PROP_FREE(xml, M_TEMP);							\
	if (rv == false)								\
		errno = save_errno;							\
											\
	return rv;									\
}											\
											\
prop ## type ## _t									\
prop ## type ## _internalize_from_zfile(const char *fname)				\
{											\
	struct _prop_object_internalize_mapped_file *mf;				\
	prop ## type ## _t obj = NULL;							\
	z_stream strm;									\
	unsigned char *out;								\
	char *uncomp_xml = NULL;							\
	size_t have;									\
	ssize_t totalsize = 0;								\
	int rv = 0;									\
											\
	mf = _prop_object_internalize_map_file(fname);					\
	if (mf == NULL)									\
		return NULL;								\
											\
	/* If it's an ordinary uncompressed plist we are done */			\
	obj = prop ## type ## _internalize(mf->poimf_xml);				\
	if (prop_object_type(obj) == PROP_TYPE_## objtype)				\
		goto out;								\
											\
	/* Output buffer (uncompressed) */						\
	uncomp_xml = _PROP_MALLOC(_READ_CHUNK, M_TEMP);					\
	if (uncomp_xml == NULL)								\
		goto out;								\
											\
	/* temporary output buffer for inflate */					\
	out = _PROP_MALLOC(_READ_CHUNK, M_TEMP);					\
	if (out == NULL) {								\
		_PROP_FREE(uncomp_xml, M_TEMP);						\
		goto out;								\
	}										\
											\
	/* Decompress the mmap'ed buffer with zlib */					\
	strm.zalloc = Z_NULL;								\
	strm.zfree = Z_NULL;								\
	strm.opaque = Z_NULL;								\
	strm.avail_in = 0;								\
	strm.next_in = Z_NULL;								\
											\
	/* 15+16 to use gzip method */							\
	if (inflateInit2(&strm, 15+16) != Z_OK)						\
		goto out2;								\
											\
	strm.avail_in = mf->poimf_mapsize;						\
	strm.next_in = (unsigned char *)mf->poimf_xml;					\
											\
	/* Inflate the input buffer and copy into 'uncomp_xml' */			\
	do {										\
		strm.avail_out = _READ_CHUNK;						\
		strm.next_out = out;							\
		rv = inflate(&strm, Z_NO_FLUSH);					\
		switch (rv) {								\
		case Z_DATA_ERROR:							\
		case Z_STREAM_ERROR:							\
		case Z_NEED_DICT:							\
		case Z_MEM_ERROR:							\
			errno = EINVAL;							\
			goto out1;							\
		}									\
		have = _READ_CHUNK - strm.avail_out;					\
		totalsize += have;							\
		uncomp_xml = _PROP_REALLOC(uncomp_xml, totalsize, M_TEMP);		\
		memcpy(uncomp_xml + totalsize - have, out, have);			\
	} while (strm.avail_out == 0);							\
											\
	/* we are done */								\
out2:											\
	(void)inflateEnd(&strm);							\
out1:											\
	obj = prop ## type ## _internalize(uncomp_xml);					\
	_PROP_FREE(out, M_TEMP);							\
	_PROP_FREE(uncomp_xml, M_TEMP);							\
out:											\
	_prop_object_internalize_unmap_file(mf);					\
											\
	return obj;									\
}

TEMPLATE(_array, ARRAY)
TEMPLATE(_dictionary, DICTIONARY)

#undef TEMPLATE
