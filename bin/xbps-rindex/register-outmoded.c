/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
 * Copyright (c) 2019 Duncan Overbruck <mail@duncano.de>.
 * Copyright (c) 2019 Piotr Wójcik.
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

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <xbps.h>
#include "defs.h"

static xbps_dictionary_t parse_outmoded(const char *path, int *rv) {
	FILE *fp;
	size_t len = 0;
	ssize_t nread;
	char *line = NULL;
	char *word;
	char *pkgname;
	xbps_dictionary_t outmoded = xbps_dictionary_create();

	if ((fp = fopen(path, "r")) == NULL) {
		*rv = errno;
		xbps_error_printf("cannot read outmoded list file %s: %s\n", path, strerror(*rv));
		return NULL;
	}

	while ((nread = getline(&line, &len, fp)) != -1) {
		xbps_dictionary_t entry = xbps_dictionary_create();
		xbps_array_t to_install = NULL;

		word = strtok(line, " \n\t");

		pkgname = xbps_pkgpattern_name(word);
		if (pkgname == NULL) {
			*rv = -1;
			return NULL;
		}

		xbps_dictionary_set_cstring(entry, "pattern", word);

		while ((word = strtok(NULL, " \n\t"))) {
			if (to_install == NULL) {
				to_install = xbps_array_create();
			}
			xbps_array_add_cstring(to_install, word);
		}

		if (to_install) {
			xbps_dictionary_set(entry, "to_install", to_install);
		}

		xbps_dictionary_set(outmoded, pkgname, entry);
	}

	if (errno) {
		*rv = errno;
		return NULL;
	}

	return outmoded;
}

int
register_outmoded(struct xbps_handle *xhp, const char *repodir, const char *source_path, const char *compression, const char *privkey)
{
	xbps_dictionary_t idx, idxmeta, outmoded;
	struct xbps_repo *repo = NULL;
	char *repopath = NULL, *rlockfname = NULL;
	int rv = 0, rlockfd = -1;

	assert(source_path);

	if (!xbps_repo_lock(xhp, repodir, &rlockfd, &rlockfname)) {
		fprintf(stderr, "xbps-rindex: cannot lock repository "
		    "%s: %s\n", repodir, strerror(errno));
		rv = -1;
		goto earlyout;
	}
	repo = xbps_repo_public_open(xhp, repodir);
	if (repo == NULL && errno != ENOENT) {
		fprintf(stderr, "xbps-rindex: cannot open/lock repository "
		    "%s: %s\n", repodir, strerror(errno));
		rv = -1;
		goto earlyout;
	}
	if (repo) {
		idx = xbps_dictionary_copy_mutable(repo->idx);
		idxmeta = xbps_dictionary_copy_mutable(repo->idxmeta);
	} else {
		idx = xbps_dictionary_create();
		idxmeta = xbps_dictionary_create();
	}
	outmoded = parse_outmoded(source_path, &rv);
	if (rv) {
		goto out;
	}
	xbps_dictionary_set(idxmeta, "outmoded", outmoded);
	if (!repodata_flush(xhp, repodir, "repodata", idx, idxmeta, compression, privkey)) {
		fprintf(stderr, "%s: failed to write repodata: %s\n",
				_XBPS_RINDEX, strerror(errno));
		rv = -1;
		goto out;
	}
	printf("index: outmoded %u packages.\n", xbps_dictionary_count(outmoded));

out:
	xbps_object_release(idx);
	if (idxmeta)
		xbps_object_release(idxmeta);

earlyout:
	if (repo)
		xbps_repo_close(repo);

	xbps_repo_unlock(rlockfd, rlockfname);
	free(repopath);

	return rv;
}
