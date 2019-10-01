/*-
 * Copyright (c) 2012-2015 Juan Romero Pardines.
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
	return xbps_repo_path_with_name(xhp, url, "repodata");
}

char *
xbps_repo_path_with_name(struct xbps_handle *xhp, const char *url, const char *name)
{
	assert(xhp);
	assert(url);
	assert(strcmp(name, "repodata") == 0 || strcmp(name, "stagedata") == 0);

	return xbps_xasprintf("%s/%s-%s",
	    url, xhp->target_arch ? xhp->target_arch : xhp->native_arch, name);
}

static int
repo_verify_index(struct xbps_repo *repo, xbps_dictionary_t idxmeta, unsigned char *digest) {
	bool verified = false;
	unsigned char *sig_buf = NULL;
	size_t sigfilelen = 0;
	struct archive_entry *entry;

	if (archive_read_next_header(repo->ar, &entry) != ARCHIVE_OK) {
		xbps_dbg_printf(repo->xhp,
		    "%s: read_next_header %s\n", repo->uri,
		    archive_error_string(repo->ar));
		return ENOENT;
	}

	if (strcmp(archive_entry_pathname(entry), XBPS_REPOIDXMETA_SIG) != 0) {
		xbps_dbg_printf(repo->xhp,
		    "%s: no signature of %s\n", repo->uri, XBPS_REPOIDX_META);
		return ENOENT;
	}

	sigfilelen = (size_t)archive_entry_size(entry);
	sig_buf = (unsigned char *) xbps_archive_get_file(repo->ar, entry);
	if (sig_buf == NULL) {
		return EIO;
	}
	verified = xbps_verify_digest_signature(repo, idxmeta, sig_buf, sigfilelen, digest);

	free(sig_buf);
	return verified ? 0 : EINVAL;
}

static xbps_dictionary_t
repo_get_dict(struct xbps_repo *repo, int *verify_error)
{
	struct archive_entry *entry;
	int rv;
	xbps_dictionary_t dict, idxmeta;
	char *bytes = NULL;
	unsigned char *digest = NULL;

	if (verify_error != NULL)
		*verify_error = -1;

	if (repo->ar == NULL)
		return NULL;

	rv = archive_read_next_header(repo->ar, &entry);
	if (rv != ARCHIVE_OK) {
		xbps_dbg_printf(repo->xhp,
		    "%s: read_next_header %s\n", repo->uri,
		    archive_error_string(repo->ar));
		return NULL;
	}
	dict = xbps_archive_get_dictionary(repo->ar, entry, &bytes);
	idxmeta = (repo->idxmeta != NULL) ? repo->idxmeta : dict;
	if (verify_error != NULL && bytes != NULL) {
		digest = xbps_buffer_hash_raw(bytes, strlen(bytes));
		*verify_error = repo_verify_index(repo, idxmeta, digest);
	}
	free(digest);
	free(bytes);
	return dict;
}


xbps_dictionary_t HIDDEN
get_safe_idxmeta(xbps_dictionary_t full) {
	static const char *keys[] = {
		"public-key",
		"public-key-size",
		"signature-by",
		"signature-type",
	};
	static const unsigned fields_count = (sizeof keys)/(sizeof *keys);
	xbps_dictionary_t safe = NULL;

	if (full == NULL){
		return NULL;
	}

	safe = xbps_dictionary_create();

	for (unsigned i = 0; i < fields_count; ++i) {
		const char *key = keys[i];
		xbps_object_t value = xbps_dictionary_get(full, key);
		if (value != NULL) {
			xbps_dictionary_set(safe, key, value);
		}
	}

	return safe;
}

bool
xbps_repo_lock(struct xbps_handle *xhp, const char *repodir,
		int *lockfd, char **lockfname)
{
	char *repofile, *lockfile;
	int fd, rv;

	assert(repodir);
	assert(lockfd);
	assert(lockfname);

	repofile = xbps_repo_path(xhp, repodir);
	assert(repofile);

	lockfile = xbps_xasprintf("%s.lock", repofile);
	free(repofile);

	for (;;) {
		fd = open(lockfile, O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC, 0660);
		rv = errno;
		if (fd != -1)
			break;
		if (rv != EEXIST) {
			xbps_dbg_printf(xhp, "[repo] `%s' failed to "
			    "create lock file %s\n", lockfile, strerror(rv));
			free(lockfile);
			return false;
		} else {
			xbps_dbg_printf(xhp, "[repo] `%s' lock file exists,"
			    "waiting for 1s...\n", lockfile);
			sleep(1);
		}
	}
	*lockfname = lockfile;
	*lockfd = fd;
	return true;
}

void
xbps_repo_unlock(int lockfd, char *lockfname)
{
        if (lockfd != -1) {
		close(lockfd);
	}
	if (lockfname) {
		unlink(lockfname);
		free(lockfname);
	}
}

static bool
repo_open_local(struct xbps_repo *repo, const char *repofile)
{
	struct stat st;
	int rv = 0;
	int verify_error = -1;
	const char *signature_type = NULL;
	xbps_dictionary_t idxmeta = NULL;

	if (fstat(repo->fd, &st) == -1) {
		rv = errno;
		xbps_dbg_printf(repo->xhp, "[repo] `%s' fstat repodata %s\n",
		    repofile, strerror(rv));
		return false;
	}

	repo->ar = archive_read_new();
	archive_read_support_filter_gzip(repo->ar);
	archive_read_support_filter_bzip2(repo->ar);
	archive_read_support_filter_xz(repo->ar);
	archive_read_support_filter_lz4(repo->ar);
	archive_read_support_filter_zstd(repo->ar);
	archive_read_support_format_tar(repo->ar);

	if (archive_read_open_fd(repo->ar, repo->fd, st.st_blksize) == ARCHIVE_FATAL) {
		rv = archive_errno(repo->ar);
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' failed to open repodata archive %s\n",
		    repofile, strerror(rv));
		return false;
	}
	if ((repo->idx = repo_get_dict(repo, NULL)) == NULL) {
		xbps_dbg_printf(repo->xhp, "[repo] `%s' failed to internalize "
		    " index on archive, removing file.\n", repofile);
		/* broken archive, remove it */
		(void)unlink(repofile);
		return false;
	}
	xbps_dictionary_make_immutable(repo->idx);
	idxmeta = repo_get_dict(repo, &verify_error);
	if (idxmeta != NULL) {
		if (verify_error == ENOENT) {
			idxmeta = get_safe_idxmeta(idxmeta);
			verify_error = 0;
		}
		if (xbps_dictionary_get_cstring_nocopy(idxmeta, "signature-type", &signature_type))
			repo->is_signed = true;
		xbps_dictionary_make_immutable(idxmeta);
	}
	repo->idxmeta = idxmeta;

	return !verify_error || (!repo->is_remote && !repo->is_signed);
}

static bool
repo_open_remote(struct xbps_repo *repo)
{
	char *rpath;
	bool rv;

	rpath = xbps_repo_path(repo->xhp, repo->uri);
	rv = xbps_repo_fetch_remote(repo, rpath);
	free(rpath);
	if (rv) {
		xbps_dbg_printf(repo->xhp, "[repo] `%s' used remotely (kept in memory).\n", repo->uri);
		if (repo->xhp->state_cb && xbps_repo_key_import(repo) != 0)
			rv = false;
	}
	return rv;
}

static struct xbps_repo *
repo_open_with_type(struct xbps_handle *xhp, const char *url, const char *name)
{
	struct xbps_repo *repo;
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
	repo->fd = -1;
	repo->xhp = xhp;
	repo->uri = url;

	if (xbps_repository_is_remote(url)) {
		/* remote repository */
		char *rpath;

		if ((rpath = xbps_get_remote_repo_string(url)) == NULL) {
			free(repo);
			return NULL;
		}
		repofile = xbps_xasprintf("%s/%s/%s-%s", xhp->metadir, rpath, arch, name);
		free(rpath);
		repo->is_remote = true;
	} else {
		/* local repository */
		repofile = xbps_repo_path_with_name(xhp, url, name);
	}
	/*
	 * In memory repo sync.
	 */
	if (repo->is_remote && (xhp->flags & XBPS_FLAG_REPOS_MEMSYNC)) {
		if (repo_open_remote(repo))
			return repo;

		goto out;
	}
	/*
	 * Open the repository archive.
	 */
	repo->fd = open(repofile, O_RDONLY|O_CLOEXEC);
	if (repo->fd == -1) {
		int rv = errno;
		xbps_dbg_printf(xhp, "[repo] `%s' open %s %s\n",
		    repofile, name, strerror(rv));
		goto out;
	}
	if (repo_open_local(repo, repofile)) {
		free(repofile);
		return repo;
	}

out:
	free(repofile);
	xbps_repo_close(repo);
	return NULL;
}

bool
xbps_repo_store(struct xbps_handle *xhp, const char *repo)
{
	char *url = NULL;

	assert(xhp);
	assert(repo);

	if (xhp->repositories == NULL) {
		xhp->repositories = xbps_array_create();
		assert(xhp->repositories);
	}
	/*
	 * If it's a local repo and path is relative, make it absolute.
	 */
	if (!xbps_repository_is_remote(repo)) {
		if (repo[0] != '/' && repo[0] != '\0') {
			if ((url = realpath(repo, NULL)) == NULL)
				xbps_dbg_printf(xhp, "[repo] %s: realpath %s\n", __func__, repo);
		}
	}
	if (xbps_match_string_in_array(xhp->repositories, url ? url : repo)) {
		xbps_dbg_printf(xhp, "[repo] `%s' already stored\n", url ? url : repo);
		if (url)
			free(url);
		return false;
	}
	if (xbps_array_add_cstring(xhp->repositories, url ? url : repo)) {
		xbps_dbg_printf(xhp, "[repo] `%s' stored successfully\n", url ? url : repo);
		if (url)
			free(url);
		return true;
	}
	if (url)
		free(url);

	return false;
}

bool
xbps_repo_remove(struct xbps_handle *xhp, const char *repo)
{
	char *url;
	bool rv = false;

	assert(xhp);
	assert(repo);

	if (xhp->repositories == NULL)
		return false;

	url = strdup(repo);
	if (xbps_remove_string_from_array(xhp->repositories, repo)) {
		if (url)
			xbps_dbg_printf(xhp, "[repo] `%s' removed\n", url);
		rv = true;
	}
	free(url);

	return rv;
}

struct xbps_repo *
xbps_repo_stage_open(struct xbps_handle *xhp, const char *url)
{
	return repo_open_with_type(xhp, url, "stagedata");
}

struct xbps_repo *
xbps_repo_public_open(struct xbps_handle *xhp, const char *url) {
	return repo_open_with_type(xhp, url, "repodata");
}

struct xbps_repo *
xbps_repo_open(struct xbps_handle *xhp, const char *url)
{
	struct xbps_repo *repo = xbps_repo_public_open(xhp, url);
	struct xbps_repo *stage = NULL;
	xbps_dictionary_t idx;
	const char *pkgname;
	xbps_object_t keysym;
	xbps_object_iterator_t iter;
	/*
	 * Load and merge staging repository if the repository is local.
	 */
	if (repo && !repo->is_remote) {
		stage = xbps_repo_stage_open(xhp, url);
		if (stage == NULL)
			return repo;
		idx = xbps_dictionary_copy_mutable(repo->idx);
		iter = xbps_dictionary_iterator(stage->idx);
		while ((keysym = xbps_object_iterator_next(iter))) {
			pkgname = xbps_dictionary_keysym_cstring_nocopy(keysym);
			xbps_dictionary_set(idx, pkgname,
					xbps_dictionary_get_keysym(stage->idx, keysym));
		}
		xbps_object_iterator_release(iter);
		xbps_object_release(repo->idx);
		xbps_repo_close(stage);
		repo->idx = idx;
		return repo;
	}
	return repo;
}

void
xbps_repo_close(struct xbps_repo *repo)
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
	if (repo->fd != -1)
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

	/* Try matching vpkg from configuration files */
	if ((pkgd = xbps_find_virtualpkg_in_conf(repo->xhp, repo->idx, pkg)))
		return pkgd;

	/* ... otherwise match a real pkg */
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

	bpkgd = xbps_archive_fetch_plist(url, plist);
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
	const char *pkgver = NULL, *tpkgver = NULL, *arch = NULL, *vpkg = NULL;

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
			if (!xbps_match_pkgdep_in_array(pkgdeps, vpkg))
				continue;

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
	bool match = false;

	if (repo->idx == NULL)
		return NULL;

	if (((pkgd = xbps_repo_get_pkg(repo, pkg)) == NULL) &&
	    ((pkgd = xbps_repo_get_virtualpkg(repo, pkg)) == NULL)) {
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
			vpkgn = xbps_pkg_name(vpkg);
			assert(vpkgn);
			if (strcmp(vpkgn, pkg) == 0) {
				free(vpkgn);
				match = true;
				break;
			}
			free(vpkgn);
			vpkg = NULL;
		}
		if (match)
			revdeps = revdeps_match(repo, pkgd, vpkg);
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
	bool has_signedby, has_pubkey_size, has_pubkey;

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

	has_signedby = (signedby != NULL);
	has_pubkey_size = (pubkey_size > 0);
	has_pubkey = (xbps_object_type(pubkey) == XBPS_TYPE_DATA);

	if (!has_signedby && !has_pubkey_size && !has_pubkey)
	{
		xbps_dbg_printf(repo->xhp,
		    "[repo] `%s' unsigned repository with meta!\n", repo->uri);
		return 0;
	}
	else if (!has_signedby || !has_pubkey_size || !has_pubkey)
	{
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
	repokeyd = xbps_plist_dictionary_from_file(repo->xhp, rkeyfile);
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

	if (!xbps_dictionary_externalize_to_file(repokeyd, rkeyfile)) {
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
