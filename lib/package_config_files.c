/*-
 * Copyright (c) 2009-2014 Juan Romero Pardines.
 * Copyright (c) 2021 Piotr Wójcik
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
#include <errno.h>

#include "xbps_api_impl.h"

struct conf_file_contents {
	bool is_link;
	const char *data;
};

static bool
equal(struct conf_file_contents *a, struct conf_file_contents *b) {
	return a->is_link == b->is_link && a->data && b->data && strcmp(a->data, b->data) == 0;
}

static void
read_conf_file(struct conf_file_contents *conf, const char *entry_pname, xbps_dictionary_t filesd) {
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	const char *cffile;
	bool ok;

	iter = xbps_array_iter_from_dict(filesd, "conf_files");
	if (iter) {
		while ((obj = xbps_object_iterator_next(iter))) {
			ok = xbps_dictionary_get_cstring_nocopy(obj, "file", &cffile);
			if (!ok)
				continue;
			if (*entry_pname == '.' && strcmp(entry_pname+1, cffile) == 0) {
				conf->is_link = false;
				xbps_dictionary_get_cstring_nocopy(obj, "sha256", &conf->data);
				break;
			}
		}
		xbps_object_iterator_release(iter);
	}
	iter = xbps_array_iter_from_dict(filesd, "links");
	if (iter) {
		while ((obj = xbps_object_iterator_next(iter))) {
			ok = xbps_dictionary_get_cstring_nocopy(obj, "file", &cffile);
			if (!ok)
				continue;
			if (*entry_pname == '.' && strcmp(entry_pname+1, cffile) == 0) {
				conf->is_link = true;
				xbps_dictionary_get_cstring_nocopy(obj, "target", &conf->data);
				break;
			}
		}
		xbps_object_iterator_release(iter);
	}
}

/*
 * Returns true if entry is a configuration file, false otherwise.
 */
int HIDDEN
xbps_entry_is_a_conf_file(xbps_dictionary_t propsd,
			  const char *entry_pname)
{
	xbps_array_t array;

	array = xbps_dictionary_get(propsd, "conf_files");
	if (xbps_array_count(array) == 0)
		return false;
	return xbps_match_string_in_array(array, entry_pname);
}

/*
 * Returns 1 if entry should be installed, 0 if don't or -1 on error.
 */
int HIDDEN
xbps_entry_install_conf_file(struct xbps_handle *xhp,
			     xbps_dictionary_t binpkg_filesd,
			     xbps_dictionary_t pkg_filesd,
			     struct archive_entry *entry,
			     const char *entry_pname,
			     const char *pkgver,
			     struct stat *st)
{
	const char *version = NULL;
	char *lnk = NULL;
	char buf[PATH_MAX], sha256_cur[PATH_MAX];
	struct conf_file_contents orig = {0}, cur = {0}, new = {0};
	int rv = 0;


	assert(xbps_object_type(binpkg_filesd) == XBPS_TYPE_DICTIONARY);
	assert(entry);
	assert(entry_pname);
	assert(pkgver);

	read_conf_file(&new, entry_pname, binpkg_filesd);

	if (new.data == NULL) {
		goto out;
	}

	/*
	 * Get original hash for the file from current
	 * installed package.
	 */
	xbps_dbg_printf(xhp, "%s: processing conf_file %s\n",
	    pkgver, entry_pname);

	if (pkg_filesd == NULL) {
		/*
		 * File exists on disk but it's not managed by the same package.
		 * Install it as file.new-<version>.
		 */
		version = xbps_pkg_version(pkgver);
		assert(version);
		xbps_dbg_printf(xhp, "%s: conf_file %s not currently "
		    "installed, renaming to %s.new-%s\n", pkgver,
		    entry_pname, entry_pname, version);
		snprintf(buf, sizeof(buf), "%s.new-%s", entry_pname, version);
		xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
		    0, pkgver, "File `%s' exists, installing configuration file to `%s'.", entry_pname, buf);
		archive_entry_copy_pathname(entry, buf);
		rv = 1;
		goto out;
	}

	read_conf_file(&orig, entry_pname, pkg_filesd);
	/*
	 * First case: original hash not found, install new file.
	 */
	if (orig.data == NULL) {
		xbps_dbg_printf(xhp, "%s: conf_file %s not installed\n",
		    pkgver, entry_pname);
		rv = 1;
		goto out;
	}

	/*
	 * Compare original, installed and new hash for current file.
	 */
		if (S_ISLNK(st->st_mode)) {
			const char *file = entry_pname + 1;
			if (strcmp(xhp->rootdir, "/") != 0) {
				snprintf(buf, sizeof(buf), "%s%s", xhp->rootdir, file);
				file = buf;
			}
			cur.is_link = true;
			lnk = xbps_symlink_target(xhp, file, orig.data);
			cur.data = lnk;
			if (!cur.data) {
				/*
				 * File not installed, install new one.
				 */
				rv = 1;
				goto out;
			}
		} else if (!xbps_file_sha256(sha256_cur, sizeof sha256_cur, entry_pname)) {
			if (errno == ENOENT) {
				/*
				 * File not installed, install new one.
				 */
				xbps_dbg_printf(xhp, "%s: conf_file %s not "
				    "installed\n", pkgver, entry_pname);
				rv = 1;
			} else {
				rv = -1;
			}
			goto out;
		} else {
			cur.is_link = false;
			cur.data = sha256_cur;
		}

		/*
		 * Orig = X, Curr = X, New = X
		 *
		 * Keep file as is (no changes).
		 */
		if (equal(&orig, &cur) &&
		    equal(&cur, &new)) {
			xbps_dbg_printf(xhp, "%s: conf_file %s orig = X, "
			    "cur = X, new = X\n", pkgver, entry_pname);
			rv = 0;
		/*
		 * Orig = X, Curr = X, New = Y
		 *
		 * Install new file (installed file hasn't been modified) if
		 * configuration option keepconfig is NOT set.
		 */
		} else if (equal(&orig, &cur) &&
			   !equal(&cur, &new) &&
			   (!(xhp->flags & XBPS_FLAG_KEEP_CONFIG))) {
			xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Updating configuration file `%s' provided "
			    "by `%s'.", entry_pname, pkgver);
			rv = 1;
		/*
		 * Orig = X, Curr = Y, New = X
		 *
		 * Keep installed file as is because it has been modified,
		 * but new package doesn't contain new changes compared
		 * to the original version.
		 */
		} else if (equal(&orig, &new) &&
			   !equal(&orig, &cur)) {
			xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
			    0, pkgver,
			    "Keeping modified configuration file `%s'.",
			    entry_pname);
			rv = 0;
		/*
		 * Orig = X, Curr = Y, New = Y
		 *
		 * Keep file as is because changes made are compatible
		 * with new version.
		 */
		} else if (equal(&cur, &new) &&
			   !equal(&orig, &cur)) {
			xbps_dbg_printf(xhp, "%s: conf_file %s orig = X, "
			    "cur = Y, new = Y\n", pkgver, entry_pname);
			rv = 0;
		/*
		 * Orig = X, Curr = Y, New = Z
		 * or
		 * Orig = X, Curr = X, New = Y if keepconf is set
		 *
		 * Install new file as <file>.new-<version>
		 */
		} else {
			version = xbps_pkg_version(pkgver);
			assert(version);
			snprintf(buf, sizeof(buf), "%s.new-%s", entry_pname, version);
			xbps_set_cb_state(xhp, XBPS_STATE_CONFIG_FILE,
			    0, pkgver, "File `%s' exists, installing configuration file to `%s'.", entry_pname, buf);
			archive_entry_copy_pathname(entry, buf);
			rv = 1;
		}

out:
	free(lnk);

	xbps_dbg_printf(xhp, "%s: conf_file %s returned %d\n",
	    pkgver, entry_pname, rv);

	return rv;
}
