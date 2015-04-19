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
 * by Juan RP <xtraeme@voidlinux.eu>.
 *
 * This program iterates all srcpkgs directories, runs './xbps-src show-build-deps',
 * and builds a dependency tree on the fly.
 *
 * As the dependency tree is being built, terminal dependencies are built
 * and packaged on the fly.
 *
 * As these builds complete additional dependencies may be satisfied and be
 * added to the build order. Ultimately the entire tree is built.
 *
 * Only one attempt is made to build any given package, no matter how many
 * other packages depend on it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <xbps.h>

struct item;

struct depn {
	struct depn *dnext;
	struct item *item;
};

struct item {
	enum { XWAITING, XDEPFAIL, XBUILD, XRUN, XDONE, XBROKEN } status;
	struct item *hnext;	/* ItemHash next */
	struct item *bnext;	/* BuildList/RunList next */
	struct depn *dbase;	/* packages depending on us */
	char *pkgn;		/* package name */
	char *emsg;		/* error message */
	int dcount;		/* build completion for our dependencies */
	int xcode;		/* exit code from build */
	pid_t pid;		/* running build */
};

#define ITHSIZE	1024
#define ITHMASK	(ITHSIZE - 1)

static struct item *ItemHash[ITHSIZE];
static struct item *BuildList;
static struct item **BuildListP = &BuildList;
static struct item *RunList;

int NParallel = 1;
int VerboseOpt;
int NRunning;
char *LogDir;
char *TargetArch;

/*
 * Item hashing and dependency helper routines, called during the
 * directory scan.
 */
static int
itemhash(const char *pkgn)
{
	int hv = 0xA1B5F342;
	int i;

	assert(pkgn);

	for (i = 0; pkgn[i]; ++i)
		hv = (hv << 5) ^ (hv >> 23) ^ pkgn[i];

	return hv & ITHMASK;
}

static struct item *
lookupItem(const char *pkgn)
{
	struct item *item;

	assert(pkgn);

	for (item = ItemHash[itemhash(pkgn)]; item; item = item->hnext) {
		if (strcmp(pkgn, item->pkgn) == 0)
			return item;
	}
	return NULL;
}

static struct item *
addItem(const char *pkgn)
{
	struct item **itemp;
	struct item *item = calloc(sizeof(*item), 1);

	assert(pkgn);
	assert(item);

	itemp = &ItemHash[itemhash(pkgn)];
	item->status = XWAITING;
	item->hnext = *itemp;
	item->pkgn = strdup(pkgn);
	*itemp = item;

	return item;
}

static void __attribute__((noreturn))
usage(const char *progname)
{
	fprintf(stderr, "%s [-a targetarch] [-j parallel] [-l logdir] /path/to/void-packages\n", progname);
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

	printf("BuildOrder %s\n", item->pkgn);
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
	char *logpath1;
	char *logpath2;
	char *logpath3;
	FILE *fp;

	assert(item);
	/*
	 * If XRUN we have to move the logfile to the correct directory.
	 * (If XDEPFAIL the logfile is already in the correct directory).
	 */
	if (item->status == XRUN) {
		logpath1 = xbps_xasprintf("%s/run/%s", LogDir, item->pkgn);
		logpath2 = xbps_xasprintf("%s/%s/%s", LogDir,
			 (item->xcode ? "bad" : "good"), item->pkgn);
		rename(logpath1, logpath2);
		free(logpath1);
		free(logpath2);
	}
	/*
	 * If XBROKEN, "xbps-src show-build-deps" returned an error, perhaps
	 * because the pkg is currently broken or cannot be packaged for the
	 * target architecture, just set it as broken.
	 */
	if (item->status == XBROKEN) {
		logpath1 = xbps_xasprintf("%s/bad/%s", LogDir, item->pkgn);
		fp = fopen(logpath1, "a");
		fprintf(fp, "%s", item->emsg);
		fclose(fp);
		free(logpath1);
		free(item->emsg);
	}

	printf("Finish %-3d %s\n", item->xcode, item->pkgn);
	assert(item->status == XRUN || item->status == XBROKEN || item->status == XDEPFAIL);
	item->status = XDONE;

	for (depn = item->dbase; depn; depn = depn->dnext) {
		xitem = depn->item;
		assert(xitem->dcount > 0);
		--xitem->dcount;
		if (xitem->status == XWAITING || xitem->status == XDEPFAIL || xitem->status == XBROKEN) {
			/*
			 * If our build went well add items dependent
			 * on us to the build, otherwise fail the items
			 * dependent on us.
			 */
			if (item->xcode) {
				xitem->xcode = item->xcode;
				xitem->status = XDEPFAIL;
				logpath3 = xbps_xasprintf("%s/bad/%s", LogDir, xitem->pkgn);
				fp = fopen(logpath3, "a");
				fprintf(fp, "Dependency failed: %s\n", item->pkgn);
				fclose(fp);
				free(logpath3);
			}
			if (xitem->dcount == 0) {
				if (xitem->status == XWAITING)
					addBuild(xitem);
				else
					processCompletion(xitem);
			}
		} else if (xitem->status == XDONE && xitem->xcode) {
			/*
			 * The package depending on us has already run
			 * (this case should not occur).
			 *
			 * Add this dependency failure to its log file
			 * (which has already been renamed).
			 */
			logpath3 = xbps_xasprintf("%s/bad/%s", LogDir, xitem->pkgn);
			fp = fopen(logpath3, "a");
			fprintf(fp, "Dependency failed: %s\n", item->pkgn);
			fclose(fp);
			free(logpath3);
		}
	}
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
	struct item *item;
	struct item **itemp;
	pid_t pid;
	int status;

	if (RunList == NULL)
		return NULL;

	while ((pid = wait3(&status, flags, NULL)) < 0 && flags == 0)
		;

	/*
	 * NOTE! The pid may be associated with one of our popen()'s
	 *	 so just ignore it if we cannot find it.
	 */
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
	} else {
		item = NULL;
	}
	return item;
}

/*
 * Start new builds from the build list and handle build completions,
 * which can potentialy add new items to the build list.
 *
 * This routine will maintain up to NParallel builds.  A new build is
 * only started once its dependencies have completed successfully so
 * when the bulk build starts it typically takes a little while before
 * fastbulk can keep the parallel pipeline full.
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

		printf("BuildStart %s\n", item->pkgn);

		/*
		 * When [re]running a build remove any bad log from prior
		 * attempts.
		 */
		logpath = xbps_xasprintf("%s/bad/%s", LogDir, item->pkgn);
		remove(logpath);
		free(logpath);

		logpath = xbps_xasprintf("%s/run/%s", LogDir, item->pkgn);
		item->status = XRUN;

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
			if (TargetArch != NULL)
				execl("./xbps-src", "./xbps-src", "-a", TargetArch,
					"-E", "-N", "-t", "pkg", item->pkgn, NULL);
			else
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
			fprintf(fp, "xfbulk: Unable to fork/exec xbps-src\n");
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
	struct depn *depn = calloc(sizeof(*depn), 1);
	char *logpath3;
	FILE *fp;

	assert(item);
	assert(xitem);
	assert(depn);

	if (VerboseOpt)
		printf("%s: added dependency: %s\n", item->pkgn, xitem->pkgn);

	depn->item = item;
	depn->dnext = xitem->dbase;
	xitem->dbase = depn;
	if (xitem->status == XDONE) {
		if (xitem->xcode) {
			assert(item->status == XWAITING ||
			       item->status == XDEPFAIL);
			item->xcode = xitem->xcode;
			item->status = XDEPFAIL;
			logpath3 = xbps_xasprintf("%s/bad/%s", LogDir, item->pkgn);
			fp = fopen(logpath3, "a");
			fprintf(fp, "Dependency failed: %s\n", xitem->pkgn);
			fclose(fp);
			free(logpath3);
		}
	} else {
		++item->dcount;
	}
}

/*
 * Recursively execute './xbps-src show-build-deps' to calculate all required
 * dependencies.
 */
static struct item *
ordered_depends(const char *bpath, const char *pkgn)
{
	struct item *item, *xitem;
	char buf[1024];
	FILE *fp;
	char *cmd;

	assert(bpath);
	assert(pkgn);

	item = addItem(pkgn);
	/*
	 * Retrieve and process dependencies recursively.  Note that
	 * addDepn() can modify item's status.
	 *
	 * Guard the recursion by bumping dcount to prevent the item
	 * from being processed for completion while we are still adding
	 * its dependencies.  This would normally not occur but it can
	 * if a pkg has a broken dependency loop.
	 */
	++item->dcount;

	if (VerboseOpt)
		printf("%s: collecting build dependencies...\n", pkgn);

	if (TargetArch != NULL)
		cmd = xbps_xasprintf("%s/xbps-src -a %s show-build-deps %s 2>&1", bpath, TargetArch, pkgn);
	else
		cmd = xbps_xasprintf("%s/xbps-src show-build-deps %s 2>&1", bpath, pkgn);

	fp = popen(cmd, "r");
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *tmp, *tmp2, *depn;
		size_t len;

		/* ignore pkgs returning errors */
		if (strncmp(buf, "=> ERROR", 8) == 0) {
			item->emsg = strdup(buf);
			item->status = XBROKEN;
			item->xcode = EXIT_FAILURE;
			break;
		}
		len = strlen(buf);
		if (len && buf[len-1] == '\n')
			buf[--len] = 0;
		/*
		 * Grab the package name component...
		 */
		if ((depn = xbps_pkgpattern_name(buf)) == NULL)
			if ((depn = xbps_pkg_name(buf)) == NULL)
				depn = strdup(buf);

		if (VerboseOpt)
			printf("%s: depends on %s\n", pkgn, depn);

		assert(depn);
		/*
		 * ... and then convert it to the real source package
		 * (dependency could be a subpackage).
		 */
		tmp = xbps_xasprintf("%s/srcpkgs/%s", bpath, depn);
		if ((tmp2 = realpath(tmp, NULL)) == NULL) {
			item->emsg = xbps_xasprintf("unresolved dependency: %s\n", depn);
			item->status = XBROKEN;
			item->xcode = EXIT_FAILURE;
			free(depn);
			free(tmp);
			break;
		}
		free(tmp);
		free(depn);
		tmp = strdup(tmp2);
		depn = basename(tmp);
		assert(depn);
		free(tmp2);

		if (VerboseOpt)
			printf("%s: dependency transformed to %s\n", pkgn, depn);

		xitem = lookupItem(depn);
		if (xitem == NULL)
			xitem = ordered_depends(bpath, depn);
		addDepn(item, xitem);
		free(tmp);

	}
	pclose(fp);
	free(cmd);
	--item->dcount;

	/*
	 * If the item has no dependencies left either add it to the
	 * build list or do completion processing (i.e. if some of the
	 * dependencies failed).
	 */
	if (item->dcount == 0) {
		switch (item->status) {
		case XWAITING:
			addBuild(item);
			break;
		case XBROKEN:
		case XDEPFAIL:
			processCompletion(item);
			break;
		default:
			assert(0);
			/* NOT REACHED */
			break;
		}
	} else {
		if (VerboseOpt)
			printf("Deferred package: %s\n", item->pkgn);
	}
	runBuilds(bpath);
	return item;
}

int
main(int argc, char **argv)
{
	DIR *dir;
	struct dirent *den;
	struct stat st;
	const char *progname = argv[0];
	char *bpath, *rpath, *minit, *tmp, cwd[PATH_MAX-1];
	size_t blen;
	int ch;

	while ((ch = getopt(argc, argv, "a:j:l:v")) != -1) {
		switch (ch) {
		case 'a':
			TargetArch = optarg;
			break;
		case 'j':
			NParallel = strtol(optarg, NULL, 0);
			break;
		case 'v':
			VerboseOpt = 1;
			break;
		case 'l':
			LogDir = optarg;
			break;
		default:
			usage(progname);
			/* NOT REACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage(progname);
		/* NOT REACHED */
	}

	if ((bpath = realpath(argv[0], NULL)) == NULL)
		exit(EXIT_FAILURE);

	blen = strlen(bpath) + strlen("/srcpkgs") + 1;
	rpath = malloc(blen);
	assert(rpath);
	snprintf(rpath, blen, "%s/srcpkgs", bpath);

	minit = xbps_xasprintf("%s/masterdir/.xbps_chroot_init", bpath);
	if (access(rpath, R_OK) == -1) {
		fprintf(stderr, "ERROR: %s/masterdir wasn't initialized, "
		    "run binary-bootstrap first.\n", bpath);
		exit(EXIT_FAILURE);
	}
	free(minit);
	/*
	 * Create LogDir and its subdirs.
	 */
	if (getcwd(cwd, sizeof(cwd)) == NULL)
		exit(EXIT_FAILURE);

	if (LogDir == NULL) {
		tmp = xbps_xasprintf("%s/log.%u", cwd, (unsigned)getpid());
	} else {
		tmp = strdup(LogDir);
	}
	assert(tmp);
	if (xbps_mkpath(tmp, 0755) != 0) {
		fprintf(stderr, "ERROR: failed to create LogDir %s: %s\n", tmp, strerror(errno));
		exit(EXIT_FAILURE);
	}
	LogDir = realpath(tmp, NULL);
	assert(tmp);
	free(tmp);

	tmp = xbps_xasprintf("%s/run", LogDir);
	if (xbps_mkpath(tmp, 0755) != 0) {
		fprintf(stderr, "ERROR: failed to create RunLogDir %s: %s\n", tmp, strerror(errno));
		exit(EXIT_FAILURE);
	}
	free(tmp);
	tmp = xbps_xasprintf("%s/good", LogDir);
	if (xbps_mkpath(tmp, 0755) != 0) {
		fprintf(stderr, "ERROR: failed to create GoodLogDir %s: %s\n", tmp, strerror(errno));
		exit(EXIT_FAILURE);
	}
	free(tmp);
	tmp = xbps_xasprintf("%s/bad", LogDir);
	if (xbps_mkpath(tmp, 0755) != 0) {
		fprintf(stderr, "ERROR: failed to create BadLogDir %s: %s\n", tmp, strerror(errno));
		exit(EXIT_FAILURE);
	}
	free(tmp);

	/*
	 * Process all directories in void-packages/srcpkgs, excluding symlinks
	 * (subpackages).
	 */
	assert(chdir(rpath) == 0);

	if ((dir = opendir(rpath)) != NULL) {
		while ((den = readdir(dir)) != NULL) {
			char *xpath;

			if (den->d_name[0] == '.')
				continue;

			if (lstat(den->d_name, &st) == -1)
				continue;

			if (!S_ISDIR(st.st_mode))
				continue;

			xpath = xbps_xasprintf("%s/template", den->d_name);

			if (lookupItem(den->d_name) == NULL &&
			    stat(xpath, &st) == 0)
				ordered_depends(bpath, den->d_name);

			free(xpath);
		}
		(void)closedir(dir);
	}
	/*
	 * Wait for all current builds to finish running, keep the pipeline
	 * full until both the BuildList and RunList have been exhausted.
	 */
	runBuilds(bpath);
	while (waitRunning(0) != NULL)
		runBuilds(bpath);

	exit(EXIT_SUCCESS);
}

