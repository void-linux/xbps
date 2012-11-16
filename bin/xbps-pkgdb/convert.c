/*-
 * Copyright (c) 2012 Juan Romero Pardines.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

static prop_data_t
create_script_blob(struct xbps_handle *xhp,
		   const char *file,
		   const char *pkgname)
{
	prop_data_t data;
	struct stat st;
	void *mf;
	char *buf;
	int fd;

	buf = xbps_xasprintf("%s/metadata/%s/%s", xhp->metadir, pkgname, file);
	if ((fd = open(buf, O_RDONLY)) == -1) {
		free(buf);
		if (errno != ENOENT)
			fprintf(stderr, "%s: can't read INSTALL script: %s\n",
		    	    pkgname, strerror(errno));

		return NULL;
	}
	if (stat(buf, &st) == -1) {
		free(buf);
		fprintf(stderr, "%s: failed to stat %s script: %s\n",
		    pkgname, file, strerror(errno));
		return NULL;
	}
	free(buf);

	mf = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	if (mf == MAP_FAILED) {
		close(fd);
		fprintf(stderr, "%s: failed to map INSTALL script: %s\n",
		    pkgname, strerror(errno));
		return NULL;
	}
	data = prop_data_create_data(mf, st.st_size);
	munmap(mf, st.st_size);

	return data;
}

/*
 * Converts package metadata format to 0.18.
 */
int
convert_pkgd_metadir(struct xbps_handle *xhp, prop_dictionary_t pkgd)
{
	prop_dictionary_t filesd, propsd;
	prop_array_t array;
	prop_data_t data;
	const char *pkgname;
	char *buf, *sha256, *propsf, *filesf;

	prop_dictionary_get_cstring_nocopy(pkgd, "pkgname", &pkgname);

	/* Merge XBPS_PKGFILES */
	propsf = xbps_xasprintf("%s/metadata/%s/%s", xhp->metadir,
			pkgname, XBPS_PKGPROPS);
	propsd = prop_dictionary_internalize_from_zfile(propsf);
	assert(propsd);

	filesf = xbps_xasprintf("%s/metadata/%s/%s", xhp->metadir,
			pkgname, XBPS_PKGFILES);
	filesd = prop_dictionary_internalize_from_zfile(filesf);
	assert(filesd);

	array = prop_dictionary_get(filesd, "files");
	if (array && prop_array_count(array))
		prop_dictionary_set(propsd, "files", array);

	array = prop_dictionary_get(filesd, "conf_files");
	if (array && prop_array_count(array))
		prop_dictionary_set(propsd, "conf_files", array);

	array = prop_dictionary_get(filesd, "dirs");
	if (array && prop_array_count(array))
		prop_dictionary_set(propsd, "dirs", array);

	array = prop_dictionary_get(filesd, "links");
	if (array && prop_array_count(array))
		prop_dictionary_set(propsd, "links", array);

	prop_object_release(filesd);

	/* Merge INSTALL script */
	if ((data = create_script_blob(xhp, "INSTALL", pkgname))) {
		prop_dictionary_set(propsd, "install-script", data);
		prop_object_release(data);
	}
	/* Merge REMOVE script */
	if ((data = create_script_blob(xhp, "REMOVE", pkgname))) {
		prop_dictionary_set(propsd, "remove-script", data);
		prop_object_release(data);
	}
	/* Externalize pkg metaplist */
	buf = xbps_xasprintf("%s/.%s.plist", xhp->metadir, pkgname);
	if (!prop_dictionary_externalize_to_file(propsd, buf)) {
		fprintf(stderr, "%s: can't externalize plist: %s\n",
		    pkgname, strerror(errno));
		return -1;
	}
	/* create sha256 hash for pkg plist */
	sha256 = xbps_file_hash(buf);
	free(buf);
	assert(sha256);
	prop_dictionary_set_cstring(pkgd, "metafile-sha256", sha256);
	free(sha256);

	/* Remove old files/dir */
	if ((remove(propsf) == -1) || (remove(filesf) == -1))
		fprintf(stderr, "%s: failed to remove %s: %s\n",
		    pkgname, propsf, strerror(errno));

	buf = xbps_xasprintf("%s/metadata/%s/INSTALL", xhp->metadir, pkgname);
	if (access(buf, R_OK) == 0)
		remove(buf);
	free(buf);

	buf = xbps_xasprintf("%s/metadata/%s/REMOVE", xhp->metadir, pkgname);
	if (access(buf, R_OK) == 0)
		remove(buf);
	free(buf);

	buf = xbps_xasprintf("%s/metadata/%s", xhp->metadir, pkgname);
	remove(buf);
	free(buf);

	buf = xbps_xasprintf("%s/metadata", xhp->metadir);
	remove(buf);
	free(buf);

	return 0;
}
