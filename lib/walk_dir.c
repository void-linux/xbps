/*-
 * Copyright (c) 2021 Érico Nogueira Rolim.
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
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

#include <xbps.h>
#include "xbps_utils.h"

int
xbps_walk_dir(const char *path,
		int (*fn) (const char *, const struct stat *, const struct dirent *))
{
	int rv, i, count;
	struct dirent **list;
	char tmp_path[PATH_MAX] = { 0 };
	struct stat sb;

	count = scandir(path, &list, NULL, alphasort);
	for (i = count - 1; i >= 0; i--) {
		if (strcmp(list[i]->d_name, ".") == 0 || strcmp(list[i]->d_name, "..") == 0)
			goto cleanup;
		if (strlen(path) + strlen(list[i]->d_name) + 1 >= PATH_MAX - 1) {
			errno = ENAMETOOLONG;
			rv = -1;
			break;
		}
		strncpy(tmp_path, path, PATH_MAX - 1);
		strncat(tmp_path, "/", PATH_MAX - 1 - strlen(tmp_path));
		strncat(tmp_path, list[i]->d_name, PATH_MAX - 1 - strlen(tmp_path));
		if (lstat(tmp_path, &sb) < 0) {
			break;
		}

		if (S_ISDIR(sb.st_mode)) {
			if (xbps_walk_dir(tmp_path, fn) < 0) {
				rv = -1;
				break;
			}
		}

		rv = fn(tmp_path, &sb, list[i]);
		if (rv != 0) {
			break;

		cleanup:
			free(list[i]);
		}

	}
	for (; i >= 0; i--) {
		free(list[i]);
	}
	free(list);
	return rv;
}
