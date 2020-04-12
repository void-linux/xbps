/*
 * Copyright (c) 2010 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * This is a derived version of DragonFly's BSD "fastbulk", adapted for xbps
 * by Juan RP <xtraeme@gmail.com>.
 *
 * This program iterates all srcpkgs directories, runs './xbps-src show-build-deps',
 * and builds a dependency tree on the fly.
 *
 * When the dependency tree is built, terminal dependencies are built
 * and packaged on the fly.
 *
 * As these builds complete additional dependencies may be satisfied and be
 * added to the build order. Ultimately the entire tree is built.
 *
 * Only one attempt is made to build any given package, no matter how many
 * other packages depend on it.
 */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <getopt.h>

#include <xbps.h>
#include "uthash.h"

#ifndef __arraycount
#define __arraycount(x) (sizeof(x) / sizeof(*x))
#endif

struct item;

struct depn {
	struct depn *dnext;
	struct item *item;
};

struct item {
	enum { XWAITING, XDEPFAIL, XBUILD, XRUN, XDONE } status;
	struct item *bnext;	/* BuildList/RunList next */
	struct depn *dbase;	/* packages depending on us */
	char *pkgn;		/* package name */
	int dcount;		/* build completion for our dependencies */
	int xcode;		/* exit code from build */
	pid_t pid;		/* running build */
	UT_hash_handle hh;
};

static struct item *hashtab;
static struct item *BuildList;
static struct item **BuildListP = &BuildList;
static struct item *RunList;

int NParallel = 1;
int VerboseOpt;
int NRunning;
unsigned int NBuilt = 0;
unsigned int NFinished = 0;
unsigned int NChecked = 0;
unsigned int NSkipped = 0;
unsigned int NTotal = 0;
char *LogDir;

static struct item *
lookupItem(const char *pkgn)
{
	struct item *item = NULL;

	assert(pkgn);

	HASH_FIND_STR(hashtab, pkgn, item);
	return item;
}

static struct item *
addItem(const char *pkgn)
{
	struct item *item = calloc(1, sizeof (struct item));

	assert(pkgn);
	assert(item);

	item->status = XWAITING;
	item->pkgn = strdup(pkgn);
	assert(item->pkgn);

	HASH_ADD_KEYPTR(hh, hashtab, item->pkgn, strlen(item->pkgn), item);

	return item;
}

static void __attribute__((noreturn))
usage(const char *progname)
{
	fprintf(stderr, "%s [-h] [-j procs] [-l logdir] [-V]"
	    " /path/to/void-packages [pkg pkgN]\n", progname);
	exit(EXIT_FAILURE);
}

/*
 * Add the item to the build request list.  This routine is called
 * after all build dependencies have been satisfied for the item.
 * runBuilds() will pick items off of BuildList to keep the parallel
 * build pipeline full.
 */
static void
addBuild(struct item *item)
{
	assert(item);

	*BuildListP = item;
	BuildListP = &item->bnext;
	item->status = XBUILD;
}

/*
 * Process the build completion for an item.
 */
static void
processCompletion(struct item *item)
{
	struct depn *depn;
	struct item *xitem;
	const char *logdir;
	char *logpath, *logpath2;
	FILE *fp;

	assert(item);
	/*
	 * If XRUN we have to move the logfile to the correct directory.
	 * (If XDEPFAIL the log is at correct location).
	 */
	if (item->status == XRUN) {
		logpath = xbps_xasprintf("%s/run/%s", LogDir, item->pkgn);
		switch (item->xcode) {
		case 0:
			logdir = "good";
			break;
		case 2:
			logdir = "skipped";
			break;
		default:
			logdir = "bad";
			break;
		}
		logpath2 = xbps_xasprintf("%s/%s/%s", LogDir, logdir, item->pkgn);
		(void)rename(logpath, logpath2);
		free(logpath);
		free(logpath2);
	}

	assert(item->status == XRUN || item->status == XDEPFAIL);
	item->status = XDONE;

	for (depn = item->dbase; depn; depn = depn->dnext) {
		xitem = depn->item;
		assert(xitem->dcount > 0);
		--xitem->dcount;

		if (xitem->status == XWAITING || xitem->status == XDEPFAIL) {
			/*
			 * If our build went well add items dependent
			 * on us to the build, otherwise fail the items
			 * dependent on us.
			 */
			if (item->xcode == 0) {
				if (xitem->dcount == 0) {
					if (xitem->status == XWAITING) {
						addBuild(xitem);
					} else {
						processCompletion(xitem);
					}
				}
			} else {
				xitem->xcode = item->xcode;
				xitem->status = XDEPFAIL;
				logpath = xbps_xasprintf("%s/deps/%s", LogDir, xitem->pkgn);
				fp = fopen(logpath, "a");
				fprintf(fp, "%s\n", item->pkgn);
				fclose(fp);
				free(logpath);
				processCompletion(xitem);
				++NSkipped;
			}
		}
	}
	++NFinished;

	printf("[%u/%u] Finished %s (PID: %u RET: %d)\n",
	    NFinished, NTotal, item->pkgn, item->pid, item->xcode);
}

/*
 * Wait for a running build to finish and process its completion.
 * Return the build or NULL if no builds are pending.
 *
 * The caller should call runBuilds() in the loop to keep the build
 * pipeline full until there is nothing left in the build list.
 */
static struct item *
waitRunning(int flags)
{
	struct item *item = NULL;
	struct item **itemp;
	pid_t pid;
	int status;

	if (RunList == NULL)
		return NULL;

	while (((pid = waitpid(0, &status, flags)) < 0) && !flags)
		;

	if (pid > 0) {
		status = WEXITSTATUS(status);
		itemp = &RunList;
		while ((item = *itemp) != NULL) {
			if (item->pid == pid)
				break;
			itemp = &item->bnext;
		}
		if (item) {
			*itemp = item->bnext;
			item->bnext = NULL;
			item->xcode = status;
			--NRunning;
			processCompletion(item);
		}
	}
	return item;
}

/*
 * Start new builds from the build list and handle build completions,
 * which can potentialy add new items to the build list.
 *
 * This routine will maintain up to NParallel builds. A new build is
 * only started once its dependencies have been completed successfully.
 */
static void
runBuilds(const char *bpath)
{
	struct item *item;
	char *logpath;
	FILE *fp;
	int fd;

	assert(bpath);
	/*
	 * Try to maintain up to NParallel builds
	 */
	while (NRunning < NParallel && BuildList) {
		item = BuildList;
		if ((BuildList = item->bnext) == NULL)
			BuildListP = &BuildList;

		item->status = XRUN;
		/*
		 * When [re]running a build remove any bad log from prior
		 * attempts.
		 */
		logpath = xbps_xasprintf("%s/bad/%s", LogDir, item->pkgn);
		(void)remove(logpath);
		free(logpath);
		logpath = xbps_xasprintf("%s/deps/%s", LogDir, item->pkgn);
		(void)remove(logpath);
		free(logpath);
		logpath = xbps_xasprintf("%s/skipped/%s", LogDir, item->pkgn);
		(void)remove(logpath);
		free(logpath);
		logpath = xbps_xasprintf("%s/run/%s", LogDir, item->pkgn);

		item->pid = fork();
		if (item->pid == 0) {
			/*
			 * Child process - setup the log file and build
			 */
			if (chdir(bpath) < 0)
				_exit(99);


			fd = open(logpath, O_RDWR|O_CREAT|O_TRUNC, 0666);
			if (fd != 1)
				dup2(fd, 1);
			if (fd != 2)
				dup2(fd, 2);
			if (fd != 1 && fd != 2)
				close(fd);
			fd = open("/dev/null", O_RDWR);
			if (fd != 0) {
				dup2(fd, 0);
				close(fd);
			}
			/* build the current pkg! */
			execl("./xbps-src", "./xbps-src",
				"-E", "-N", "-t", "pkg", item->pkgn, NULL);

			_exit(99);
		} else if (item->pid < 0) {
			/*
			 * Parent fork() failed, log the problem and
			 * do completion processing.
			 */
			item->xcode = -98;
			fp = fopen(logpath, "a");
			fprintf(fp, "xbps-fbulk: unable to fork/exec xbps-src\n");
			fclose(fp);
			processCompletion(item);
		} else {
			/*
			 * Parent is now tracking the running child,
			 * add the item to the RunList.
			 */
			item->bnext = RunList;
			RunList = item;
			++NRunning;
			++NBuilt;
			printf("[%u/%u] Building %s (PID: %u)\n",
			     NBuilt+NSkipped, NTotal, item->pkgn, item->pid);
		}
		free(logpath);
	}
	/*
	 * Process any completed builds (non-blocking)
	 */
	while (waitRunning(WNOHANG) != NULL)
		;
}

/*
 * Add a reverse dependency from the deepest point (xitem) to the
 * packages that depend on xitem (item in this case).
 *
 * Caller will check dcount after it is through adding dependencies.
 */
static void
addDepn(struct item *item, struct item *xitem)
{
	struct depn *depn = calloc(1, sizeof(struct depn));

	assert(item);
	assert(xitem);
	assert(depn);

	depn->item = item;
	depn->dnext = xitem->dbase;
	xitem->dbase = depn;
	++item->dcount;
}

/*
 * Recursively execute 'xbps-src show-build-deps' to calculate all required
 * dependencies.
 */
static struct item *
ordered_depends(const char *bpath, const char *pkgn)
{
	struct item *item, *xitem;
	char buf[1024], cmd[PATH_MAX];
	FILE *fp;

	assert(bpath);
	assert(pkgn);

	item = addItem(pkgn);
	/*
	 * Retrieve and process dependencies recursively.
	 */
	++NChecked;
	printf("[%u] Checking %s\n", NChecked, item->pkgn);

	snprintf(cmd, sizeof(cmd)-1,
	    "%s/xbps-src show-build-deps %s 2>&1", bpath, pkgn);
	fp = popen(cmd, "r");
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char dpath[PATH_MAX];
		size_t len;
		struct stat st;

		/* ignore xbps-src messages */
		if (strncmp(buf, "=>", 2) == 0) {
			continue;
		}

		len = strlen(buf);
		if (len && buf[len-1] == '\n')
			buf[--len] = 0;

		snprintf(dpath, sizeof(dpath)-1,
		    "%s/srcpkgs/%s/template", bpath, buf);
		if (stat(dpath, &st) == -1) {
			/*
			 * Ignore unexistent dependencies, this
			 * might happen for virtual packages or 
			 * autogenerated pkgs (-32bit, etc).
			 *
			 * We don't really care if the pkg has
			 * invalid dependencies, at build time they
			 * will be properly catched by xbps-src.
			 */
			continue;
		}
		if (VerboseOpt)
			printf("%s: depends on %s\n", pkgn, buf);

		xitem = lookupItem(buf);
		if (xitem == NULL)
			xitem = ordered_depends(bpath, buf);

		addDepn(item, xitem);
	}
	pclose(fp);
	++NTotal;
	/*
	 * If the item has no dependencies left add it to the build list.
	 */
	if (item->dcount == 0) {
		addBuild(item);
	} else {
		if (VerboseOpt)
			printf("Deferred package: %s\n", item->pkgn);
	}
	return item;
}

int
main(int argc, char **argv)
{
	DIR *dir;
	struct dirent *den;
	struct stat st;
	const char *progname = argv[0];
	const char *logdirs[] = { "good", "bad", "run", "deps", "skipped" };
	char *bpath, *rpath, *tmp, cwd[PATH_MAX];
	size_t blen;
	int ch;
	const struct option longopts[] = {
		{ NULL, 0, NULL, 0 }
	};

	while ((ch = getopt_long(argc, argv, "hj:l:vV", longopts, NULL)) != -1) {
		switch (ch) {
		case 'j':
			NParallel = strtol(optarg, NULL, 0);
			break;
		case 'v':
			VerboseOpt = 1;
			break;
		case 'l':
			LogDir = optarg;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case 'h':
		default:
			usage(progname);
			/* NOT REACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1) {
		usage(progname);
		/* NOT REACHED */
	}

	/*
	 * Check masterdir is properly initialized.
	 */
	if ((bpath = realpath(argv[0], NULL)) == NULL)
		exit(EXIT_FAILURE);

	blen = strlen(bpath) + strlen("/srcpkgs") + 1;
	rpath = malloc(blen);
	assert(rpath);
	snprintf(rpath, blen, "%s/srcpkgs", bpath);

	tmp = xbps_xasprintf("%s/masterdir/.xbps_chroot_init", bpath);
	if (access(tmp, R_OK) == -1) {
		fprintf(stderr, "ERROR: %s/masterdir wasn't initialized, "
		    "run binary-bootstrap first.\n", bpath);
		exit(EXIT_FAILURE);
	}
	free(tmp);

	/*
	 * Create Logdirs.
	 */
	if (LogDir == NULL) {
		if (getcwd(cwd, sizeof(cwd)-1) == NULL) {
			exit(EXIT_FAILURE);
		}
		tmp = xbps_xasprintf("%s/fbulk-log.%u", cwd, (unsigned)getpid());
	} else {
		tmp = strdup(LogDir);
	}
	if (xbps_mkpath(tmp, 0755) != 0) {
		fprintf(stderr, "ERROR: failed to create %s logdir: %s\n",
		     tmp, strerror(errno));
		exit(EXIT_FAILURE);
	}
	LogDir = realpath(tmp, NULL);
	free(tmp);
	assert(LogDir);

	for (unsigned int i = 0; i < __arraycount(logdirs); i++) {
		const char *p = logdirs[i];
		tmp = xbps_xasprintf("%s/%s", LogDir, p);
		if (xbps_mkpath(tmp, 0755) != 0) {
			fprintf(stderr, "ERROR: failed to create %s logdir: %s\n",
			    tmp, strerror(errno));
			exit(EXIT_FAILURE);
		}
		free(tmp);
	}

	/*
	 * Generate dependency tree. This is done in two steps to know how
	 * many packages will be built.
	 */
	if (chdir(rpath) == -1) {
		fprintf(stderr, "ERROR: failed to chdir to %s: %s\n",
		    rpath, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if ((dir = opendir(rpath)) != NULL) {
		while ((den = readdir(dir)) != NULL) {
			char xpath[PATH_MAX];
			bool found = false;

			if (den->d_name[0] == '.' ||
			    (strcmp(den->d_name, "..") == 0))
				continue;

			if (lstat(den->d_name, &st) == -1)
				continue;

			if (!S_ISDIR(st.st_mode))
				continue;

			if (argc == 1) {
				/* process all pkgs */
				found = true;
			}
			/* only process pkgs specified as arguments */
			for (int i = 1; i < argc; i++) {
				if (strcmp(argv[i], den->d_name) == 0) {
					found = true;
					break;
				}
			}
			if (!found)
				continue;

			snprintf(xpath, sizeof(xpath)-1, "%s/template", den->d_name);
			if ((stat(xpath, &st) == 0) && !lookupItem(den->d_name)) {
				ordered_depends(bpath, den->d_name);
			}
		}
		(void)closedir(dir);
	}
	/*
	 * Start building collected packages, keep the pipeline full
	 * until both the BuildList and RunList have been exhausted.
	 */
	free(rpath);
	runBuilds(bpath);
	while (waitRunning(0) != NULL)
		runBuilds(bpath);

	exit(EXIT_SUCCESS);
}

