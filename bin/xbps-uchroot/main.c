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
 * 	- This uses IPC/PID/UTS namespaces, nothing more.
 * 	- Disables namespace features if running inside containers.
 * 	- Supports overlayfs on a temporary directory or a tmpfs mount.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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
#include <ftw.h>
#include <signal.h>
#include <getopt.h>

#include <xbps.h>
#include "queue.h"

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

struct bindmnt {
	SIMPLEQ_ENTRY(bindmnt) entries;
	char *src;
	const char *dest;
};

static char *tmpdir;
static bool overlayfs_on_tmpfs;
static SIMPLEQ_HEAD(bindmnt_head, bindmnt) bindmnt_queue =
    SIMPLEQ_HEAD_INITIALIZER(bindmnt_queue);

static void __attribute__((noreturn))
usage(const char *p)
{
	printf("Usage: %s [-b src:dest] [-O -t -o <opts>] <dir> <cmd> [<cmdargs>]\n\n"
	    "-b src:dest Bind mounts <src> into <dir>/<dest> (may be specified multiple times)\n"
	    "-O          Creates a tempdir and mounts <dir> read-only via overlayfs\n"
	    "-t          Creates tempdir and mounts it on tmpfs (for use with -O)\n"
	    "-o opts     Options to be passed to the tmpfs mount (for use with -t)\n", p);
	exit(EXIT_FAILURE);
}

static void __attribute__((noreturn))
die(const char *fmt, ...)
{
	va_list ap;
	int save_errno = errno;

	va_start(ap, fmt);
	fprintf(stderr, "ERROR: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, " (%s)\n", strerror(save_errno));
	va_end(ap);
	exit(EXIT_FAILURE);
}

static int
ftw_cb(const char *fpath, const struct stat *sb UNUSED, int type,
		struct FTW *ftwbuf UNUSED)
{
	int sverrno = 0;

	if (type == FTW_F || type == FTW_SL || type == FTW_SLN) {
		if (unlink(fpath) == -1)
			sverrno = errno;
	} else if (type == FTW_D || type == FTW_DNR || type == FTW_DP) {
		if (rmdir(fpath) == -1)
			sverrno = errno;
	} else {
		return 0;
	}
	if (sverrno != 0) {
		fprintf(stderr, "Failed to remove %s: %s\n", fpath, strerror(sverrno));
	}
	return 0;
}

static void
cleanup_overlayfs(void)
{
	if (tmpdir == NULL)
		return;

	if (!overlayfs_on_tmpfs) {
		/* recursively remove the temporary dir */
		if (nftw(tmpdir, ftw_cb, 20, FTW_MOUNT|FTW_PHYS|FTW_DEPTH) != 0) {
			fprintf(stderr, "Failed to remove directory tree %s: %s\n",
				tmpdir, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}
	rmdir(tmpdir);
}

static void __attribute__((noreturn))
sighandler_cleanup(int signum)
{
	switch (signum) {
	case SIGINT:
	case SIGTERM:
	case SIGQUIT:
		cleanup_overlayfs();
		break;
	}
	_exit(signum);
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
setup_overlayfs(const char *chrootdir, uid_t ruid, gid_t rgid, bool tmpfs, const char *tmpfs_opts)
{
	char *upperdir, *workdir, *newchrootdir, *mopts;
	const void *opts = NULL;

	if (tmpfs) {
		/*
		* Create a temporary directory on tmpfs for overlayfs storage.
		*/
		opts = tmpfs_opts;
		if (mount("tmpfs", tmpdir, "tmpfs", 0, opts) == -1)
			die("failed to mount tmpfs on %s", tmpdir);
	}
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

	if (chown(upperdir, ruid, rgid) == -1)
		die("chown upperdir %s", upperdir);
	if (chown(workdir, ruid, rgid) == -1)
		die("chown workdir %s", workdir);
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
	struct sigaction sa;
	uid_t ruid, euid, suid;
	gid_t rgid, egid, sgid;
	const char *chrootdir, *tmpfs_opts, *cmd, *argv0;
	char **cmdargs, *b, mountdir[PATH_MAX-1];
	int c, clone_flags, container_flags, child_status = 0;
	pid_t child;
	bool overlayfs = false;
	const struct option longopts[] = {
		{ NULL, 0, NULL, 0 }
	};

	tmpfs_opts = chrootdir = cmd = NULL;
	argv0 = argv[0];

	while ((c = getopt_long(argc, argv, "Oto:b:V", longopts, NULL)) != -1) {
		switch (c) {
		case 'O':
			overlayfs = true;
			break;
		case 't':
			overlayfs_on_tmpfs = true;
			break;
		case 'o':
			tmpfs_opts = optarg;
			break;
		case 'b':
			if (optarg == NULL || *optarg == '\0')
				break;
			add_bindmount(optarg);
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage(argv0);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage(argv0);

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
		if (chown(tmpdir, ruid, rgid) == -1)
			die("chown tmpdir %s", tmpdir);
	}

	/*
	 * Register a signal handler to clean up temporary masterdir.
	 */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sighandler_cleanup;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	clone_flags = (SIGCHLD|CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWUTS|CLONE_NEWPID);
	container_flags = clone_flags & ~(CLONE_NEWNS|CLONE_NEWIPC|CLONE_NEWUTS|CLONE_NEWPID);

	/* Issue the clone(2) syscall with our settings */
	if ((child = syscall(__NR_clone, clone_flags, NULL)) == -1 &&
			(child = syscall(__NR_clone, container_flags, NULL)) == -1)
		die("clone");

	if (child == 0) {
		struct bindmnt *bmnt;
		/*
		 * Restrict privileges on the child.
		 */
		if (prctl(PR_SET_NO_NEW_PRIVS, 1) == -1 && errno != EINVAL) {
			die("prctl PR_SET_NO_NEW_PRIVS");
		} else if (prctl (PR_SET_SECUREBITS,
			SECBIT_NOROOT|SECBIT_NOROOT_LOCKED) == -1) {
			die("prctl SECBIT_NOROOT");
		}

		/* mount as private, systemd mounts it as shared by default */
		if (mount(NULL, "/", "none", MS_PRIVATE|MS_REC, NULL) == -1)
			die("Failed to mount / private");
		if (mount(NULL, "/", "none", MS_PRIVATE|MS_REMOUNT|MS_NOSUID, NULL) == -1)
			die("Failed to remount /");

		/* setup our overlayfs if set */
		if (overlayfs)
			chrootdir = setup_overlayfs(chrootdir, ruid, rgid,
			    overlayfs_on_tmpfs, tmpfs_opts);

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

		/* bind mount all user specified mnts */
		SIMPLEQ_FOREACH(bmnt, &bindmnt_queue, entries)
			bindmount(ruid, chrootdir, bmnt->src, bmnt->dest);

		/* move chrootdir to / and chroot to it */
		if (fsuid_chdir(ruid, chrootdir) == -1)
			die("Failed to chdir to %s", chrootdir);

		if (mount(".", ".", NULL, MS_BIND|MS_PRIVATE, NULL) == -1)
			die("Failed to bind mount %s", chrootdir);

		mount(chrootdir, "/", NULL, MS_MOVE, NULL);

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
