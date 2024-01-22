/*-
 * Copyright (c) 2010-2015 Juan Romero Pardines.
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

#include "defs.h"
#include "fetch.h"
#include "xbps_api_impl.h"

#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <openssl/sha.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xbps.h>


struct locate {
	bool        byhash;
	const char* expr;
	regex_t     regex;
};

struct file {
	char* hash;
	char* path;
	char* target;
};

static inline void parse_line(struct file* file, char* line) {
	char *field, *end_field;
	int   field_nr;

	field    = strtok_r(line, ";", &end_field);
	field_nr = 0;
	do {
		if (field[0] == '%' && field[1] == '\0')
			field = NULL;
		switch (field_nr++) {
			case 0:
				file->hash = field;
				break;
			case 1:
				file->path = field;
				break;
			case 2:
				file->target = field;
				break;
		}
	} while ((field = strtok_r(NULL, ";", &end_field)));
}

static inline void print_line(const char* pkg, struct file* file) {
	printf("%s: %s", pkg, file->path);
	if (file->target)
		printf(" -> %s", file->target);
}

static char* archive_get_file(struct archive* ar, struct archive_entry* entry) {
	ssize_t buflen;
	char*   buf;

	assert(ar != NULL);
	assert(entry != NULL);

	buflen = archive_entry_size(entry);
	buf    = malloc(buflen + 1);
	if (buf == NULL)
		return NULL;

	if (archive_read_data(ar, buf, buflen) != buflen) {
		free(buf);
		return NULL;
	}
	buf[buflen] = '\0';
	return buf;
}

static int repo_search_files(struct xbps_repo* repo, void* locate_ptr, bool* done) {
	struct locate*        locate = locate_ptr;
	struct archive*       ar;
	struct archive_entry* entry;
	FILE*                 ar_file;
	char*                 files_uri;
	char*                 reponame_escaped;

	(void) done;

	if (!xbps_repository_is_remote(repo->uri)) {
		files_uri = xbps_xasprintf("%s/%s-files", repo->uri, repo->xhp->target_arch ? repo->xhp->target_arch : repo->xhp->native_arch);
	} else {
		if (!(reponame_escaped = xbps_get_remote_repo_string(repo->uri)))
			return false;
		files_uri = xbps_xasprintf("%s/%s/%s-files", repo->xhp->metadir, reponame_escaped, repo->xhp->target_arch ? repo->xhp->target_arch : repo->xhp->native_arch);
		free(reponame_escaped);
	}

	if ((ar_file = fopen(files_uri, "r")) == NULL) {
		xbps_dbg_printf("[repo] `%s' failed to open file-data archive %s\n", files_uri, strerror(errno));
		return false;
	}

	ar = archive_read_new();
	assert(ar);

	archive_read_support_filter_gzip(ar);
	archive_read_support_filter_bzip2(ar);
	archive_read_support_filter_xz(ar);
	archive_read_support_filter_lz4(ar);
	archive_read_support_filter_zstd(ar);
	archive_read_support_format_tar(ar);

	if (archive_read_open_FILE(ar, ar_file) == ARCHIVE_FATAL) {
		xbps_dbg_printf("[repo] `%s' failed to open repodata archive %s\n", files_uri, archive_error_string(ar));
		archive_read_close(ar);
		fclose(ar_file);
		return false;
	}

	while (archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		const char* pkg     = archive_entry_pathname(entry);
		char*       content = archive_get_file(ar, entry);
		char *      line, *end_line;
		struct file file;

		line = strtok_r(content, "\n", &end_line);
		do {
			parse_line(&file, line);
			if (locate->byhash) {
				if (file.hash != NULL && strcasecmp(file.hash, locate->expr) == 0)
					print_line(pkg, &file);
			} else if (locate->expr != NULL) {
				if (strcasestr(file.path, locate->expr) != NULL || (file.target && strcasestr(file.target, locate->expr))) {
					print_line(pkg, &file);
				}
			} else {    // regex
				if (!regexec(&locate->regex, file.path, 0, NULL, 0) ||
				    (file.target && !regexec(&locate->regex, file.target, 0, NULL, 0))) {
					print_line(pkg, &file);
				}
			}
		} while ((line = strtok_r(NULL, "\n", &end_line)));

		free(content);
	}

	fclose(ar_file);
	free(files_uri);
	return 0;
}

int ownedby(struct xbps_handle* xh, const char* file, bool repo_mode UNUSED, bool regex) {
	struct locate locate;

	memset(&locate, 0, sizeof(locate));

	if (regex) {
		if (regcomp(&locate.regex, file, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) {
			xbps_error_printf("xbps-locate: invalid regular expression\n");
			exit(1);
		}
	} else {
		locate.expr = file;
	}

	xbps_rpool_foreach(xh, repo_search_files, &locate);

	if (regex)
		regfree(&locate.regex);

	return 0;
}

int ownedhash(struct xbps_handle* xh, const char* file, bool repo_mode UNUSED, bool regex UNUSED) {
	struct locate locate;
	struct stat   fstat;
	char          hash[XBPS_SHA256_SIZE];

	memset(&locate, 0, sizeof(locate));
	locate.byhash = true;

	if (stat(file, &fstat) != -1) {
		if (!S_ISREG(fstat.st_mode)) {
			xbps_error_printf("query: `%s' exist but is not a regular file\n", file);
			return 1;
		}
		if (!xbps_file_sha256(hash, sizeof(hash), file)) {
			xbps_error_printf("query: cannot get hash of `%s': %s\n", file, strerror(errno));
			return 1;
		}
		locate.expr = hash;
	} else {
		locate.expr = file;
	}

	xbps_rpool_foreach(xh, repo_search_files, &locate);

	return 0;
}
