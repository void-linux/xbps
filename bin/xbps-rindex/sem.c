/*-
 * Copyright (c) 2014 Juan Romero Pardines.
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "defs.h"

struct idxlock *
index_lock(struct xbps_handle *xhp)
{
	struct idxlock *il;

	if ((il = malloc(sizeof(struct idxlock))) == NULL)
		return NULL;

	/*
	 * Generate semaphore name for target architecture.
	 */
	il->semname = xbps_xasprintf("/xbps-rindex-%s",
			xhp->target_arch ? xhp->target_arch : xhp->native_arch);
	/*
	 * Create/open the POSIX named semaphore.
	 */
	il->sem = sem_open(il->semname, O_CREAT, 0660, 1);
	if (il->sem == SEM_FAILED) {
		fprintf(stderr, "%s: failed to create/open named "
		    "semaphore: %s\n", _XBPS_RINDEX, strerror(errno));
		return NULL;
	}
	if (sem_wait(il->sem) == -1) {
		fprintf(stderr, "%s: failed to lock named semaphore: %s\n",
		    _XBPS_RINDEX, strerror(errno));
		return NULL;
	}

	return il;
}

void
index_unlock(struct idxlock *il)
{
	/* Unlock semaphore, close and destroy it (if possible) */
	sem_post(il->sem);
	sem_close(il->sem);
	sem_unlink(il->semname);
	free(il->semname);
	free(il);
}
