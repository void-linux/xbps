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

/*
 * This is based on linux-user-chroot by Colin Walters, but has been adapted
 * specifically for xbps-src use:
 *
 * 	- This bind mounts exactly what we need, no support for additional mounts.
 * 	- This uses IPC/PID/mount namespaces, nothing more.
 * 	- Disables namespace features if running in OpenVZ containers.
 * 	- Supports overlayfs on a tmpfs mounted directory.
 */
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/prctl.h>
#include <sys/fsuid.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <sched.h>
#include <limits.h>	/* PATH_MAX */

#include <xbps.h>

#ifndef SECBIT_NOROOT
#define SECBIT_NOROOT (1 << 0)
#endif

#ifndef SECBIT_NOROOT_LOCKED
#define SECBIT_NOROOT_LOCKED (1 << 1)
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS	38
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

static char *tmpdir;

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
	exit(EXIT_FAILURE);
}

static void
cleanup_overlayfs(void)
{
	if (tmpdir)
		rmdir(tmpdir);
}

static void __attribute__((noreturn))
usage(const char *p)
{
	printf("Usage: %s [-D dir] [-H dir] [-S dir] [-O] <chrootdir> <command>\n\n"
	    "-D <distdir> Directory to be bind mounted at <chrootdir>/void-packages\n"
	    "-H <hostdir> Directory to be bind mounted at <chrootdir>/host\n"
	    "-S <shmdir>  Directory to be bind mounted at <chrootdir>/<shmdir>\n", p);
	exit(EXIT_FAILURE);
}

static int
fsuid_chdir(uid_t uid, const char *path)
{
	int saveerrno, rv;

	(void)setfsuid(uid);
	rv = chdir(path);
	saveerrno = errno;
	(void)setfsuid(0);
	errno = saveerrno;

	return rv;
}

static int
openvz_container(void)
{
	if ((!access("/proc/vz/vzaquota", R_OK)) &&
	    (!access("/proc/user_beancounters", R_OK)))
		return 1;

	return 0;
}

static void
bindmount(uid_t ruid, const char *chrootdir, const char *dir, const char *dest)
{
	char mountdir[PATH_MAX-1];

	snprintf(mountdir, sizeof(mountdir), "%s/%s", chrootdir, dest ? dest : dir);

	if (fsuid_chdir(ruid, dir) == -1)
		die("Couldn't chdir to %s", dir);
	if (mount(".", mountdir, NULL, MS_BIND|MS_REC|MS_PRIVATE, NULL) == -1)
		die("Failed to bind mount %s at %s", dir, mountdir);
}

static char *
setup_overlayfs(const char *chrootdir, uid_t ruid, gid_t rgid)
{
	char *upperdir, *workdir, *newchrootdir, *mopts;
	const void *opts;
	/*
	 * Create a temporary directory on tmpfs for overlayfs storage.
	 */
	if (mount("tmpfs", tmpdir, "tmpfs", 0, NULL) == -1)
		die("failed to mount tmpfs on %s", tmpdir);
	/*
	 * Create the upper/work dirs to setup overlayfs.
	 */
	upperdir = xbps_xasprintf("%s/upperdir", tmpdir);
	if (mkdir(upperdir, 0755) == -1)
		die("failed to create upperdir (%s)", upperdir);

	workdir = xbps_xasprintf("%s/workdir", tmpdir);
	if (mkdir(workdir, 0755) == -1)
		die("failed to create workdir (%s)", workdir);

	newchrootdir = xbps_xasprintf("%s/masterdir", tmpdir);
	if (mkdir(newchrootdir, 0755) == -1)
		die("failed to create newchrootdir (%s)", newchrootdir);

	mopts = xbps_xasprintf("upperdir=%s,lowerdir=%s,workdir=%s",
		upperdir, chrootdir, workdir);

	opts = mopts;
	if (mount(chrootdir, newchrootdir, "overlay", 0, opts) == -1)
		die("failed to mount overlayfs on %s", newchrootdir);

	if (chown(newchrootdir, ruid, rgid) == -1)
		die("chown newchrootdir %s", newchrootdir);

	free(mopts);
	free(upperdir);
	free(workdir);

	return newchrootdir;
}

int
main(int argc, char **argv)
{
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;
	const char *chrootdir, *distdir, *hostdir, *shmdir, *cmd, *argv0;
	char **cmdargs, *b, mountdir[PATH_MAX-1];
	int aidx = 0, clone_flags, child_status = 0;
	pid_t child;
	bool overlayfs = false;

	chrootdir = distdir = hostdir = shmdir = cmd = NULL;
	argv0 = argv[0];
	argc--;
	argv++;

	if (argc < 2)
		usage(argv0);

	while (aidx < argc) {
		if (strcmp(argv[aidx], "-O") == 0) {
			/* use overlayfs */
			overlayfs = true;
			aidx++;
		} else if (strcmp(argv[aidx], "-D") == 0) {
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

	if (getresgid(&rgid, &egid, &sgid) == -1)
		die("getresgid");

	if (getresuid(&ruid, &euid, &suid) == -1)
		die("getresuid");

	if (rgid == 0)
		rgid = ruid;

	if (overlayfs) {
		b = xbps_xasprintf("%s.XXXXXXXXXX", chrootdir);
		if ((tmpdir = mkdtemp(b)) == NULL)
			die("failed to create tmpdir directory");
	}

	clone_flags = (SIGCHLD|CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWUTS|CLONE_NEWPID);
	if (openvz_container()) {
		/*
		 * If running in a OpenVZ container simply disable all namespace
		 * features.
		 */
		clone_flags &= ~(CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWUTS|CLONE_NEWPID);
	}

	/* Issue the clone(2) syscall with our settings */
	if ((child = syscall(__NR_clone, clone_flags, NULL)) == -1)
		die("clone");

	if (child == 0) {
		/*
		 * Restrict privileges on the child.
		 */
		if (prctl(PR_SET_NO_NEW_PRIVS, 1) == -1 && errno != EINVAL) {
			die("prctl PR_SET_NO_NEW_PRIVS");
		} else if (prctl (PR_SET_SECUREBITS,
			SECBIT_NOROOT|SECBIT_NOROOT_LOCKED) == -1) {
			die("prctl SECBIT_NOROOT");
		}
		/* setup our overlayfs if set */
		if (overlayfs)
			chrootdir = setup_overlayfs(chrootdir, ruid, rgid);

		/* mount /proc */
		snprintf(mountdir, sizeof(mountdir), "%s/proc", chrootdir);
		if (mount("proc", mountdir, "proc", MS_MGC_VAL|MS_PRIVATE, NULL) == -1) {
			/* try bind mount */
			bindmount(ruid, chrootdir, "/proc", NULL);
		}

		/* bind mount /sys */
		bindmount(ruid, chrootdir, "/sys", NULL);

		/* bind mount /dev */
		bindmount(ruid, chrootdir, "/dev", NULL);

		/* bind mount hostdir if set */
		if (hostdir)
			bindmount(ruid, chrootdir, hostdir, "/host");

		/* bind mount distdir (if set) */
		if (distdir)
			bindmount(ruid, chrootdir, distdir, "/void-packages");

		/* bind mount shmdir (if set) */
		if (shmdir)
			bindmount(ruid, chrootdir, shmdir, NULL);

		/* move chrootdir to / and chroot to it */
		if (fsuid_chdir(ruid, chrootdir) == -1)
			die("Failed to chdir to %s", chrootdir);

		if (mount(".", ".", NULL, MS_BIND|MS_PRIVATE, NULL) == -1)
			die("Failed to bind mount %s", chrootdir);

		if (mount(chrootdir, "/", NULL, MS_MOVE, NULL) == -1)
			die("Failed to move %s as rootfs", chrootdir);

		if (chroot(".") == -1)
			die("Failed to chroot to %s", chrootdir);

		/* Switch back to the gid/uid of invoking process */
		if (setgid(rgid) == -1)
			die("setgid child");
		if (setuid(ruid) == -1)
			die("setuid child");

		if (execvp(cmd, cmdargs) == -1)
			die("Failed to execute command %s", cmd);
	}
	/* Switch back to the gid/uid of invoking process also in the parent */
	if (setgid(rgid) == -1)
		die("setgid child");
	if (setuid(ruid) == -1)
		die("setuid child");

	/* Wait until the child terminates */
	while (waitpid(child, &child_status, 0) < 0) {
		if (errno != EINTR)
			die("waitpid");
	}

	if (!WIFEXITED(child_status)) {
		cleanup_overlayfs();
		return -1;
	}

	cleanup_overlayfs();
	return WEXITSTATUS(child_status);
}
