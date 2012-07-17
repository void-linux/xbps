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
#include <fcntl.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <assert.h>

#include <xbps_api.h>
#include "defs.h"

int
acquire_repo_lock(const char *plist, char **plist_lock)
{
	int try = 0, fd = -1;

	*plist_lock = xbps_xasprintf("%s.lock", plist);
	assert(*plist_lock);

	fd = open(*plist_lock, O_RDWR);
	if (fd == -1) {
		if (errno == ENOENT) {
			fd = creat(*plist_lock, 0640);
			if (fd == -1) {
				fprintf(stderr, "Failed to create "
				    "repository file lock: %s\n",
				strerror(errno));
				return -1;
			}
		} else {
			fprintf(stderr, "Failed to open repository "
			     "file lock: %s\n", strerror(errno));
			return -1;
		}
	}
	/*
	 * Acquire the the exclusive file lock or wait until
	 * it's available.
	 */
#define WAIT_SECONDS 30
	while (lockf(fd, F_TLOCK, 0) < 0) {
		if (errno == EAGAIN || errno == EACCES) {
			if (++try < WAIT_SECONDS) {
				fprintf(stderr,"Repository index file "
				    "is busy! retrying in 5 sec...\n");
				sleep(5);
				continue;
			}
		}
		fprintf(stderr, "Failed to acquire repository "
		    "file lock in %d seconds!\n", WAIT_SECONDS);
		close(fd);
		return -1;
	}
	return fd;
}

void
release_repo_lock(char **plist_lock, int fd)
{
	assert(*plist_lock);

	if (fd == -1)
		return;
	if (lockf(fd, F_ULOCK, 0) == -1) {
		fprintf(stderr, "failed to unlock file lock: %s\n",
		     strerror(errno));
		close(fd);
		exit(EXIT_FAILURE);
	}
	close(fd);
	unlink(*plist_lock);
	free(*plist_lock);
}
