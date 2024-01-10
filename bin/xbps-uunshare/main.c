/*-
 * Copyright (c) 2014-2015 Juan Romero Pardines.
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
#include <sys/mount.h>
#include <sys/fsuid.h>
#include <sys/wait.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <signal.h>
#include <fcntl.h>
#include <grp.h>
#include <errno.h>
#include <limits.h>
#include <syscall.h>
#include <assert.h>
#include <getopt.h>

#include <xbps.h>
#include "queue.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

struct bindmnt {
	SIMPLEQ_ENTRY(bindmnt) entries;
	char *src;
	const char *dest;
};

static int errval = 0;
static SIMPLEQ_HEAD(bindmnt_head, bindmnt) bindmnt_queue =
    SIMPLEQ_HEAD_INITIALIZER(bindmnt_queue);

static void __attribute__((noreturn))
die(const char *fmt, ...)
{
	va_list ap;
	int save_errno = errno;

	va_start(ap, fmt);
	fprintf(stderr, "ERROR ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, " (%s)\n", strerror(save_errno));
	va_end(ap);
	exit(errval != 0 ? errval : EXIT_FAILURE);
}

static void __attribute__((noreturn))
usage(const char *p, bool fail)
{
	printf("Usage: %s [OPTIONS] [--] <dir> <cmd> [<cmdargs>]\n\n"
	    "OPTIONS\n"
	    " -b, --bind-rw <src:dest>  Bind mounts <src> into <dir>/<dest> (read-write)\n"
	    " -h, --help                Show usage\n"
	    " -V, --version             Show XBPS version\n", p);
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void
add_bindmount(char *bm)
{
	struct bindmnt *bmnt;
	char *b, *src, *dest;
	size_t len;

	src = strdup(bm);
	assert(src);
	dest = strchr(bm, ':');
	if (dest == NULL || *dest == '\0') {
		errno = EINVAL;
		die("invalid argument for bindmount: %s", bm);
	}
	dest++;
	b = strchr(bm, ':');
	len = strlen(bm) - strlen(b);
	src[len] = '\0';

	bmnt = malloc(sizeof(struct bindmnt));
	assert(bmnt);

	bmnt->src = src;
	bmnt->dest = dest;
	SIMPLEQ_INSERT_TAIL(&bindmnt_queue, bmnt, entries);
}

static void
bindmount(const char *chrootdir, const char *dir, const char *dest)
{
	char mountdir[PATH_MAX-1];

	snprintf(mountdir, sizeof(mountdir), "%s/%s", chrootdir, dest ? dest : dir);
	if (chdir(dir) == -1)
		die("chdir to %s", chrootdir);
	if (mount(".", mountdir, NULL, MS_BIND|MS_REC|MS_PRIVATE, NULL) == -1)
		die("Failed to bind mount %s at %s", dir, mountdir);
}

int
main(int argc, char **argv)
{
	struct bindmnt *bmnt;
	uid_t uid = getuid();
	gid_t gid = getgid();
	const char *chrootdir, *cmd, *argv0;
	char **cmdargs, buf[32];
	int c, fd;
	const struct option longopts[] = {
		{ "bind-rw", required_argument, NULL, 'b' },
		{ "version", no_argument, NULL, 'V' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	chrootdir = cmd = NULL;
	argv0 = argv[0];

	while ((c = getopt_long(argc, argv, "b:hV", longopts, NULL)) != -1) {
		switch (c) {
		case 'b':
			if (optarg == NULL || *optarg == '\0')
				break;
			add_bindmount(optarg);
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'h':
			usage(argv0, false);
			/* NOTREACHED */
		case '?':
		default:
			usage(argv0, true);
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2) {
		usage(argv0, true);
		/* NOTREACHED */
	}

	chrootdir = argv[0];
	cmd = argv[1];
	cmdargs = argv + 1;

	/* Never allow chrootdir == / */
	if (strcmp(chrootdir, "/") == 0)
		die("/ is not allowed to be used as chrootdir");

	/* Make chrootdir absolute */
	if (chrootdir[0] != '/') {
		char cwd[PATH_MAX-1];
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			die("getcwd");
		chrootdir = xbps_xasprintf("%s/%s", cwd, chrootdir);
	}

	/*
	 * Unshare from the current process namespaces and set ours.
	 */
	if (unshare(CLONE_NEWUSER|CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWUTS) == -1) {
		errval = 99;
		die("unshare");
	}
	/*
	 * Setup uid/gid user mappings and restrict setgroups().
	 */
	if ((fd = open("/proc/self/uid_map", O_RDWR)) == -1)
		die("failed to open /proc/self/uid_map rw");
	if (write(fd, buf, snprintf(buf, sizeof buf, "%u %u 1\n", uid, uid)) == -1)
		die("failed to write to /proc/self/uid_map");

	close(fd);

	if ((fd = open("/proc/self/setgroups", O_RDWR)) != -1) {
		if (write(fd, "deny", 4) == -1)
			die("failed to write to /proc/self/setgroups");
		close(fd);
	}

	if ((fd = open("/proc/self/gid_map", O_RDWR)) == -1)
		die("failed to open /proc/self/gid_map rw");
	if (write(fd, buf, snprintf(buf, sizeof buf, "%u %u 1\n", gid, gid)) == -1)
		die("failed to write to /proc/self/gid_map");

	close(fd);

	/* bind mount /proc */
	bindmount(chrootdir, "/proc", NULL);

	/* bind mount /sys */
	bindmount(chrootdir, "/sys", NULL);

	/* bind mount /dev */
	bindmount(chrootdir, "/dev", NULL);

	/* bind mount all user specified mnts */
	SIMPLEQ_FOREACH(bmnt, &bindmnt_queue, entries)
		bindmount(chrootdir, bmnt->src, bmnt->dest);

	/* move chrootdir to / and chroot to it */
	if (chdir(chrootdir) == -1)
		die("chdir to %s", chrootdir);

	if (mount(".", ".", NULL, MS_BIND|MS_PRIVATE, NULL) == -1)
		die("Failed to bind mount %s", chrootdir);

	if (mount(chrootdir, "/", NULL, MS_MOVE, NULL) == -1)
		die("Failed to move %s as rootfs", chrootdir);

	if (chroot(".") == -1)
		die("Failed to chroot to %s", chrootdir);

	if (execvp(cmd, cmdargs) == -1)
		die("Failed to execute command %s", cmd);

	/* NOTREACHED */
	exit(EXIT_FAILURE);
}
