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
#include <string.h>
#include <errno.h>

#include <zlib.h>
#include <xbps_api.h>
#include "xbps_api_impl.h"

/**
 * @file lib/plist.c
 * @brief PropertyList generic routines
 * @defgroup plist PropertyList generic functions
 *
 * These functions manipulate plist files and objects shared by almost
 * all library functions.
 */
bool
xbps_add_obj_to_dict(prop_dictionary_t dict, prop_object_t obj,
		       const char *key)
{
	assert(dict != NULL);
	assert(obj != NULL);
	assert(key != NULL);

	if (!prop_dictionary_set(dict, key, obj)) {
		prop_object_release(dict);
		errno = EINVAL;
		return false;
	}

	prop_object_release(obj);
	return true;
}

bool
xbps_add_obj_to_array(prop_array_t array, prop_object_t obj)
{
	assert(array != NULL);
	assert(obj != NULL);

	if (!prop_array_add(array, obj)) {
		prop_object_release(array);
		errno = EINVAL;
		return false;
	}

	prop_object_release(obj);
	return true;
}

int
xbps_callback_array_iter_in_dict(prop_dictionary_t dict, const char *key,
				 int (*fn)(prop_object_t, void *, bool *),
				 void *arg)
{
	prop_object_iterator_t iter;
	prop_object_t obj;
	int rv = 0;
	bool cbloop_done = false;

	assert(dict != NULL);
	assert(key != NULL);
	assert(fn != NULL);

	iter = xbps_get_array_iter_from_dict(dict, key);
	if (iter == NULL)
		return EINVAL;

	while ((obj = prop_object_iterator_next(iter))) {
		rv = (*fn)(obj, arg, &cbloop_done);
		if (rv != 0 || cbloop_done)
			break;
	}

	prop_object_iterator_release(iter);

	return rv;
}

int
xbps_callback_array_iter_reverse_in_dict(prop_dictionary_t dict,
    const char *key, int (*fn)(prop_object_t, void *, bool *), void *arg)
{
	prop_array_t array;
	prop_object_t obj;
	int rv = 0;
	bool cbloop_done = false;
	unsigned int cnt = 0;

	assert(dict != NULL);
	assert(key != NULL);
	assert(fn != NULL);

	array = prop_dictionary_get(dict, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		xbps_dbg_printf("invalid key '%s' for dictionary", key);
		return EINVAL;
	}

	if ((cnt = prop_array_count(array)) == 0)
		return 0;

	while (cnt--) {
		obj = prop_array_get(array, cnt);
		rv = (*fn)(obj, arg, &cbloop_done);
		if (rv != 0 || cbloop_done)
			break;
	}

	return rv;
}

prop_dictionary_t
xbps_find_pkg_from_plist(const char *plist, const char *pkgname)
{
	prop_dictionary_t dict, obj, res;

	assert(plist != NULL);
	assert(pkgname != NULL);

	dict = prop_dictionary_internalize_from_zfile(plist);
	if (dict == NULL) {
		xbps_dbg_printf("cannot internalize %s for pkg %s: %s",
		    plist, pkgname, strerror(errno));
		return NULL;
	}

	obj = xbps_find_pkg_in_dict_by_name(dict, "packages", pkgname);
	if (obj == NULL) {
		prop_object_release(dict);
		return NULL;
	}

	res = prop_dictionary_copy(obj);
	prop_object_release(dict);

	return res;
}

prop_dictionary_t
xbps_find_pkg_dict_installed(const char *str, bool bypattern)
{
	prop_dictionary_t d, pkgd, rpkgd = NULL;
	pkg_state_t state = 0;

	if ((d = xbps_regpkgdb_dictionary_get()) == NULL)
		return NULL;

	if (bypattern)
		pkgd = xbps_find_pkg_in_dict_by_pattern(d, "packages", str);
	else
		pkgd = xbps_find_pkg_in_dict_by_name(d, "packages", str);
	if (pkgd == NULL)
		goto out;

	if (xbps_get_pkg_state_dictionary(pkgd, &state) != 0)
		goto out;

	switch (state) {
	case XBPS_PKG_STATE_INSTALLED:
	case XBPS_PKG_STATE_UNPACKED:
		rpkgd = prop_dictionary_copy(pkgd);
		break;
	case XBPS_PKG_STATE_CONFIG_FILES:
		errno = ENOENT;
		xbps_dbg_printf("'%s' installed but its state is "
		    "config-files\n",str);
		break;
	default:
		break;
	}
out:
	xbps_regpkgdb_dictionary_release();
	return rpkgd;
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_name(prop_dictionary_t dict,
			      const char *key,
			      const char *pkgname)
{
	prop_object_iterator_t iter;
	prop_object_t obj = NULL;
	const char *dpkgn;

	assert(dict != NULL);
	assert(pkgname != NULL);
	assert(key != NULL);

	if ((iter = xbps_get_array_iter_from_dict(dict, key)) == NULL)
		return NULL;

	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &dpkgn);
		if (strcmp(dpkgn, pkgname) == 0)
			break;
	}
	prop_object_iterator_release(iter);
	if (obj == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return obj;
}

prop_dictionary_t
xbps_find_pkg_in_dict_by_pattern(prop_dictionary_t dict,
				 const char *key,
				 const char *pattern)
{
	prop_object_iterator_t iter;
	prop_object_t obj = NULL;
	const char *pkgver;

	assert(dict != NULL);
	assert(key != NULL);
	assert(pattern != NULL);

	if ((iter = xbps_get_array_iter_from_dict(dict, key)) == NULL)
		return NULL;

	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		if (xbps_pkgpattern_match(pkgver, __UNCONST(pattern)))
			break;
	}
	prop_object_iterator_release(iter);
	if (obj == NULL) {
		errno = ENOENT;
		return NULL;
	}

	return obj;
}

bool
xbps_find_string_in_array(prop_array_t array, const char *val)
{
	prop_object_iterator_t iter;
	prop_object_t obj;

	assert(array != NULL);
	assert(val != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return false;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (prop_object_type(obj) != PROP_TYPE_STRING)
			continue;
		if (prop_string_equals_cstring(obj, val)) {
			prop_object_iterator_release(iter);
			return true;
		}
	}

	prop_object_iterator_release(iter);

	return false;
}

prop_object_iterator_t
xbps_get_array_iter_from_dict(prop_dictionary_t dict, const char *key)
{
	prop_array_t array;

	assert(dict != NULL);
	assert(key != NULL);

	array = prop_dictionary_get(dict, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		errno = EINVAL;
		return NULL;
	}

	return prop_array_iterator(array);
}

prop_dictionary_t
xbps_get_pkg_dict_from_metadata_plist(const char *pkgn, const char *plist)
{
	prop_dictionary_t plistd = NULL;
	char *plistf;

	assert(pkgn != NULL);
	assert(plist != NULL);

	plistf = xbps_xasprintf("%s/%s/metadata/%s/%s",
	    xbps_get_rootdir(), XBPS_META_PATH, pkgn, plist);
	if (plistf == NULL)
		return NULL;

	plistd = prop_dictionary_internalize_from_zfile(plistf);
	free(plistf);
	if (plistd == NULL) {
		xbps_dbg_printf("cannot read from plist file %s for %s: %s\n",
		    plist, pkgn, strerror(errno));
		return NULL;
	}

	return plistd;
}

bool
xbps_remove_string_from_array(prop_array_t array, const char *str)
{
	prop_object_t obj;
	prop_object_iterator_t iter;
	unsigned int idx = 0;
	bool found = false;

	assert(array != NULL);
	assert(str != NULL);

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return false;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		if (prop_string_equals_cstring(obj, str)) {
			found = true;
			break;
		}
		idx++;
	}
	prop_object_iterator_release(iter);
	if (!found) {
		errno = ENOENT;
		return false;
	}

	prop_array_remove(array, idx);

	return true;
}

bool
xbps_remove_pkg_from_dict(prop_dictionary_t dict,
			  const char *key,
			  const char *pkgname)
{
	prop_array_t array;
	prop_object_t obj;
	prop_object_iterator_t iter;
	const char *curpkgname;
	unsigned int i = 0;
	bool found = false;

	assert(dict != NULL);
	assert(key != NULL);
	assert(pkgname != NULL);

	array = prop_dictionary_get(dict, key);
	if (prop_object_type(array) != PROP_TYPE_ARRAY) {
		errno = EINVAL;
		xbps_dbg_printf("invalid array type for key: %s pkgname: %s\n",
		    key, pkgname);
		return false;
	}

	iter = prop_array_iterator(array);
	if (iter == NULL)
		return false;

	/* Iterate over the array of dictionaries to find its index. */
	while ((obj = prop_object_iterator_next(iter))) {
		prop_dictionary_get_cstring_nocopy(obj, "pkgname", &curpkgname);
		if ((curpkgname && (strcmp(curpkgname, pkgname) == 0))) {
			found = true;
			break;
		}
		i++;
	}
	prop_object_iterator_release(iter);
	if (found) {
		prop_array_remove(array, i);
		return true;
	}

	errno = ENOENT;

	return false;
}

bool
xbps_remove_pkg_dict_from_file(const char *pkg, const char *plist)
{
	prop_dictionary_t pdict;

	assert(pkg != NULL);
	assert(plist != NULL);

	pdict = prop_dictionary_internalize_from_zfile(plist);
	if (pdict == NULL) {
		xbps_dbg_printf("'%s' cannot read from file %s: %s\n",
		    pkg, plist, strerror(errno));
		return false;
	}

	if (!xbps_remove_pkg_from_dict(pdict, "packages", pkg)) {
		prop_object_release(pdict);
		return false;
	}

	if (!prop_dictionary_externalize_to_zfile(pdict, plist)) {
		xbps_dbg_printf("'%s' cannot write plist file %s: %s\n",
		    pkg, plist, strerror(errno));
		prop_object_release(pdict);
		return false;
	}

	prop_object_release(pdict);

	return true;
}

/*
 * Takes a compressed data buffer, decompresses it and returns the
 * new buffer uncompressed if all was right.
 */
#define _READ_CHUNK	512

static char *
_xbps_uncompress_plist_data(char *xml, size_t len)
{
	z_stream strm;
	unsigned char out[_READ_CHUNK];
	char *uncomp_xml = NULL;
	size_t have;
	ssize_t totalsize = 0;
	int rv = 0;

	/* Decompress the mmap'ed buffer with zlib */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;

	/* 15+16 to use gzip method */
	if (inflateInit2(&strm, 15+16) != Z_OK)
		return NULL;

	strm.avail_in = len;
	strm.next_in = (unsigned char *)xml;

	/* Output buffer (uncompressed) */
	uncomp_xml = malloc(_READ_CHUNK);
	if (uncomp_xml == NULL) {
		(void)inflateEnd(&strm);
		return NULL;
	}

	/* Inflate the input buffer and copy into 'uncomp_xml' */
	do {
		strm.avail_out = _READ_CHUNK;
		strm.next_out = out;
		rv = inflate(&strm, Z_NO_FLUSH);
		switch (rv) {
		case Z_DATA_ERROR:
			/*
			 * Wrong compressed data or uncompressed, try
			 * normal method as last resort.
			 */
			(void)inflateEnd(&strm);
			free(uncomp_xml);
			errno = EAGAIN;
			return NULL;
		case Z_STREAM_ERROR:
		case Z_NEED_DICT:
		case Z_MEM_ERROR:
			(void)inflateEnd(&strm);
			free(uncomp_xml);
			return NULL;
		}
		have = _READ_CHUNK - strm.avail_out;
		totalsize += have;
		uncomp_xml = realloc(uncomp_xml, totalsize);
		memcpy(uncomp_xml + totalsize - have, out, have);
	} while (strm.avail_out == 0);

	/* we are done */
	(void)inflateEnd(&strm);

	return uncomp_xml;
}
#undef _READ_CHUNK

prop_dictionary_t HIDDEN
xbps_read_dict_from_archive_entry(struct archive *ar,
				  struct archive_entry *entry)
{
	prop_dictionary_t d = NULL;
	size_t buflen = 0;
	ssize_t nbytes = -1;
	char *buf, *uncomp_buf;

	assert(ar != NULL);
	assert(entry != NULL);

	buflen = (size_t)archive_entry_size(entry);
	buf = malloc(buflen);
	if (buf == NULL)
		return NULL;

	nbytes = archive_read_data(ar, buf, buflen);
	if ((size_t)nbytes != buflen) {
		free(buf);
		return NULL;
	}

	uncomp_buf = _xbps_uncompress_plist_data(buf, buflen);
	if (uncomp_buf == NULL) {
		if (errno && errno != EAGAIN) {
			/* Error while decompressing */
			free(buf);
			return NULL;
		} else if (errno == EAGAIN) {
			/* Not a compressed data, try again */
			errno = 0;
			d = prop_dictionary_internalize(buf);
			free(buf);
		}
	} else {
		/* We have the uncompressed data */
		d = prop_dictionary_internalize(uncomp_buf);
		free(uncomp_buf);
		free(buf);
	}

	if (d == NULL)
		return NULL;
	else if (prop_object_type(d) != PROP_TYPE_DICTIONARY) {
		prop_object_release(d);
		return NULL;
	}

	return d;
}
