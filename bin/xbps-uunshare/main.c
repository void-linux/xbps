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
#define _GNU_SOURCE
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

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

static int errval = 0;

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
usage(const char *p)
{
	printf("Usage: %s [-D dir] [-H dir] [-S dir] <chrootdir> <command>\n\n"
	    "-D <distdir> Directory to be bind mounted at <chrootdir>/void-packages\n"
	    "-H <hostdir> Directory to be bind mounted at <chrootdir>/host\n"
	    "-S <shmdir>  Directory to be bind mounted at <chrootdir>/<shmdir>\n", p);
	exit(EXIT_FAILURE);
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
	uid_t uid = getuid();
	gid_t gid = getgid();
	const char *chrootdir, *distdir, *hostdir, *shmdir, *cmd, *argv0;
	char **cmdargs, buf[32];
	int fd, aidx = 0;

	chrootdir = distdir = hostdir = shmdir = cmd = NULL;
	argv0 = argv[0];
	argc--;
	argv++;

	if (argc < 2)
		usage(argv0);

	while (aidx < argc) {
		if (strcmp(argv[aidx], "-D") == 0) {
			/* distdir */
			distdir = argv[aidx+1];
			aidx += 2;
		} else if (strcmp(argv[aidx], "-H") == 0) {
			/* hostdir */
			hostdir = argv[aidx+1];
			aidx += 2;
		} else if (strcmp(argv[aidx], "-S") == 0) {
			/* shmdir */
			shmdir = argv[aidx+1];
			aidx += 2;
		} else {
			break;
		}
	}
	if ((argc - aidx) < 2)
		usage(argv0);

	chrootdir = argv[aidx];
	cmd = argv[aidx+1];
	cmdargs = argv + aidx + 1;

	/* Never allow chrootdir == / */
	if (strcmp(chrootdir, "/") == 0)
		die("/ is not allowed to be used as chrootdir");

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
		die("failed to open /proc/self/uidmap rw");
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

	/* bind mount hostdir if set */
	if (hostdir)
		bindmount(chrootdir, hostdir, "/host");

	/* bind mount distdir (if set) */
	if (distdir)
		bindmount(chrootdir, distdir, "/void-packages");

	/* bind mount shmdir (if set) */
	if (shmdir)
		bindmount(chrootdir, shmdir, NULL);

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
