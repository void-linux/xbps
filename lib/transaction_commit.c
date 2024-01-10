/*-
 * Copyright (c) 2009-2020 Juan Romero Pardines.
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
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>

#include "xbps_api_impl.h"

/**
 * @file lib/transaction_commit.c
 * @brief Transaction handling routines
 * @defgroup transaction Transaction handling functions
 *
 * The following image shows off the full transaction dictionary returned
 * by xbps_transaction_prepare().
 *
 * @image html images/xbps_transaction_dictionary.png
 *
 * Legend:
 *  - <b>Salmon bg box</b>: The transaction dictionary.
 *  - <b>White bg box</b>: mandatory objects.
 *  - <b>Grey bg box</b>: optional objects.
 *  - <b>Green bg box</b>: possible value set in the object, only one of them
 *    will be set.
 *
 * Text inside of white boxes are the key associated with the object, its
 * data type is specified on its edge, i.e string, array, integer, dictionary.
 */

static int
run_post_remove_scripts(struct xbps_handle *xhp, xbps_array_t remove_scripts)
{
	int rv = 0;

	for (unsigned int i = 0; i < xbps_array_count(remove_scripts); i++) {
		xbps_dictionary_t dict;
		bool update = false;
		xbps_data_t script = NULL;
		const char *pkgver = NULL;
		const void *buf;
		size_t buflen;

		dict = xbps_array_get(remove_scripts, i);
		assert(dict);

		xbps_dictionary_get_cstring_nocopy(dict, "pkgver", &pkgver);
		assert(pkgver);

		xbps_dictionary_get_bool(dict, "update", &update);

		script = xbps_dictionary_get(dict, "remove-script");
		assert(script);

		buf = xbps_data_data_nocopy(script);
		buflen = xbps_data_size(script);
		rv = xbps_pkg_exec_buffer(xhp, buf, buflen, pkgver, "post", update);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL, rv, pkgver,
			    "%s: [trans] REMOVE script failed to execute pre ACTION: %s",
			    pkgver, strerror(rv));
			goto out;
		}
		rv = xbps_pkg_exec_buffer(xhp, buf, buflen, pkgver, "purge", update);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL, rv, pkgver,
			    "%s: [trans] REMOVE script failed to execute pre ACTION: %s",
			    pkgver, strerror(rv));
			goto out;
		}
	}

out:
	return rv;
}

int
xbps_transaction_commit(struct xbps_handle *xhp)
{
	xbps_array_t remove_scripts;
	xbps_dictionary_t pkgdb_pkgd;
	xbps_object_t obj;
	xbps_object_iterator_t iter;
	xbps_trans_type_t ttype;
	const char *pkgver = NULL, *pkgname = NULL;
	int rv = 0;
	bool update;

	setlocale(LC_ALL, "");

	/*
	 * Store remove scripts and pkgver in this array so
	 * that we can run them after the package has been removed
	 * from the package database.
	 */
	remove_scripts = xbps_array_create();
	if (remove_scripts == NULL)
		return errno ? errno : ENOMEM;

	assert(xbps_object_type(xhp->transd) == XBPS_TYPE_DICTIONARY);
	/*
	 * Create cachedir if necessary.
	 */
	if (xbps_mkpath(xhp->cachedir, 0755) == -1) {
		if (errno != EEXIST) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL,
			    errno, NULL,
			    "[trans] cannot create cachedir `%s': %s",
			    xhp->cachedir, strerror(errno));
			return errno;
		}
	}
	if (chdir(xhp->cachedir) == -1) {
		xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL,
		    errno, NULL,
		    "[trans] failed to change dir to cachedir `%s': %s",
		    xhp->cachedir, strerror(errno));
		return errno;
	}
	iter = xbps_array_iter_from_dict(xhp->transd, "packages");
	if (iter == NULL)
		return EINVAL;

	/*
	 * Download and verify binary packages.
	 */
	if ((rv = xbps_transaction_fetch(xhp, iter)) != 0) {
		xbps_dbg_printf("[trans] failed to fetch and verify binpkgs: "
		    "%s\n", strerror(rv));
		goto out;
	}
	if (xhp->flags & XBPS_FLAG_DOWNLOAD_ONLY) {
		goto out;
	}

	/*
	 * After all downloads are finished, clear the connection cache
	 * to avoid file descriptor leaks (see #303)
	 */
	xbps_fetch_unset_cache_connection();

	/*
	 * Internalize metadata of downloaded binary packages.
	 */
	if ((rv = xbps_transaction_internalize(xhp, iter)) < 0) {
		xbps_dbg_printf("[trans] failed to internalize transaction binpkgs: "
		    "%s\n", strerror(-rv));
		goto out;
	}

	/*
	 * Collect files in the transaction and find some issues
	 * like multiple packages installing the same file.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FILES, 0, NULL, NULL);
	if ((rv = xbps_transaction_files(xhp, iter)) != 0) {
		xbps_dbg_printf("[trans] failed to verify transaction files: "
		    "%s\n", strerror(rv));
		goto out;
	}

	/*
	 * Install, update, configure or remove packages as specified
	 * in the transaction dictionary.
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_RUN, 0, NULL, NULL);

	/*
	 * Create rootdir if necessary.
	 */
	if (xbps_mkpath(xhp->rootdir, 0750) == -1) {
		rv = errno;
		if (rv != EEXIST) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL, errno, xhp->rootdir,
			    "[trans] failed to create rootdir `%s': %s",
			    xhp->rootdir, strerror(rv));
			goto out;
		}
	}
	if (chdir(xhp->rootdir) == -1) {
		rv = errno;
		xbps_set_cb_state(xhp, XBPS_STATE_UNPACK_FAIL, rv, xhp->rootdir,
		    "[trans] failed to chdir to rootdir `%s': %s",
		    xhp->rootdir, strerror(errno));
		goto out;
	}

	/*
	 * Run all pre-remove scripts.
	 * And store the remove scripts in remove_scripts
	 * so we can execute the post and purge actions
	 * after the package is removed from the pkgdb.
	 */
	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_t dict;
		xbps_data_t script = NULL;
		const char *pkgdb_pkgver;

		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		xbps_dictionary_get_cstring_nocopy(obj, "pkgname", &pkgname);

		ttype = xbps_transaction_pkg_type(obj);
		if (ttype == XBPS_TRANS_INSTALL || ttype == XBPS_TRANS_HOLD || ttype == XBPS_TRANS_CONFIGURE) {
			xbps_dbg_printf("%s: skipping pre-remove script for "
			    "%s: %d\n", __func__, pkgver, ttype);
			continue;
		}

		if ((pkgdb_pkgd = xbps_pkgdb_get_pkg(xhp, pkgname)) == NULL) {
			rv = ENOENT;
			xbps_dbg_printf("[trans] cannot find %s in pkgdb: %s\n",
			    pkgname, strerror(rv));
			goto out;
		}

		script = xbps_dictionary_get(pkgdb_pkgd, "remove-script");
		if (script == NULL)
			continue;

		xbps_dictionary_get_cstring_nocopy(pkgdb_pkgd, "pkgver", &pkgdb_pkgver);
		assert(pkgdb_pkgver);

		update = ttype == XBPS_TRANS_UPDATE;

		dict = xbps_dictionary_create();
		if (dict == NULL) {
			rv = errno ? errno : ENOMEM;
			goto out;
		}
		if (!xbps_dictionary_set_cstring(dict, "pkgver", pkgdb_pkgver)) {
			rv = errno ? errno : ENOMEM;
			goto out;
		}
		if (!xbps_dictionary_set_bool(dict, "update", update)) {
			rv = errno ? errno : ENOMEM;
			goto out;
		}
		if (!xbps_dictionary_set(dict, "remove-script", script)) {
			rv = errno ? errno : ENOMEM;
			goto out;
		}
		if (!xbps_array_add(remove_scripts, dict)) {
			rv = errno ? errno : ENOMEM;
			goto out;
		}
		xbps_object_release(dict);
		rv = xbps_pkg_exec_script(xhp, pkgdb_pkgd, "remove-script", "pre", update);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL, rv, pkgver,
			    "%s: [trans] REMOVE script failed to execute pre ACTION: %s",
			    pkgver, strerror(rv));
			goto out;
		}
	}
	xbps_object_iterator_reset(iter);

	/*
	 * Run all pre-install scripts.
	 */
	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		ttype = xbps_transaction_pkg_type(obj);
		if (ttype == XBPS_TRANS_REMOVE || ttype == XBPS_TRANS_HOLD) {
			xbps_dbg_printf("%s: skipping pre-install script for "
			    "%s: %d\n", __func__, pkgver, ttype);
			continue;
		}
		rv = xbps_pkg_exec_script(xhp, obj, "install-script", "pre", ttype == XBPS_TRANS_UPDATE);
		if (rv != 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_TRANS_FAIL, rv, pkgver,
			    "%s: [trans] INSTALL script failed to execute pre ACTION: %s",
			    pkgver, strerror(rv));
			goto out;
		}
	}
	xbps_object_iterator_reset(iter);


	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);

		ttype = xbps_transaction_pkg_type(obj);
		if (ttype == XBPS_TRANS_REMOVE) {
			/*
			 * Remove package.
			 */
			update = false;
			xbps_dictionary_get_bool(obj, "remove-and-update", &update);
			rv = xbps_remove_pkg(xhp, pkgver, update);
			if (rv != 0) {
				xbps_dbg_printf("[trans] failed to "
				    "remove %s: %s\n", pkgver, strerror(rv));
				goto out;
			}
			continue;

		} else if (ttype == XBPS_TRANS_UPDATE) {
			/*
			 * Update a package: execute pre-remove action of
			 * existing package before unpacking new version.
			 */
			xbps_set_cb_state(xhp, XBPS_STATE_UPDATE, 0, pkgver, NULL);
			rv = xbps_remove_pkg(xhp, pkgver, true);
			if (rv != 0) {
				xbps_set_cb_state(xhp,
				    XBPS_STATE_UPDATE_FAIL,
				    rv, pkgver,
				    "%s: [trans] failed to update "
				    "package `%s'", pkgver,
				    strerror(rv));
				goto out;
			}
		} else if (ttype == XBPS_TRANS_CONFIGURE) {
			/*
			 * Package just needs to be configured, ignore it.
			 */
			continue;
		} else if (ttype == XBPS_TRANS_HOLD) {
			/*
			 * Package is on hold mode, ignore it.
			 */
			continue;
		} else {
			/* Install or reinstall package */
			xbps_set_cb_state(xhp, XBPS_STATE_INSTALL, 0,
			    pkgver, NULL);
		}
		/*
		 * Unpack binary package.
		 */
		if ((rv = xbps_unpack_binary_pkg(xhp, obj)) != 0) {
			xbps_dbg_printf("[trans] failed to unpack "
			    "%s: %s\n", pkgver, strerror(rv));
			goto out;
		}
		/*
		 * Register package.
		 */
		if ((rv = xbps_register_pkg(xhp, obj)) != 0) {
			xbps_dbg_printf("[trans] failed to register "
			    "%s: %s\n", pkgver, strerror(rv));
			goto out;
		}
	}
	/* if there are no packages to install or update we are done */
	if (!xbps_dictionary_get(xhp->transd, "total-update-pkgs") &&
	    !xbps_dictionary_get(xhp->transd, "total-install-pkgs"))
		goto out;

	if (xhp->target_arch && strcmp(xhp->native_arch, xhp->target_arch)) {
		/* if installing packages for target_arch, don't configure anything */
		goto out;
		/* do not configure packages if only unpacking is desired */
	} else if (xhp->flags & XBPS_FLAG_UNPACK_ONLY) {
		goto out;
	}

	xbps_object_iterator_reset(iter);
	/* Force a pkgdb write for all unpacked pkgs in transaction */
	if ((rv = xbps_pkgdb_update(xhp, true, true)) != 0)
		goto out;

	/*
	 * Run all post and purge-remove scripts.
	 */
	rv = run_post_remove_scripts(xhp, remove_scripts);
	if (rv < 0) {
		rv = -rv;
		goto out;
	}

	/*
	 * Configure all unpacked packages (post-install).
	 */
	xbps_set_cb_state(xhp, XBPS_STATE_TRANS_CONFIGURE, 0, NULL, NULL);

	while ((obj = xbps_object_iterator_next(iter)) != NULL) {
		xbps_dictionary_get_cstring_nocopy(obj, "pkgver", &pkgver);
		ttype = xbps_transaction_pkg_type(obj);
		if (ttype == XBPS_TRANS_REMOVE || ttype == XBPS_TRANS_HOLD) {
			xbps_dbg_printf("%s: skipping configuration for "
			    "%s: %d\n", __func__, pkgver, ttype);
			continue;
		}
		update = false;
		if (ttype == XBPS_TRANS_UPDATE)
			update = true;

		rv = xbps_configure_pkg(xhp, pkgver, false, update);
		if (rv != 0) {
			xbps_dbg_printf("%s: configure failed for "
			    "%s: %s\n", __func__, pkgver, strerror(rv));
			goto out;
		}
		/*
		 * Notify client callback when a package has been
		 * installed or updated.
		 */
		if (update) {
			xbps_set_cb_state(xhp, XBPS_STATE_UPDATE_DONE, 0,
			    pkgver, NULL);
		} else {
			xbps_set_cb_state(xhp, XBPS_STATE_INSTALL_DONE, 0,
			    pkgver, NULL);
		}
	}

out:
	xbps_object_release(remove_scripts);
	xbps_object_iterator_release(iter);
	if (rv == 0) {
		/* Force a pkgdb write for all unpacked pkgs in transaction */
		rv = xbps_pkgdb_update(xhp, true, true);
	}
	return rv;
}
