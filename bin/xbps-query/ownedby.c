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
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xbps.h>


struct thread_data {
	pthread_t        this_thread;
	struct archive*  ar;
	bool             byhash;
	const char*      expr;
	const regex_t*   regex;
	pthread_mutex_t* archive_mutex;
	pthread_mutex_t* print_mutex;
};

static inline int parse_line(char** fields, int fields_size, char* line) {
	char* next;
	int   i = 0;

	while (i < fields_size - 1) {
		if (!(next = strchr(line, '%')))
			break;

		*next = '\0';

		fields[i++] = *line ? line : NULL;
		line = next + 1;
	}
	fields[i++] = *line ? line : NULL;

	return i;
}

static inline void print_line(pthread_mutex_t* mutex, const char* pkg, char** file) {
	if (mutex) pthread_mutex_lock(mutex);
	printf("%s: %s", pkg, file[1]);
	if (file[2])
		printf(" -> %s", file[2]);
	printf("\n");
	if (mutex) pthread_mutex_unlock(mutex);
}

static char* archive_get_file(struct archive* ar, struct archive_entry* entry) {
	ssize_t buflen;
	char*   buf;

	assert(ar != NULL);
	assert(entry != NULL);

	buflen = archive_entry_size(entry);
	buf = malloc(buflen + 1);
	if (buf == NULL)
		return NULL;
	if (archive_read_data(ar, buf, buflen) != buflen) {
		free(buf);
		return NULL;
	}
	buf[buflen] = '\0';
	return buf;
}

static void* thread_func(void* data_ptr) {
	struct thread_data*   data = data_ptr;
	struct archive_entry* entry;

	for (;;) {
		char* pkg;
		char* content;
		char* line, *end_line;
		char* file[3];
		int   ar_rv;

		if (data->archive_mutex) pthread_mutex_lock(data->archive_mutex);
		if ((ar_rv = archive_read_next_header(data->ar, &entry)) == ARCHIVE_OK) {
			pkg = strdup(archive_entry_pathname(entry));
			content = archive_get_file(data->ar, entry);
		}
		if (data->archive_mutex) pthread_mutex_unlock(data->archive_mutex);

		if (ar_rv != ARCHIVE_OK)
			break;

		line = strtok_r(content, "\n", &end_line);
		while (line) {
			parse_line(file, 3, line);
			if (data->byhash) {
				if (file[0] != NULL && strcasecmp(file[0], data->expr) == 0)
					print_line(data->print_mutex, pkg, file);
			} else if (data->expr != NULL) {
				if (strcasestr(file[1], data->expr) != NULL || (file[2] && strcasestr(file[2], data->expr))) {
					print_line(data->print_mutex, pkg, file);
				}
			} else {    // regex
				if (!regexec(data->regex, file[1], 0, NULL, 0) ||
				    (file[2] && !regexec(data->regex, file[2], 0, NULL, 0))) {
					print_line(data->print_mutex, pkg, file);
				}
			}
			line = strtok_r(NULL, "\n", &end_line);
		}

		free(pkg);
		free(content);
	}

	return NULL;
}

static int repo_search_files(struct xbps_handle* xh, const char* repouri, bool byhash, const char* expr, regex_t* regex) {
	struct archive*     ar;
	FILE*               ar_file;
	char*               files_uri;
	char*               reponame_escaped;
	pthread_mutex_t     archive_mutex, print_mutex;
	int                 maxthreads;
	struct thread_data* thds;

	if (!xbps_repository_is_remote(repouri)) {
		files_uri = xbps_xasprintf("%s/%s-files", repouri, xh->target_arch ? xh->target_arch : xh->native_arch);
	} else {
		if (!(reponame_escaped = xbps_get_remote_repo_string(repouri)))
			return false;
		files_uri = xbps_xasprintf("%s/%s/%s-files", xh->metadir, reponame_escaped, xh->target_arch ? xh->target_arch : xh->native_arch);
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

	if (archive_read_open_FILE(ar, ar_file) != ARCHIVE_OK) {
		xbps_dbg_printf("[repo] `%s' failed to open repodata archive %s\n", files_uri, archive_error_string(ar));
		archive_read_close(ar);
		fclose(ar_file);
		return false;
	}

	maxthreads = (int) sysconf(_SC_NPROCESSORS_ONLN);
	if (maxthreads <= 1) {
		struct thread_data thd = {
			.ar = ar,
			.byhash = byhash,
			.expr = expr,
			.regex = regex,
			.archive_mutex = NULL,
			.print_mutex = NULL
		};

		thread_func(&thd);
	} else {
		thds = calloc(maxthreads, sizeof(*thds));
		assert(thds);

		pthread_mutex_init(&archive_mutex, NULL);
		pthread_mutex_init(&print_mutex, NULL);

		for (int i = 0; i < maxthreads; i++) {
			int rv;

			thds[i].ar = ar;
			thds[i].byhash = byhash;
			thds[i].expr = expr;
			thds[i].regex = regex;
			thds[i].archive_mutex = &archive_mutex;
			thds[i].print_mutex = &print_mutex;

			if ((rv = pthread_create(&thds[i].this_thread, NULL, thread_func, &thds[i])) != 0) {
				xbps_error_printf("unable to create thread: %s, retrying...\n", strerror(rv));
				i--;
			}
		}

		for (int i = 0; i < maxthreads; i++) {
			pthread_join(thds[i].this_thread, NULL);
		}
	}

	fclose(ar_file);
	free(files_uri);
	return 0;
}

int ownedby(struct xbps_handle* xh, const char* file, bool repo_mode UNUSED, bool useregex) {
	const char* expr = NULL;
	regex_t     regex;

	if (useregex) {
		if (regcomp(&regex, file, REG_EXTENDED | REG_NOSUB | REG_ICASE) != 0) {
			xbps_error_printf("xbps-locate: invalid regular expression\n");
			exit(1);
		}
	} else {
		expr = file;
	}

	for (unsigned int i = 0; i < xbps_array_count(xh->repositories); i++) {
		const char* repo_uri;
		xbps_array_get_cstring_nocopy(xh->repositories, i, &repo_uri);
		repo_search_files(xh, repo_uri, false, expr, &regex);
	}

	if (useregex)
		regfree(&regex);

	return 0;
}

int ownedhash(struct xbps_handle* xh, const char* file, bool repo_mode UNUSED, bool regex UNUSED) {
	const char* expr;
	struct stat fstat;
	char        hash[XBPS_SHA256_SIZE];

	if (stat(file, &fstat) != -1) {
		if (!S_ISREG(fstat.st_mode)) {
			xbps_error_printf("query: `%s' exist but is not a regular file\n", file);
			return 1;
		}
		if (!xbps_file_sha256(hash, sizeof(hash), file)) {
			xbps_error_printf("query: cannot get hash of `%s': %s\n", file, strerror(errno));
			return 1;
		}
		expr = hash;
	} else {
		expr = file;
	}

	for (unsigned int i = 0; i < xbps_array_count(xh->repositories); i++) {
		const char* repo_uri;
		xbps_array_get_cstring_nocopy(xh->repositories, i, &repo_uri);
		repo_search_files(xh, repo_uri, true, expr, NULL);
	}

	return 0;
}
