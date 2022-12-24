/*-
 * Copyright (c) 2014 Juan Romero Pardines.
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
#include <sys/stat.h>
#include <fcntl.h>

#include "xbps_api_impl.h"

static int
pkgdb038(struct xbps_handle *xhp, const char *opkgdb_plist)
{
	xbps_dictionary_t pkgdb, opkgdb;
	xbps_object_iterator_t iter;
	xbps_object_t obj;
	int rv = 0;

	/*
	 * The pkgdb-0.38.plist format contains all pkg metadata objects,
	 * except its files list. To avoid a broken conversion, the old
	 * pkg metadata plists are kept, and the converted ones are written
	 * to another path.
	 *
	 * 	- <metadir>/pkgdb-0.38.plist
	 * 	- <metadir>/.<pkgname>-files.plist
	 */
	opkgdb = xbps_plist_dictionary_from_file(xhp, opkgdb_plist);
	if (opkgdb == NULL)
		return EINVAL;

	pkgdb = xbps_dictionary_create();
	assert(pkgdb);
	/*
	 * Iterate over the old pkgdb dictionary and copy all pkg objects
	 * into the new pkgdb dictionary.
	 */
	iter = xbps_dictionary_iterator(opkgdb);
	assert(iter);

	while ((obj = xbps_object_iterator_next(iter))) {
		xbps_dictionary_t pkgd, pkgfilesd, pkgmetad;
		xbps_object_iterator_t iter2;
		xbps_object_t obj2;
		const char *pkgname, *repo;
		char *pkgmeta;

		pkgname = xbps_dictionary_keysym_cstring_nocopy(obj);
		pkgd = xbps_dictionary_get_keysym(opkgdb, obj);
		/*
		 * Rename "repository-origin" obj to "repository" to match
		 * the repository index obj.
		 */
		if (xbps_dictionary_get_cstring_nocopy(pkgd, "repository-origin", &repo)) {
			xbps_dictionary_set_cstring(pkgd, "repository", repo);
			xbps_dictionary_remove(pkgd, "repository-origin");
		}
		/*
		 * Copy old pkgdb objects to the new pkgdb.
		 */
		if (!xbps_dictionary_set(pkgdb, pkgname, pkgd)) {
			xbps_dbg_printf("%s: failed to copy %s pkgd "
			   "for pkgdb conversion\n", __func__, pkgname);
			rv = EINVAL;
			goto out;
		}
		/*
		 * Copy pkg metadata objs to the new pkgdb.
		 */
		pkgmeta = xbps_xasprintf("%s/.%s.plist", xhp->metadir, pkgname);
		pkgmetad = xbps_plist_dictionary_from_file(xhp, pkgmeta);
		if (pkgmetad == NULL) {
			rv = EINVAL;
			xbps_dbg_printf("%s: cannot open %s: %s\n",
			    __func__, pkgmeta, strerror(errno));
			goto out;
		}
		pkgfilesd = xbps_dictionary_create();
		assert(pkgfilesd);

		iter2 = xbps_dictionary_iterator(pkgmetad);
		assert(iter2);

		while ((obj2 = xbps_object_iterator_next(iter2))) {
			xbps_object_t curobj;
			const char *key, *excluded[] = {
				"conf_files", "dirs", "files", "links"
			};
			bool skip = false;

			key = xbps_dictionary_keysym_cstring_nocopy(obj2);
			curobj = xbps_dictionary_get_keysym(pkgmetad, obj2);
			for (uint8_t i = 0; i < __arraycount(excluded); i++) {
				if (strcmp(excluded[i], key) == 0) {
					skip = true;
					break;
				}
			}
			if (skip) {
				assert(xbps_object_type(curobj) == XBPS_TYPE_ARRAY);
				if (xbps_array_count(curobj))
					xbps_dictionary_set(pkgfilesd, key, curobj);

				continue;
			}
			if (!xbps_dictionary_set(pkgd, key, curobj)) {
				xbps_dbg_printf("%s: failed to copy %s "
				    "pkgd for pkgdb conversion\n", pkgname, key);
				xbps_object_iterator_release(iter2);
				xbps_object_release(pkgmetad);
				free(pkgmeta);
				rv = EINVAL;
				goto out;
			}
		}
		xbps_object_iterator_release(iter2);
		/*
		 * Externalize <pkgname>-files.plist if pkg contains any file.
		 */
		if (xbps_dictionary_count(pkgfilesd)) {
			char *pkgfiles, *sha256;

			pkgfiles = xbps_xasprintf("%s/.%s-files.plist", xhp->metadir, pkgname);
			if (!xbps_dictionary_externalize_to_file(pkgfilesd, pkgfiles)) {
				xbps_dbg_printf("%s: failed to "
				    "externalize %s: %s\n", __func__, pkgfiles, strerror(errno));
				rv = EINVAL;
				goto out;
			}
			xbps_dbg_printf("%s: externalized %s successfully\n", __func__, pkgfiles);
			/*
			 * Update SHA56 hash for the pkg files plist.
			 */
			sha256 = xbps_file_hash(pkgfiles);
			assert(sha256);
			xbps_dictionary_set_cstring(pkgd, "metafile-sha256", sha256);
			free(sha256);
			free(pkgfiles);
		} else {
			/* unnecessary obj if pkg contains no files */
			xbps_dictionary_remove(pkgd, "metafile-sha256");
		}
		xbps_object_release(pkgfilesd);
		xbps_object_release(pkgmetad);
		free(pkgmeta);
	}
	/*
	 * Externalize the new pkgdb plist.
	 */
	if (!xbps_dictionary_externalize_to_file(pkgdb, xhp->pkgdb_plist)) {
		xbps_dbg_printf("%s: failed to externalize %s: "
		    "%s!\n", __func__, xhp->pkgdb_plist, strerror(errno));
		rv = EINVAL;
		goto out;
	}
out:
	if (iter)
		xbps_object_iterator_release(iter);
	if (pkgdb)
		xbps_object_release(pkgdb);
	if (opkgdb)
		xbps_object_release(opkgdb);

	return rv;
}

int HIDDEN
xbps_pkgdb_conversion(struct xbps_handle *xhp)
{
	char *opkgdb = NULL;
	int rv = 0;

	/*
	 * If pkgdb-0.38.plist exists there's nothing to do.
	 */
	if (xhp && xhp->pkgdb_plist && (access(xhp->pkgdb_plist, R_OK) == 0))
		return 0;
	/*
	 * If pkgdb-0.21.plist does not exist there's nothing to do.
	 */
	opkgdb = xbps_xasprintf("%s/pkgdb-0.21.plist", xhp->metadir);
	if (access(opkgdb, R_OK) == 0) {
		/*
		 * Make the conversion and exit on success. It's just
		 * better to make the upgrade in two steps.
		 */
		xbps_set_cb_state(xhp, XBPS_STATE_PKGDB, 0, NULL, NULL);
		if ((rv = pkgdb038(xhp, opkgdb)) == 0) {
			xbps_set_cb_state(xhp, XBPS_STATE_PKGDB_DONE, 0, NULL, NULL);
			xbps_end(xhp);
			exit(EXIT_SUCCESS);
		}
	}
	free(opkgdb);

	return rv;
}
