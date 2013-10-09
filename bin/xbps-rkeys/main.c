/*-
 * Copyright (c) 2013 Juan Romero Pardines.
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
#include <unistd.h>
#include <getopt.h>
#include <assert.h>

#include <xbps.h>
#include "defs.h"

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-rkeys [OPTIONS] <MODE> [REPOURL...]\n\n"
	    "OPTIONS\n"
	    " -a --all            Process all repositories in configuration file\n"
	    " -C --config <file>  Full path to configuration file\n"
	    " -d --debug          Debug mode shown to stderr\n"
	    " -h --help           Print usage help\n"
	    " -r --rootdir <dir>  Full path to rootdir\n"
	    " -V --version        Show XBPS version\n\n"
	    "MODE\n"
	    " -i --import         Import public RSA key(s)\n"
	    " -R --remove         Remove public RSA key(s)\n"
	    " -s --show           Show repository info\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
state_cb(struct xbps_state_cb_data *xscd, void *cbd _unused)
{
	int rv = 0;

	switch (xscd->state) {
	/* notifications */
	case XBPS_STATE_REPO_KEY_IMPORT:
		printf("%s\n", xscd->desc);
		printf("Fingerprint: ");
		xbps_print_hexfp(xscd->arg);
		printf("\n");
		rv = noyes("Do you want to import this public key?");
		break;
	case XBPS_STATE_REPOSYNC:
		printf("[*] Downloading repository index `%s'...\n", xscd->arg);
		break;
	default:
		xbps_dbg_printf(xscd->xhp,
		    "%s: unknown state %d\n", xscd->arg, xscd->state);
		break;
	}

	return rv;
}

static int
repo_import_key_cb(struct xbps_repo *repo, void *arg _unused, bool *done _unused)
{
	return xbps_repo_key_import(repo);
}

static int
repo_info_cb(struct xbps_repo *repo, void *arg _unused, bool *done _unused)
{
	xbps_dictionary_t rkeyd = NULL;
	xbps_data_t rpubkey;
	unsigned char *fp;
	const char *signee;
	uint16_t rpubkeysiz;

	if (!repo->is_remote)
		return 0;

	printf("%s (%s, %s)\n", repo->uri,
	    repo->is_signed ? "RSA signed" : "unsigned",
	    repo->is_verified ? "verified" : "unverified");

	rkeyd = xbps_dictionary_get(repo->xhp->repokeys, repo->uri);
	if (xbps_object_type(rkeyd) == XBPS_TYPE_DICTIONARY) {
		rpubkey = xbps_dictionary_get(rkeyd, "public-key");
		assert(rpubkey);
		xbps_dictionary_get_uint16(rkeyd, "public-key-size", &rpubkeysiz);
		xbps_dictionary_get_cstring_nocopy(rkeyd, "signature-by", &signee);
		printf(" Signed by: %s\n", signee);
		printf(" %u ", rpubkeysiz);
		fp = xbps_pubkey2fp(repo->xhp, rpubkey);
		assert(fp);
		xbps_print_hexfp((const char *)fp);
		free(fp);
		printf("\n");
	}
	return 0;
}

static int
repo_remove_key_cb(struct xbps_repo *repo, void *arg, bool *done _unused)
{
	bool *flush = arg;

	if (xbps_object_type(repo->xhp->repokeys) != XBPS_TYPE_DICTIONARY)
		return 0;

	xbps_dictionary_remove(repo->xhp->repokeys, repo->uri);
	printf("Removed `%s' from storage.\n", repo->uri);
	*flush = true;
	return 0;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "aC:dhir:Rsv";
	const struct option longopts[] = {
		{ "all", no_argument, NULL, 'a' },
		{ "config", required_argument, NULL, 'C' },
		{ "debug", no_argument, NULL, 'd' },
		{ "help", no_argument, NULL, 'h' },
		{ "import", no_argument, NULL, 'i' },
		{ "remove", no_argument, NULL, 'R' },
		{ "show", no_argument, NULL, 's' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	struct xferstat xfer;
	char *rkeys;
	const char *conffile = NULL, *rootdir = NULL;
	int c, rv, flags = 0;
	bool all, flush, import, remove, show;

	all = import = remove = show = flush = false;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		switch (c) {
		case 'a':
			all = true;
			break;
		case 'C':
			conffile = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'i':
			import = true;
			break;
		case 'R':
			remove = true;
			break;
		case 'r':
			rootdir = optarg;
			break;
		case 's':
			show = true;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage(true);
			/* NOTREACHED */
		}
	}
	if (!all && (argc == optind))
		usage(true);

	memset(&xh, 0, sizeof(xh));
	xh.fetch_cb = fetch_file_progress_cb;
	xh.fetch_cb_data = &xfer;
	xh.state_cb = state_cb;
	xh.rootdir = rootdir;
	xh.conffile = conffile;
	xh.flags = flags;

	/* register specified repos */
	if (!all) {
		for (int i = optind; i < argc; i++) {
			if (xh.repositories == NULL)
				xh.repositories = xbps_array_create();

			xbps_array_add_cstring_nocopy(xh.repositories, argv[i]);
		}
	}
	/* initialize libxbps */
	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (import) {
		/* Sync remote repodata first */
		xbps_rpool_sync(&xh, NULL);
		rv = xbps_rpool_foreach(&xh, repo_import_key_cb, NULL);
	} else if (remove) {
		rv = xbps_rpool_foreach(&xh, repo_remove_key_cb, &flush);
		if (flush) {
			rkeys = xbps_xasprintf("%s/%s", xh.metadir, XBPS_REPOKEYS);
			xbps_dictionary_externalize_to_file(xh.repokeys, rkeys);
			free(rkeys);
		}
	} else if (show) {
		rv = xbps_rpool_foreach(&xh, repo_info_cb, NULL);
	}
	xbps_end(&xh);

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
