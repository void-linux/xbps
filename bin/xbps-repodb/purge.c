/*- * Copyright (c) 2020 Duncan Overbruck <mail@duncano.de>
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
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#include <xbps.h>

#include "uthash.h"
#include "defs.h"

struct arch {
	char arch[64];
	struct arch *next;
	struct xbps_repo *repo, *stage;
};

struct repo {
	char path[PATH_MAX];
	struct repo *next;
	struct dirent **namelist;
	int nnames;
	struct arch *archs;
};

static struct repo *repos;

static void
add_repo(struct xbps_handle *xhp, const char *path)
{
	struct repo *repo = calloc(1, sizeof (struct repo));
	if (repo == NULL) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	xbps_strlcpy(repo->path, path, sizeof repo->path);
	repo->namelist = NULL;
	repo->nnames = 0;
	repo->archs = NULL;
	repo->next = repos;
	repos = repo;
	xbps_dbg_printf(xhp, "Scanning repository: %s\n", path);

	repo->nnames = scandir(path, &repo->namelist, NULL, NULL);
	if (repo->nnames == -1) {
		perror("scandir");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < repo->nnames; i++) {
		const char *name = repo->namelist[i]->d_name, *d;
		if (*name == '.')
			continue;
		if ((d = strrchr(name, '-')) == NULL)
			continue;
		if (strcmp(d+1, "repodata") == 0) {
			struct arch *arch = calloc(1, sizeof (struct arch));
			if (arch == NULL) {
				perror("calloc");
				exit(1);
			}
			if ((size_t)(d-name) >= sizeof arch->arch) {
				xbps_error_printf("invalid repodata: %s\n", name);
				exit(1);
			}
			strncpy(arch->arch, name, d-name);
			arch->next = repo->archs;
			repo->archs = arch;
			xbps_dbg_printf(xhp, "  found architecture: %s\n", arch->arch);

			xhp->target_arch = arch->arch;
			arch->repo = xbps_repo_public_open(xhp, path);
			if (arch->repo == NULL) {
				xbps_error_printf("Failed to read repodata: %s",
				    strerror(errno));
				exit(1);
			}
			arch->stage = xbps_repo_stage_open(xhp, path);
			if (arch->repo == NULL && errno != ENOENT) {
				xbps_error_printf("Failed to read stagedata: %s",
				    strerror(errno));
				exit(1);
			}
		}
	}
}

static bool
same_pkgver(xbps_dictionary_t pkgd, const char *pkgver)
{
	const char *rpkgver = NULL;
	xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &rpkgver);
	return strcmp(pkgver, rpkgver) == 0;
}

/*
 * Return true if this pkgver is not in any of the repodata
 * or stagedata repository indexes.
 */
static bool
check_obsolete_noarch(struct xbps_handle *xhp, struct repo *repo,
		const char *pkgver)
{
	char name[XBPS_NAME_SIZE];
	xbps_dictionary_t pkgd;

	if (!xbps_pkg_name(name, sizeof name, pkgver)) {
		xbps_error_printf("invalid pkgver: %s\n", pkgver);
		return false;
	}
	for (struct arch *a = repo->archs; a; a = a->next) {
		if (a->stage != NULL) {
			if ((pkgd = xbps_dictionary_get(a->stage->idx, name)) != NULL) {
				if (same_pkgver(pkgd, pkgver)) {
					xbps_dbg_printf(xhp, "found package "
					    "`%s' in `%s/%s-stagedata'\n", pkgver, repo->path,
					    a->arch);
					return false;
				}
			}
		}
		if ((pkgd = xbps_dictionary_get(a->repo->idx, name)) != NULL) {
			if (same_pkgver(pkgd, pkgver)) {
				xbps_dbg_printf(xhp, "found package "
					"`%s' in `%s/%s-repodata'\n", pkgver, repo->path, a->arch);
				return false;
			}
		}
	}

	xbps_dbg_printf(xhp, "package `%s' is obsolete\n", pkgver);
	return true;
}

/*
 * If the package is noarch, check all indexes using `check_obsolete_noarch`,
 * otherwise return true if the repodata version doesn't match
 * the supplied pkgver.
 */
static bool
check_obsolete(struct xbps_handle *xhp, struct repo *repo,
		const char *pkgver, const char *arch)
{
	char name[XBPS_NAME_SIZE];
	struct arch *found = NULL;
	xbps_dictionary_t pkgd;

	if (strcmp(arch, "noarch") == 0)
		return check_obsolete_noarch(xhp, repo, pkgver);

	for (struct arch *a = repo->archs; a; a = a->next)
		if (strcmp(a->arch, arch) == 0)
			found = a;

	if (found == NULL) {
		/* XXX: found package for architecture without repodata, delete? */
		xbps_error_printf("package `%s' with architecture `%s' without repository index\n",
		    pkgver, arch);
		return false;
	}

	if (!xbps_pkg_name(name, sizeof name, pkgver)) {
		/* XXX: delete invalid packages? */
		xbps_error_printf("invalid pkgver: %s\n", pkgver);
		return false;
	}

	if (found->stage != NULL) {
		if ((pkgd = xbps_dictionary_get(found->stage->idx, name)) != NULL) {
			if (same_pkgver(pkgd, pkgver)) {
				xbps_dbg_printf(xhp, "found package "
					"`%s' in `%s/%s-stagedata'\n", pkgver, repo->path,
				    found->arch);
				return false;
			}
		}
	}
	if ((pkgd = xbps_dictionary_get(found->repo->idx, name)) != NULL) {
		if (same_pkgver(pkgd, pkgver)) {
			xbps_dbg_printf(xhp, "found package "
			    "`%s' in `%s/%s-repodata'\n", pkgver, repo->path, found->arch);
			return false;
		}
	}

	xbps_dbg_printf(xhp, "package `%s' is obsolete\n", pkgver);
	return true;
}

static int
purge_repo(struct xbps_handle *xhp, struct repo *repo, bool dry)
{
	char buf[PATH_MAX], path[PATH_MAX];
	const char *pkgver, *arch;
	size_t pathlen = strlen(repo->path);

	xbps_strlcpy(path, repo->path, sizeof path);

	for (int i = 0; i < repo->nnames; i++) {
		const char *name = repo->namelist[i]->d_name;
		char *d;
		xbps_strlcpy(buf, name, sizeof buf);
		if ((d = strrchr(buf, '.')) == NULL)
			continue;
		if (strcmp(d+1, "xbps") != 0)
			continue;
		*d = '\0';
		if ((d = strrchr(buf, '.')) == NULL)
			continue;
		*d = '\0';
		arch = d+1;
		pkgver = buf;

		if (!check_obsolete(xhp, repo, pkgver, arch)) {
			/* the package is not obsolete */
			continue;
		}

		/* reset path to append the file */
		path[pathlen] = '\0';

		if (xbps_path_append(path, sizeof path, name) == -1) {
			xbps_error_printf("path too long");
			exit(1);
		}
		if (dry || xhp->flags & XBPS_FLAG_VERBOSE)
			fprintf(stdout, "removing %s...\n", path);
		if (!dry) {
			if (unlink(path) == -1)
				xbps_error_printf("unlink: %s: %s", path, strerror(errno));
		}

		/* try to remove signature file */
		if (xbps_strlcat(path, ".sig", sizeof path) >= sizeof path) {
			xbps_error_printf("path too long");
			exit(1);
		}
		if (dry || xhp->flags & XBPS_FLAG_VERBOSE)
			fprintf(stdout, "removing %s...\n", path);
		if (!dry) {
			if (unlink(path) == -1 && errno != ENOENT)
				xbps_error_printf("unlink: %s: %s", path, strerror(errno));
		}

	}
	return 0;
}

int
purge_repos(struct xbps_handle *xhp, int argc, char *argv[], bool dry)
{
	for (int i = 0; i < argc; i++)
		add_repo(xhp, argv[i]);
	for (struct repo *repo = repos; repo; repo = repo->next)
		purge_repo(xhp, repo, dry);
	return 0;
}
