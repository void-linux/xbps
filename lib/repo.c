/*-
 * Copyright (c) 2012-2014 Juan Romero Pardines.
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <fcntl.h>

#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/pem.h>

#include "xbps_api_impl.h"

/**
 * @file lib/repo.c
 * @brief Repository functions
 * @defgroup repo Repository functions
 */
char *
xbps_repo_path(struct xbps_handle *xhp, const char *url)
{
	assert(xhp);
	assert(url);

	return xbps_xasprintf("%s/%s-repodata",
	    url, xhp->target_arch ? xhp->target_arch : xhp->native_arch);
}

static xbps_dictionary_t
repo_get_dict(struct xbps_repo *repo)
{
	xbps_dictionary_t d = NULL;
	struct archive_entry *entry;
	char *adata = NULL;
	const void *buf;
	off_t offset;
	size_t size;
	int rv;

	if (repo->ar == NULL)
		return NULL;

	rv = archive_read_next_header(repo->ar, &entry);
	if (rv != ARCHIVE_OK) {
		xbps_dbg_printf(repo->xhp,
		    "%s: read_next_header %s\n", repo->uri,
		    archive_error_string(repo->ar));
		return NULL;
	}
	for (;;) {
		rv = archive_read_data_block(repo->ar, &buf, &size, &offset);
		if (rv == ARCHIVE_EOF)
			break;
		if (rv != ARCHIVE_OK) {
			if (adata != NULL)
				free(adata);

			xbps_dbg_printf(repo->xhp,
			    "%s: read_data_block %s\n", repo->uri,
			    archive_error_string(repo->ar));
			return NULL;
		}
		if (adata == NULL) {
			adata = malloc(size);
		} else {
			adata = realloc(adata, size+offset);
			if (adata == NULL) {
				free(adata);
				return NULL;
			}
		}
		memcpy(adata+offset, buf, size);
	}
	if (adata != NULL) {
		d = xbps_dictionary_internalize(adata);
		free(adata);
	}
	return d;
}

struct xbps_repo *
xbps_repo_open(struct xbps_handle *xhp, const char *url, bool lock)
{
	struct xbps_repo *repo;
	struct stat st;
	const char *arch;
	char *repofile;

	assert(xhp);
	assert(url);

	if (xhp->target_arch)
		arch = xhp->target_arch;
	else
		arch = xhp->native_arch;

	repo = calloc(1, sizeof(struct xbps_repo));
	assert(repo);
	repo->xhp = xhp;
	repo->uri = url;

	if (xbps_repository_is_remote(url)) {
		/* remote repository */
		char *rpath;

		if ((rpath = xbps_get_remote_repo_string(url)) == NULL) {
			free(repo);
			return NULL;
		}
		repofile = xbps_xasprintf("%s/%s/%s-repodata", xhp->metadir, rpath, arch);
		free(rpath);
		repo->is_remote = true;
	} else {
		/* local repository */
		repofile = xbps_repo_path(xhp, url);
	}

	if (stat(repofile, &st) == -1) {
		xbps_dbg_printf(xhp, "[repo] `%s' stat repodata %s\n",
		    repofile, strerror(errno));
		goto out;
	}
	/*
	 * Open or create the repository archive.
	 */
	if (lock)
		repo->fd = open(repofile, O_CREAT|O_RDWR, 0664);
	else
		repo->fd = open(repofile, O_RDONLY);

	if (repo->fd == -1) {
		xbps_dbg_printf(xhp, "[repo] `%s' open repodata %s\n",
		    repofile, strerror(errno));
		goto out;
	}
	/*
	 * Acquire a POSIX file lock on the archive; wait if the lock is
	 * already taken.
	 */
        if (lock && lockf(repo->fd, F_LOCK, 0) == -1) {
		xbps_dbg_printf(xhp, "[repo] failed to lock %s: %s\n", repo->uri, strerror(errno));
		goto out;
	}

	repo->ar = archive_read_new();
	archive_read_support_compression_gzip(repo->ar);
	archive_read_support_format_tar(repo->ar);

	if (archive_read_open_fd(repo->ar, repo->fd, st.st_blksize) == ARCHIVE_FATAL) {
		xbps_dbg_printf(xhp,
		    "[repo] `%s' failed to open repodata archive %s\n",
		    repofile, strerror(archive_errno(repo->ar)));
		goto out;
	}
	if ((repo->idx = repo_get_dict(repo)) == NULL) {
		xbps_dbg_printf(xhp,
		    "[repo] `%s' failed to internalize index on archive %s: %s\n",
		    url, repofile, strerror(archive_errno(repo->ar)));
		goto out;
	}
	repo->idxmeta = repo_get_dict(repo);
	if (repo->idxmeta != NULL)
		repo->is_signed = true;

	free(repofile);
	return repo;

out:
	if (repo->ar)
		archive_read_free(repo->ar);
	if (repo->fd != -1)
		close(repo->fd);
	free(repofile);
	free(repo);
	return NULL;
}

void
xbps_repo_open_idxfiles(struct xbps_repo *repo)
{
	assert(repo);
	repo->idxfiles = repo_get_dict(repo);
}

void
xbps_repo_close(struct xbps_repo *repo, bool lock)
{
	assert(repo);

	if (repo->ar != NULL)
		archive_read_finish(repo->ar);

	if (repo->idx != NULL) {
		xbps_object_release(repo->idx);
		repo->idx = NULL;
	}
	if (repo->idxmeta != NULL) {
		xbps_object_release(repo->idxmeta);
		repo->idxmeta = NULL;
	}
	if (repo->idxfiles != NULL) {
		xbps_object_release(repo->idxfiles);
		repo->idxfiles = NULL;
	}
        if (lock && lockf(repo->fd, F_ULOCK, 0) == -1)
		xbps_dbg_printf(repo->xhp, "[repo] failed to unlock %s: %s\n", repo->uri, strerror(errno));

	close(repo->fd);
	free(repo);
}

xbps_dictionary_t
xbps_repo_get_virtualpkg(struct xbps_repo *repo, const char *pkg)
{
	xbps_dictionary_t pkgd;

	assert(repo);
	assert(pkg);

	if (repo->idx == NULL)
		return NULL;

	pkgd = xbps_find_virtualpkg_in_dict(repo->xhp, repo->idx, pkg);
	if (pkgd) {
		xbps_dictionary_set_cstring_nocopy(pkgd,
				"repository", repo->uri);
		return pkgd;
	}
	return NULL;
}

xbps_dictionary_t
xbps_repo_get_pkg(struct xbps_repo *repo, const char *pkg)
{
	xbps_dictionary_t pkgd;

	assert(repo);
	assert(pkg);

	if (repo->idx == NULL)
		return NULL;

	pkgd = xbps_find_pkg_in_dict(repo->idx, pkg);
	if (pkgd) {
		xbps_dictionary_set_cstring_nocopy(pkgd,
				"repository", repo->uri);
		return pkgd;
	}

	return NULL;
}

xbps_dictionary_t
xbps_repo_get_pkg_plist(struct xbps_handle *xhp, xbps_dictionary_t pkgd,
		const char *plist)
{
	xbps_dictionary_t bpkgd;
	char *url;

	url = xbps_repository_pkg_path(xhp, pkgd);
	if (url == NULL)
		return NULL;

	bpkgd = xbps_get_pkg_plist_from_binpkg(url, plist);
	free(url);
	return bpkgd;
}

static xbps_array_t
revdeps_match(struct xbps_repo *repo, xbps_dictionary_t tpkgd, const char *str)
{
	xbps_dictionary_t pkgd;
	xbps_array_t revdeps = NULL, pkgdeps, provides;
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *pkgver, *tpkgver, *arch, *vpkg;
	char *buf;

	iter = xbps_dictionary_iterator(repo->idx);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		pkgd = xbps_dictionary_get_keysym(repo->idx, obj);
		if (xbps_dictionary_equals(pkgd, tpkgd))
			continue;

		pkgdeps = xbps_dictionary_get(pkgd, "run_depends");
		if (!xbps_array_count(pkgdeps))
			continue;
		/*
		 * Try to match passed in string.
		 */
		if (str) {
			if (!xbps_match_pkgdep_in_array(pkgdeps, str))
				continue;
			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (!xbps_pkg_arch_match(repo->xhp, arch, NULL))
				continue;

			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "pkgver", &tpkgver);
			/* match */
			if (revdeps == NULL)
				revdeps = xbps_array_create();

			if (!xbps_match_string_in_array(revdeps, tpkgver))
				xbps_array_add_cstring_nocopy(revdeps, tpkgver);

			continue;
		}
		/*
		 * Try to match any virtual package.
		 */
		provides = xbps_dictionary_get(tpkgd, "provides");
		for (unsigned int i = 0; i < xbps_array_count(provides); i++) {
			xbps_array_get_cstring_nocopy(provides, i, &vpkg);
			if (strchr(vpkg, '_') == NULL)
				buf = xbps_xasprintf("%s_1", vpkg);
			else
				buf = strdup(vpkg);

			if (!xbps_match_pkgdep_in_array(pkgdeps, buf)) {
				free(buf);
				continue;
			}
			free(buf);
			xbps_dictionary_get_cstring_nocopy(pkgd,
			    "architecture", &arch);
			if (!xbps_pkg_arch_match(repo->xhp, arch, NULL))
				continue;

			xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver",
			    &tpkgver);
			/* match */
			if (revdeps == NULL)
				revdeps = xbps_array_create();

			if (!xbps_match_string_in_array(revdeps, tpkgver))
				xbps_array_add_cstring_nocopy(revdeps, tpkgver);
		}
		/*
		 * Try to match by pkgver.
		 */
		xbps_dictionary_get_cstring_nocopy(tpkgd, "pkgver", &pkgver);
		if (!xbps_match_pkgdep_in_array(pkgdeps, pkgver))
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd,
		    "architecture", &arch);
		if (!xbps_pkg_arch_match(repo->xhp, arch, NULL))
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgd, "pkgver", &tpkgver);
		/* match */
		if (revdeps == NULL)
			revdeps = xbps_array_create();

		if (!xbps_match_string_in_array(revdeps, tpkgver))
			xbps_array_add_cstring_nocopy(revdeps, tpkgver);
	}
	xbps_object_iterator_release(iter);
	return revdeps;
}

xbps_array_t
xbps_repo_get_pkg_revdeps(struct xbps_repo *repo, const char *pkg)
{
	xbps_array_t revdeps = NULL, vdeps = NULL;
	xbps_dictionary_t pkgd;
	const char *vpkg;
	char *buf = NULL;
	bool match = false;

	if (repo->idx == NULL)
		return NULL;

	if (((pkgd = xbps_rpool_get_pkg(repo->xhp, pkg)) == NULL) &&
	    ((pkgd = xbps_rpool_get_virtualpkg(repo->xhp, pkg)) == NULL)) {
		errno = ENOENT;
		return NULL;
	}
	/*
	 * If pkg is a virtual pkg let's match it instead of the real pkgver.
	 */
	if ((vdeps = xbps_dictionary_get(pkgd, "provides"))) {
		for (unsigned int i = 0; i < xbps_array_count(vdeps); i++) {
			char *vpkgn;

			xbps_array_get_cstring_nocopy(vdeps, i, &vpkg);
			if (strchr(vpkg, '_') == NULL)
				buf = xbps_xasprintf("%s_1", vpkg);
			else
				buf = strdup(vpkg);

			vpkgn = xbps_pkg_name(buf);
			assert(vpkgn);
			if (strcmp(vpkgn, pkg) == 0) {
				free(vpkgn);
				break;
			}
			free(vpkgn);
			free(buf);
			buf = NULL;
		}
		if (buf) {
			match = true;
			revdeps = revdeps_match(repo, pkgd, buf);
			free(buf);
		}
	}
	if (!match)
		revdeps = revdeps_match(repo, pkgd, NULL);

	return revdeps;
}

int
xbps_repo_key_import(struct xbps_repo *repo)
{
	xbps_dictionary_t repokeyd = NULL;
	xbps_data_t pubkey = NULL;
	uint16_t pubkey_size = 0;
	const char *signedby = NULL;
	char *hexfp = NULL;
	char *p, *dbkeyd, *rkeyfile = NULL;
	int import, rv = 0;

	assert(repo);
	/*
	 * If repository does not have required metadata plist, ignore it.
	 */
	if (!xbps_dictionary_count(repo->idxmeta)) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' unsigned repository!\n", repo->uri);
		return 0;
	}
	/*
	 * Check for required objects in index-meta:
	 * 	- signature-by (string)
	 * 	- public-key (data)
	 * 	- public-key-size (number)
	 */
	xbps_dictionary_get_cstring_nocopy(repo->idxmeta, "signature-by", &signedby);
	xbps_dictionary_get_uint16(repo->idxmeta, "public-key-size", &pubkey_size);
	pubkey = xbps_dictionary_get(repo->idxmeta, "public-key");

	if (signedby == NULL || pubkey_size == 0 ||
	    xbps_object_type(pubkey) != XBPS_TYPE_DATA) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s': incomplete signed repository "
		    "(missing objs)\n", repo->uri);
		rv = EINVAL;
		goto out;
	}
	hexfp = xbps_pubkey2fp(repo->xhp, pubkey);
	/*
	 * Check if the public key is alredy stored.
	 */
	rkeyfile = xbps_xasprintf("%s/keys/%s.plist", repo->xhp->metadir, hexfp);
	repokeyd = xbps_dictionary_internalize_from_zfile(rkeyfile);
	if (xbps_object_type(repokeyd) == XBPS_TYPE_DICTIONARY) {
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' public key already stored.\n", repo->uri);
		goto out;
	}
	/*
	 * Notify the client and take appropiate action to import
	 * the repository public key. Pass back the public key openssh fingerprint
	 * to the client.
	 */
	import = xbps_set_cb_state(repo->xhp, XBPS_STATE_REPO_KEY_IMPORT, 0,
			hexfp, "`%s' repository has been RSA signed by \"%s\"",
			repo->uri, signedby);
	if (import <= 0) {
		rv = EAGAIN;
		goto out;
	}

	p = strdup(rkeyfile);
	dbkeyd = dirname(p);
	assert(dbkeyd);
	if (access(dbkeyd, R_OK|W_OK) == -1) {
		rv = errno;
		if (rv == ENOENT)
			rv = xbps_mkpath(dbkeyd, 0755);
		if (rv != 0) {
			rv = errno;
			xbps_dbg_printf(repo->xhp,
			    "[repo] `%s' cannot create %s: %s\n",
			    repo->uri, dbkeyd, strerror(errno));
			free(p);
			goto out;
		}
	}
	free(p);

	repokeyd = xbps_dictionary_create();
	xbps_dictionary_set(repokeyd, "public-key", pubkey);
	xbps_dictionary_set_uint16(repokeyd, "public-key-size", pubkey_size);
	xbps_dictionary_set_cstring_nocopy(repokeyd, "signature-by", signedby);

	if (!xbps_dictionary_externalize_to_zfile(repokeyd, rkeyfile)) {
		rv = errno;
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' failed to externalize %s: %s\n",
		    repo->uri, rkeyfile, strerror(rv));
	}

out:
	if (hexfp)
		free(hexfp);
	if (repokeyd)
		xbps_object_release(repokeyd);
	if (rkeyfile)
		free(rkeyfile);
	return rv;
}
