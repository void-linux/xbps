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
#include <sys/wait.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <spawn.h>

#include "xbps_api_impl.h"

static int
pfcexec(const char *file, const char **argv)
{
	pid_t			child;
	int			status, rv;

	if (((getuid() == 0) && (access("bin/sh", X_OK) == 0))) {
		if ((chroot(".") != 0) && (chdir("/") != 0))
			return errno;
	}
	if ((rv = posix_spawn(&child, file, NULL, NULL,
			      (char ** const)__UNCONST(argv),
			      NULL)) == 0) {
		while (waitpid(child, &status, 0) < 0) {
			if (errno != EINTR)
				return errno;
		}
		if (!WIFEXITED(status))
			return errno;

		return WEXITSTATUS(status);
	}
	return rv;
}

static int
vfcexec(const char *arg, va_list ap)
{
	const char **argv;
	size_t argv_size, argc;
	int retval;

	argv_size = 16;
	if ((argv = malloc(argv_size * sizeof(*argv))) == NULL) {
		errno = ENOMEM;
		return -1;
	}
	argv[0] = arg;
	argc = 1;

	do {
		if (argc == argv_size) {
			argv_size *= 2;
			argv = realloc(argv, argv_size * sizeof(*argv));
			if (argv == NULL) {
				errno = ENOMEM;
				return -1;
			}
		}
		arg = va_arg(ap, const char *);
		argv[argc++] = arg;

	} while (arg != NULL);

	retval = pfcexec(argv[0], argv);
	free(argv);

	return retval;
}

int HIDDEN
xbps_file_exec(const char *arg, ...)
{
	va_list	ap;
	int	result;

	va_start(ap, arg);
	result = vfcexec(arg, ap);
	va_end(ap);

	return result;
}
