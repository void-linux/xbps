/*-
 * Copyright (c) 2009-2011 Juan Romero Pardines.
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

#include "xbps_api_impl.h"

/**
 * @file lib/transaction_dictionary.c
 * @brief Transaction handling routines
 * @defgroup transdict Transaction handling functions
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

static prop_dictionary_t transd;
static prop_array_t trans_mdeps;
static bool transd_initialized;
static bool trans_mdeps_initialized;

static int
create_transaction_dictionary(void)
{
	prop_array_t unsorted;

	if (transd_initialized)
		return 0;

	transd = prop_dictionary_create();
	if (transd == NULL)
		return ENOMEM;

        unsorted = prop_array_create();
        if (unsorted == NULL) {
		prop_object_release(transd);
		return ENOMEM;
	}

        if (!xbps_add_obj_to_dict(transd, unsorted, "unsorted_deps")) {
		prop_object_release(unsorted);
		prop_object_release(transd);
		return EINVAL;
	}

	transd_initialized = true;
	return 0;
}

static int
create_transaction_missingdeps(void)
{
	if (trans_mdeps_initialized)
		return 0;

	trans_mdeps = prop_array_create();
	if (trans_mdeps == NULL)
		return ENOMEM;

	trans_mdeps_initialized = true;
	return 0;
}

static int
compute_transaction_stats(void)
{
	prop_object_iterator_t iter;
	prop_object_t obj;
	uint64_t tsize, dlsize, instsize;
	uint32_t inst_pkgcnt, up_pkgcnt, cf_pkgcnt, rm_pkgcnt;
	int rv = 0;
	const char *tract;

	inst_pkgcnt = up_pkgcnt = cf_pkgcnt = rm_pkgcnt = 0;
	tsize = dlsize = instsize = 0;

	iter = xbps_array_iter_from_dict(transd, "packages");
	if (iter == NULL)
		return EINVAL;

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		/*
		 * Count number of pkgs to be removed, configured,
		 * installed and updated.
		 */
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		if (strcmp(tract, "install") == 0)
			inst_pkgcnt++;
		else if (strcmp(tract, "update") == 0)
			up_pkgcnt++;
		else if (strcmp(tract, "configure") == 0)
			cf_pkgcnt++;
		else if (strcmp(tract, "remove") == 0)
			rm_pkgcnt++;
	}

	if (inst_pkgcnt &&
	    !prop_dictionary_set_uint32(transd, "total-install-pkgs",
	    inst_pkgcnt)) {
		rv = EINVAL;
		goto out;
	}
	if (up_pkgcnt &&
	    !prop_dictionary_set_uint32(transd, "total-update-pkgs",
	    up_pkgcnt)) {
		rv = EINVAL;
		goto out;
	}
	if (cf_pkgcnt &&
	    !prop_dictionary_set_uint32(transd, "total-configure-pkgs",
	    cf_pkgcnt)) {
		rv = EINVAL;
		goto out;
	}
	if (rm_pkgcnt &&
	    !prop_dictionary_set_uint32(transd, "total-remove-pkgs",
	    rm_pkgcnt)) {
		rv = EINVAL;
		goto out;
	}

	prop_object_iterator_reset(iter);

	while ((obj = prop_object_iterator_next(iter)) != NULL) {
		prop_dictionary_get_cstring_nocopy(obj, "transaction", &tract);
		/*
		 * Only process pkgs to be installed or updated.
		 */
		if ((strcmp(tract, "configure") == 0) ||
		    (strcmp(tract, "remove") == 0))
			continue;

		prop_dictionary_get_uint64(obj, "filename-size", &tsize);
		dlsize += tsize;
		tsize = 0;
		prop_dictionary_get_uint64(obj, "installed_size", &tsize);
		instsize += tsize;
		tsize = 0;
	}

	/*
	 * Add object in transaction dictionary with total installed
	 * size that it will take.
	 */
	if (!prop_dictionary_set_uint64(transd,
	    "total-installed-size", instsize)) {
		rv = EINVAL;
		goto out;
	}
	/*
	 * Add object in transaction dictionary with total download
	 * size that needs to be sucked in.
	 */
	if (!prop_dictionary_set_uint64(transd,
	    "total-download-size", dlsize)) {
		rv = EINVAL;
		goto out;
	}
out:
	prop_object_iterator_release(iter);

	return rv;
}

prop_dictionary_t HIDDEN
xbps_transaction_dictionary_get(void)
{
	if (create_transaction_dictionary() != 0)
		return NULL;

	return transd;
}

prop_array_t
xbps_transaction_missingdeps_get(void)
{
	if (create_transaction_missingdeps() != 0)
		return NULL;

	return trans_mdeps;
}

prop_dictionary_t
xbps_transaction_prepare(void)
{
	int rv = 0;

	if (!transd_initialized && !trans_mdeps_initialized) {
		errno = ENXIO;
		return NULL;
	}
	/*
	 * If there are missing deps bail out.
	 */
	if (prop_array_count(trans_mdeps) > 0) {
		prop_object_release(transd);
		errno = ENODEV;
		return NULL;
	}
	/*
	 * Check for packages to be replaced.
	 */
	if ((rv = xbps_transaction_package_replace(transd)) != 0) {
		errno = rv;
		prop_object_release(transd);
		prop_object_release(trans_mdeps);
		return NULL;
	}
	/*
	 * Sort package dependencies if necessary.
	 */
	if ((rv = xbps_sort_pkg_deps()) != 0) {
		errno = rv;
		prop_object_release(transd);
		prop_object_release(trans_mdeps);
		return NULL;
	}
	/*
	 * Add transaction stats for total download/installed size,
	 * number of packages to be installed, updated, configured
	 * and removed to the transaction dictionary.
	 */
	if ((rv = compute_transaction_stats()) != 0) {
		errno = rv;
		prop_object_release(transd);
		prop_object_release(trans_mdeps);
		return NULL;
	}
	/*
	 * The missing deps array is not necessary anymore.
	 */
	prop_object_release(trans_mdeps);

	return prop_dictionary_copy(transd);
}
