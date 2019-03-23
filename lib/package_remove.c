/*-
 * Copyright (c) 2009-2015 Juan Romero Pardines.
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
#include <dirent.h>
#include <libgen.h>
#include <fcntl.h>
#include <unistd.h>

#include "xbps_api_impl.h"

static bool
check_remove_pkg_files(struct xbps_handle *xhp,
	xbps_dictionary_t pkgd, const char *pkgver, uid_t euid)
{
	struct stat st;
	xbps_array_t array;
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *objs[] = { "files", "conf_files", "links", "dirs" };
	const char *file;
	char path[PATH_MAX];
	bool fail = false;

	for (uint8_t i = 0; i < __arraycount(objs); i++) {
		array = xbps_dictionary_get(pkgd, objs[i]);
		if (array == NULL || xbps_array_count(array) == 0)
			continue;

		iter = xbps_array_iter_from_dict(pkgd, objs[i]);
		if (iter == NULL)
			continue;

		while ((obj = xbps_object_iterator_next(iter))) {
			xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
			snprintf(path, sizeof(path), "%s/%s", xhp->rootdir, file);
			/*
			 * Check if effective user ID owns the file; this is
			 * enough to ensure the user has write permissions
			 * on the directory.
			 */
			if (euid == 0 || (!lstat(path, &st) && euid == st.st_uid)) {
				/* success */
				continue;
			}
			if (errno != ENOENT) {
				/*
				 * only bail out if something else than ENOENT
				 * is returned.
				 */
				int rv = errno;
				if (rv == 0) {
					/* lstat succeeds but euid != uid */
					rv = EPERM;
				}
				fail = true;
				xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FILE_FAIL,
				    rv, pkgver,
				    "%s: cannot remove `%s': %s",
				    pkgver, file, strerror(rv));
			}
			errno = 0;
		}
		xbps_object_iterator_release(iter);
	}

	return fail;
}

struct order_length_t {
	unsigned int pos;
	size_t len;
};

static int cmp_order_length(const void *l1, const void *l2) {
	return ((const struct order_length_t*)l1)->len <
	       ((const struct order_length_t*)l2)->len;
}

static int
remove_pkg_files(struct xbps_handle *xhp,
		 xbps_dictionary_t dict,
		 const char *key,
		 const char *pkgver)
{
	xbps_array_t array, dirs;
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *curobj = NULL;
	/* These are symlinks in Void and must not be removed */
	const char *basesymlinks[] = {
		"/bin",
		"/sbin",
		"/usr/sbin",
		"/lib",
		"/lib32",
		"/lib64",
		"/usr/lib32",
		"/usr/lib64",
		"/var/run",
	};
	int rv = 0;

	assert(xbps_object_type(dict) == XBPS_TYPE_DICTIONARY);
	assert(key != NULL);

	array = xbps_dictionary_get(dict, key);
	if (xbps_array_count(array) == 0)
		return 0;

	iter = xbps_array_iter_from_dict(dict, key);
	if (iter == NULL)
		return ENOMEM;

	if (strcmp(key, "files") == 0)
		curobj = "file";
	else if (strcmp(key, "conf_files") == 0)
		curobj = "configuration file";
	else if (strcmp(key, "links") == 0)
		curobj = "link";
	else if (strcmp(key, "dirs") == 0)
		curobj = "directory";

	xbps_object_iterator_reset(iter);

	/*
	 * directories must be ordered before removal
	 */
	if (strcmp(key, "dirs") == 0) {
		unsigned int i = 0;
		unsigned int n = xbps_array_count(array);
		struct order_length_t *lengths =
		  (struct order_length_t*) malloc(sizeof(struct order_length_t) * n);

		if (lengths == NULL)
			return ENOMEM;

		while ((obj = xbps_object_iterator_next(iter))) {
			const char *file;
			xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
			lengths[i].len = strlen(file);
			lengths[i].pos = i;
			i++;
		}

		qsort(lengths, n, sizeof(struct order_length_t), cmp_order_length);
		dirs = xbps_array_create_with_capacity(n);

		for (i = 0; i < n; i++) {
			xbps_array_add(dirs, xbps_array_get(array, lengths[i].pos));
		}

		xbps_object_iterator_release(iter);
		iter = xbps_array_iterator(dirs);
		free(lengths);
	}

	xbps_object_iterator_reset(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		const char *file, *sha256;
		char path[PATH_MAX];
		bool found;

		xbps_dictionary_get_cstring_nocopy(obj, "file", &file);
		snprintf(path, sizeof(path), "%s/%s", xhp->rootdir, file);

		if ((strcmp(key, "files") == 0) ||
		    (strcmp(key, "conf_files") == 0)) {
			/*
			 * Check SHA256 hash in regular files and
			 * configuration files.
			 */
			xbps_dictionary_get_cstring_nocopy(obj,
			    "sha256", &sha256);
			rv = xbps_file_hash_check(path, sha256);
			if (rv == ENOENT) {
				/* missing file, ignore it */
				xbps_set_cb_state(xhp,
				    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
				    rv, pkgver,
				    "%s: failed to check hash for %s `%s': %s",
				    pkgver, curobj, file, strerror(rv));
				rv = 0;
				continue;
			} else if (rv == ERANGE) {
				rv = 0;
				if ((xhp->flags &
				    XBPS_FLAG_FORCE_REMOVE_FILES) == 0) {
					xbps_set_cb_state(xhp,
					    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
					    0, pkgver,
					    "%s: %s `%s' SHA256 mismatch, "
					    "preserving file", pkgver,
					    curobj, file);
					continue;
				} else {
					xbps_set_cb_state(xhp,
					    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
					    0, pkgver,
					    "%s: %s `%s' SHA256 mismatch, "
					    "forcing removal", pkgver,
					    curobj, file);
				}
			} else if (rv != 0 && rv != ERANGE) {
				xbps_set_cb_state(xhp,
				    XBPS_STATE_REMOVE_FILE_HASH_FAIL,
				    rv, pkgver,
				    "%s: [remove] failed to check hash for "
				    "%s `%s': %s", pkgver, curobj, file,
				    strerror(rv));
				break;
			}
		}
		/*
		 * Make sure to not remove any symlink of root directory.
		 */
		found = false;
		for (uint8_t i = 0; i < __arraycount(basesymlinks); i++) {
			if (strcmp(file, basesymlinks[i]) == 0) {
				found = true;
				xbps_dbg_printf(xhp, "[remove] %s ignoring "
				    "%s removal\n", pkgver, file);
				break;
			}
		}
		if (found) {
			continue;
		}
		if (strcmp(key, "links") == 0) {
			const char *target = NULL;
			char *lnk;

			xbps_dictionary_get_cstring_nocopy(obj, "target", &target);
			assert(target);
			lnk = xbps_symlink_target(xhp, path, target);
			if (lnk == NULL) {
				xbps_dbg_printf(xhp, "[remove] %s "
				    "symlink_target: %s\n", path, strerror(errno));
				continue;
			}
			if (strcmp(lnk, target)) {
				xbps_dbg_printf(xhp, "[remove] %s symlink "
				    "modified (stored %s current %s)\n", path,
				    target, lnk);
				if ((xhp->flags & XBPS_FLAG_FORCE_REMOVE_FILES) == 0) {
					free(lnk);
					continue;
				}
			}
			free(lnk);
		}
		/*
		 * Remove the object if possible.
		 */
		if (remove(path) == -1) {
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FILE_FAIL,
			    errno, pkgver,
			    "%s: failed to remove %s `%s': %s", pkgver,
			    curobj, file, strerror(errno));
		} else {
			/* success */
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FILE,
			    0, pkgver, "Removed %s `%s'", curobj, file);
		}
	}
	xbps_object_iterator_release(iter);

	return rv;
}

int HIDDEN
xbps_remove_pkg(struct xbps_handle *xhp, const char *pkgver, bool update)
{
	xbps_dictionary_t pkgd = NULL, pkgfilesd = NULL;
	char *pkgname, metafile[PATH_MAX];
	int rv = 0;
	pkg_state_t state = 0;
	uid_t euid;

	assert(xhp);
	assert(pkgver);

	pkgname = xbps_pkg_name(pkgver);
	assert(pkgname);

	euid = geteuid();

	if ((pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
		rv = errno;
		xbps_dbg_printf(xhp, "[remove] cannot find %s in pkgdb: %s\n",
		    pkgver, strerror(rv));
		goto out;
	}
	if ((rv = xbps_pkg_state_dictionary(pkgd, &state)) != 0) {
		xbps_dbg_printf(xhp, "[remove] cannot find %s in pkgdb: %s\n",
		    pkgver, strerror(rv));
		goto out;
	}
	xbps_dbg_printf(xhp, "attempting to remove %s state %d\n", pkgver, state);

	if (!update)
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE, 0, pkgver, NULL);

	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    rv, pkgver,
		   "%s: [remove] failed to chdir to rootdir `%s': %s",
		    pkgver, xhp->rootdir, strerror(rv));
		goto out;
	}

	/* internalize pkg files dictionary from metadir */
	snprintf(metafile, sizeof(metafile), "%s/.%s-files.plist", xhp->metadir, pkgname);
	pkgfilesd = xbps_plist_dictionary_from_file(xhp, metafile);
	if (pkgfilesd == NULL)
		xbps_dbg_printf(xhp, "WARNING: metaplist for %s "
		    "doesn't exist!\n", pkgver);

	/* If package was "half-removed", remove it fully. */
	if (state == XBPS_PKG_STATE_HALF_REMOVED)
		goto purge;
	/*
	 * Run the pre remove action and show pre-remove message if exists.
	 */
	rv = xbps_pkg_exec_script(xhp, pkgd, "remove-script", "pre", update);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    errno, pkgver,
		    "%s: [remove] REMOVE script failed to "
		    "execute pre ACTION: %s",
		    pkgver, strerror(rv));
		goto out;
	}
	/* show remove-msg if exists */
	if ((rv = xbps_cb_message(xhp, pkgd, "remove-msg")) != 0)
		goto out;

	/* unregister alternatives */
	if (update)
		xbps_dictionary_set_bool(pkgd, "alternatives-update", true);

	if ((rv = xbps_alternatives_unregister(xhp, pkgd)) != 0)
		goto out;

	/*
	 * If updating a package, we just need to execute the current
	 * pre-remove action target and we are done. Its files will be
	 * overwritten later in unpack phase.
	 */
	if (update) {
		free(pkgname);
		return 0;
	}

	if (pkgfilesd) {
		/*
		 * Do the removal in 2 phases:
		 * 	1- check if user has enough perms to remove all entries
		 * 	2- perform removal
		 */
		if (check_remove_pkg_files(xhp, pkgfilesd, pkgver, euid)) {
			rv = EPERM;
			goto out;
		}
		/* Remove links */
		if ((rv = remove_pkg_files(xhp, pkgfilesd, "links", pkgver)) != 0)
			goto out;
		/* Remove regular files */
		if ((rv = remove_pkg_files(xhp, pkgfilesd, "files", pkgver)) != 0)
			goto out;
		/* Remove configuration files */
		if ((rv = remove_pkg_files(xhp, pkgfilesd, "conf_files", pkgver)) != 0)
			goto out;
		/* Remove dirs */
		if ((rv = remove_pkg_files(xhp, pkgfilesd, "dirs", pkgver)) != 0)
			goto out;
	}
	/*
	 * Execute the post REMOVE action if file exists and we aren't
	 * updating the package.
	 */
	rv = xbps_pkg_exec_script(xhp, pkgd, "remove-script", "post", false);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    rv, pkgver,
		    "%s: [remove] REMOVE script failed to execute "
		    "post ACTION: %s", pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Set package state to "half-removed".
	 */
	rv = xbps_set_pkg_state_installed(xhp, pkgver,
	     XBPS_PKG_STATE_HALF_REMOVED);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    rv, pkgver,
		    "%s: [remove] failed to set state to half-removed: %s",
		    pkgver, strerror(rv));
		goto out;
	}

purge:
	/*
	 * Execute the purge REMOVE action if file exists.
	 */
	rv = xbps_pkg_exec_script(xhp, pkgd, "remove-script", "purge", false);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
		    rv, pkgver,
		    "%s: REMOVE script failed to execute "
		    "purge ACTION: %s", pkgver, strerror(rv));
		goto out;
	}
	/*
	 * Remove package metadata plist.
	 */
	if (remove(metafile) == -1) {
		if (errno != ENOENT) {
			xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL,
			    rv, pkgver,
			    "%s: failed to remove metadata file: %s",
			    pkgver, strerror(errno));
		}
	}
	/*
	 * Unregister package from pkgdb.
	 */
	xbps_dictionary_remove(xhp->pkgdb, pkgname);
	xbps_dbg_printf(xhp, "[remove] unregister %s returned %d\n", pkgver, rv);
	xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_DONE, 0, pkgver, NULL);
out:
	if (pkgname != NULL)
		free(pkgname);
	if (rv != 0) {
		xbps_set_cb_state(xhp, XBPS_STATE_REMOVE_FAIL, rv, pkgver,
		    "%s: failed to remove package: %s", pkgver, strerror(rv));
	}

	return rv;
}
