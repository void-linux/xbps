/*-
 * Copyright (c) 2014-2020 Juan Romero Pardines.
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
 * 	- Supports read-only bind mounts.
 */
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
#include <dirent.h>

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
	bool ro;	/* readonly */
};

static char *tmpdir;
static bool overlayfs_on_tmpfs;
static SIMPLEQ_HEAD(bindmnt_head, bindmnt) bindmnt_queue =
    SIMPLEQ_HEAD_INITIALIZER(bindmnt_queue);

static void __attribute__((noreturn))
usage(const char *p, bool fail)
{
	printf("Usage: %s [OPTIONS] [--] <dir> <cmd> [<cmdargs>]\n\n"
	    "-B, --bind-ro <src:dest> Bind mounts <src> into <dir>/<dest> (read-only)\n"
	    "-b, --bind-rw <src:dest> Bind mounts <src> into <dir>/<dest> (read-write)\n"
	    "-O, --overlayfs          Creates a tempdir and mounts <dir> read-only via overlayfs\n"
	    "-t, --tmpfs              Creates a tempdir and mounts <dir> on tmpfs (for use with -O)\n"
	    "-o, --options <opts>     Options to be passed to the tmpfs mount (for use with -t)\n"
	    "-V, --version            Show XBPS version\n"
	    "-h, --help               Show usage\n", p);
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
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
ftw_cb(const char *fpath, const struct stat *sb)
{
	int sverrno = 0;

	if (S_ISDIR(sb->st_mode)) {
		if (rmdir(fpath) == -1)
			sverrno = errno;
	} else {
		if (unlink(fpath) == -1)
			sverrno = errno;
	}
	if (sverrno != 0) {
		xbps_error_printf("Failed to remove %s: %s\n", fpath, strerror(sverrno));
	}
	return 0;
}

static int
walk_dir(const char *path,
	int (*fn)(const char *fpath, const struct stat *sb))
{
	struct dirent **list;
	struct stat sb;
	const char *p;
	char tmp_path[PATH_MAX] = {0};
	int rv = 0, i;

	i = scandir(path, &list, NULL, alphasort);
	if (i == -1) {
		rv = -1;
		goto out;
	}
	while (i--) {
		p = list[i]->d_name;
		if (strcmp(p, ".") == 0 || strcmp(p, "..") == 0)
			continue;
		if (strlen(path) + strlen(p) + 1 >= (PATH_MAX - 1)) {
			errno = ENAMETOOLONG;
			rv = -1;
			break;
		}
		strncpy(tmp_path, path, PATH_MAX - 1);
		strncat(tmp_path, "/", PATH_MAX - 1 - strlen(tmp_path));
		strncat(tmp_path, p, PATH_MAX - 1 - strlen(tmp_path));
		if (lstat(tmp_path, &sb) < 0) {
			break;
		}
		if (S_ISDIR(sb.st_mode)) {
			if (walk_dir(tmp_path, fn) < 0) {
				rv = -1;
				break;
			}
		}
		rv = fn(tmp_path, &sb);
		if (rv != 0) {
			break;
		}
	}
out:
	free(list);
	return rv;
}

static void
cleanup_overlayfs(void)
{
	if (tmpdir == NULL)
		return;

	if (overlayfs_on_tmpfs)
		goto out;

	/* recursively remove the temporary dir */
	if (walk_dir(tmpdir, ftw_cb) != 0) {
		xbps_error_printf("Failed to remove directory tree %s: %s\n",
			tmpdir, strerror(errno));
		exit(EXIT_FAILURE);
	}
out:
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
add_bindmount(const char *bm, bool ro)
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
	bmnt->ro = ro;
	SIMPLEQ_INSERT_TAIL(&bindmnt_queue, bmnt, entries);
}

static void
bindmount(const char *chrootdir, const char *dir, const char *dest)
{
	char mountdir[PATH_MAX-1];
	int flags = MS_BIND|MS_PRIVATE;

	snprintf(mountdir, sizeof(mountdir), "%s%s", chrootdir, dest ? dest : dir);

	if (chdir(dir) == -1)
		die("Couldn't chdir to %s", dir);
	if (mount(".", mountdir, NULL, flags, NULL) == -1)
		die("Failed to bind mount %s at %s", dir, mountdir);
}

static void
remount_rdonly(const char *chrootdir, const char *dir, const char *dest, bool ro)
{
	char mountdir[PATH_MAX-1];
	int flags = MS_REMOUNT|MS_BIND|MS_RDONLY;

	if (!ro)
		return;

	snprintf(mountdir, sizeof(mountdir), "%s%s", chrootdir, dest ? dest : dir);

	if (chdir(dir) == -1)
		die("Couldn't chdir to %s", dir);
	if (mount(".", mountdir, NULL, flags, NULL) == -1)
		die("Failed to remount read-only %s at %s", dir, mountdir);
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
	const char *rootdir, *tmpfs_opts, *cmd, *argv0;
	char **cmdargs, *b, *chrootdir, mountdir[PATH_MAX-1];
	int c, clone_flags, container_flags, child_status = 0;
	pid_t child;
	bool overlayfs = false;
	const struct option longopts[] = {
		{ "overlayfs", no_argument, NULL, 'O' },
		{ "tmpfs", no_argument, NULL, 't' },
		{ "options", required_argument, NULL, 'o' },
		{ "bind-rw", required_argument, NULL, 'B' },
		{ "bind-ro", required_argument, NULL, 'b' },
		{ "version", no_argument, NULL, 'V' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 }
	};

	tmpfs_opts = rootdir = cmd = NULL;
	argv0 = argv[0];

	while ((c = getopt_long(argc, argv, "Oto:B:b:Vh", longopts, NULL)) != -1) {
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
		case 'B':
			if (optarg == NULL || *optarg == '\0')
				break;
			add_bindmount(optarg, true);
			break;
		case 'b':
			if (optarg == NULL || *optarg == '\0')
				break;
			add_bindmount(optarg, false);
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

	rootdir = argv[0];
	cmd = argv[1];
	cmdargs = argv + 1;


	/* Make chrootdir absolute */
	chrootdir = realpath(rootdir, NULL);
	if (!chrootdir)
		die("realpath rootdir");

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
		if (chown(tmpdir, ruid, rgid) == -1)
			die("chown tmpdir %s", tmpdir);
		/*
		 * Register a signal handler to clean up temporary masterdir.
		 */
		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = sighandler_cleanup;
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGQUIT, &sa, NULL);
	}

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
		}
		if (prctl(PR_SET_SECUREBITS,
			SECBIT_NOROOT|SECBIT_NOROOT_LOCKED) == -1) {
			die("prctl PR_SET_SECUREBITS");
		}

		/* mount as private, systemd mounts it as shared by default */
		if (mount(NULL, "/", "none", MS_PRIVATE|MS_REC, NULL) == -1)
			die("Failed to mount / private");

		/* setup our overlayfs if set */
		if (overlayfs)
			chrootdir = setup_overlayfs(chrootdir, ruid, rgid,
			    overlayfs_on_tmpfs, tmpfs_opts);

		/* mount /proc */
		snprintf(mountdir, sizeof(mountdir), "%s/proc", chrootdir);
		if (mount("proc", mountdir, "proc",
			  MS_MGC_VAL|MS_PRIVATE|MS_RDONLY, NULL) == -1) {
			/* try bind mount */
			add_bindmount("/proc:/proc", true);
		}
		/* bind mount /sys, /dev (ro) and /dev/shm (rw) */
		add_bindmount("/sys:/sys", true);
		add_bindmount("/dev:/dev", true);
		add_bindmount("/dev/shm:/dev/shm", false);

		/* bind mount all specified mnts */
		SIMPLEQ_FOREACH(bmnt, &bindmnt_queue, entries)
			bindmount(chrootdir, bmnt->src, bmnt->dest);

		/* remount bind mounts as read-only if set */
		SIMPLEQ_FOREACH(bmnt, &bindmnt_queue, entries)
			remount_rdonly(chrootdir, bmnt->src, bmnt->dest, bmnt->ro);

		/* move chrootdir to / and chroot to it */
		if (chdir(chrootdir) == -1)
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
