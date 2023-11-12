/*-
 * Copyright (c) 2023 classabbyamp.
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

#include "compat.h"

void HIDDEN
strmode(mode_t mode, char *buf)
{
	switch (mode & S_IFMT) {
	/* many of these are not currently packageable, but why not keep them for future compat? */
	case S_IFSOCK: *buf++ = 's'; break;
	case S_IFLNK: *buf++ = 'l'; break;
	case S_IFREG: *buf++ = '-'; break;
	case S_IFBLK: *buf++ = 'b'; break;
	case S_IFDIR: *buf++ = 'd'; break;
	case S_IFCHR: *buf++ = 'c'; break;
	case S_IFIFO: *buf++ = 'p'; break;
#ifdef S_IFWHT
	case S_IFWHT: *buf++ = 'w'; break;
#endif
	default: *buf++ = '?'; break;
	}
	*buf++ = (mode & S_IRUSR) ? 'r' : '-';
	*buf++ = (mode & S_IWUSR) ? 'w' : '-';
	switch (mode & (S_IXUSR | S_ISUID)) {
	case (S_IXUSR | S_ISUID): *buf++ = 's'; break;
	case S_ISUID: *buf++ = 'S'; break;
	case S_IXUSR: *buf++ = 'x'; break;
	default: *buf++ = '-'; break;
	}

	*buf++ = (mode & S_IRGRP) ? 'r' : '-';
	*buf++ = (mode & S_IWGRP) ? 'w' : '-';
	switch (mode & (S_IXGRP | S_ISGID)) {
	case S_IXGRP | S_ISGID: *buf++ = 's'; break;
	case S_ISUID: *buf++ = 'S'; break;
	case S_IXGRP: *buf++ = 'x'; break;
	default: *buf++ = '-'; break;
	}

	*buf++ = (mode & S_IROTH) ? 'r' : '-';
	*buf++ = (mode & S_IWOTH) ? 'w' : '-';
	switch (mode & (S_IXOTH | S_ISVTX)) {
	case S_IXOTH | S_ISVTX: *buf++ = 't'; break;
	case S_ISVTX: *buf++ = 'T'; break;
	case S_IXOTH: *buf++ = 'x'; break;
	default: *buf++ = '-'; break;
	}

	*buf = '\0';
}
