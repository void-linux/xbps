/*-
 * Copyright (c) 2008-2012 Juan Romero Pardines.
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
#include <strings.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <unistd.h>

#include <xbps_api.h>
#include "../xbps-bin/defs.h"

/* error messages in bold/red */
#define MSG_ERROR	"\033[1m\033[31m"
/* warn messages in bold/yellow */
#define MSG_WARN	"\033[1m\033[33m"
/* normal messages in bold */
#define MSG_NORMAL	"\033[1m"
#define MSG_RESET	"\033[m"

static void
write_plist_file(prop_dictionary_t dict, const char *file)
{
	assert(dict != NULL || file != NULL);

	if (!prop_dictionary_externalize_to_zfile(dict, file)) {
		prop_object_release(dict);
		xbps_error_printf("xbps-uhelper: couldn't write to %s: %s",
		    file, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void __attribute__((noreturn))
usage(void)
{
	fprintf(stderr,
	"usage: xbps-uhelper [options] [action] [args]\n"
	"\n"
	"  Available actions:\n"
	"    cmpver, digest, fetch, getpkgdepname, getpkgname, getpkgrevision,\n"
	"    getpkgversion, pkgmatch, register, sanitize-plist, unregister,\n"
	"    updatepkgdb, version\n"
	"\n"
	"  Action arguments:\n"
	"    cmpver\t\t<instver> <reqver>\n"
	"    digest\t\t<file> <file1+N>\n"
	"    fetch\t\t<URL> <URL1+N>\n"
	"    getpkgdepname\t<string>\n"
	"    getpkgdepversion\t<string>\n"
	"    getpkgname\t\t<string>\n"
	"    getpkgrevision\t<string>\n"
	"    getpkgversion\t<string>\n"
	"    pkgmatch\t\t<pkg-version> <pkg-pattern>\n"
	"    register\t\t<pkgname> <version> <shortdesc>\n"
	"    sanitize-plist\t<plist>\n"
	"    unregister\t\t<pkgname> <version>\n"
	"    version\t\t<pkgname>\n"
	"\n"
	"  Options shared by all actions:\n"
	"    -C\t\tPath to xbps.conf file.\n"
	"    -d\t\tDebugging messages to stderr.\n"
	"    -r\t\t\t<rootdir>\n"
	"    -V\t\tPrints the xbps release version\n"
	"\n"
	"  Examples:\n"
	"    $ xbps-uhelper cmpver 'foo-1.0' 'foo-2.1'\n"
	"    $ xbps-uhelper digest file ...\n"
	"    $ xbps-uhelper fetch http://www.foo.org/file.blob ...\n"
	"    $ xbps-uhelper getpkgdepname 'foo>=0'\n"
	"    $ xbps-uhelper getpkgdepversion 'foo>=0'\n"
	"    $ xbps-uhelper getpkgname foo-2.0\n"
	"    $ xbps-uhelper getpkgrevision foo-2.0_1\n"
	"    $ xbps-uhelper getpkgversion foo-2.0\n"
	"    $ xbps-uhelper pkgmatch foo-1.0 'foo>=1.0'\n"
	"    $ xbps-uhelper register pkgname 2.0 \"A short description\"\n"
	"    $ xbps-uhelper sanitize-plist foo.plist\n"
	"    $ xbps-uhelper unregister pkgname 2.0\n"
	"    $ xbps-uhelper updatepkgdb\n"
	"    $ xbps-uhelper version pkgname\n");

	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	prop_dictionary_t dict, pkgd;
	prop_array_t array;
	struct xbps_handle xh;
	struct xferstat xfer;
	const char *pkgn, *version, *rootdir = NULL, *confdir = NULL;
	char *plist, *pkgname, *in_chroot_env, *hash, *tmp;
	bool in_chroot = false;
	int flags = 0, i, c, rv = 0;

	while ((c = getopt(argc, argv, "C:dr:V")) != -1) {
		switch (c) {
		case 'C':
			confdir = optarg;
			break;
		case 'r':
			/* To specify the root directory */
			rootdir = optarg;
			break;
		case 'd':
			flags |= XBPS_FLAG_DEBUG;
			break;
		case 'V':
			printf("%s\n", XBPS_RELVER);
			exit(EXIT_SUCCESS);
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	if ((strcasecmp(argv[0], "register") == 0) ||
	    (strcasecmp(argv[0], "unregister") == 0) ||
	    (strcasecmp(argv[0], "version") == 0) ||
	    (strcasecmp(argv[0], "fetch") == 0) ||
	    (strcasecmp(argv[0], "updatepkgdb") == 0)) {
		/*
		* Initialize libxbps.
		*/
		memset(&xh, 0, sizeof(xh));
		xh.flags = flags;
		xh.fetch_cb = fetch_file_progress_cb;
		xh.fetch_cb_data = &xfer;
		xh.rootdir = rootdir;
		xh.conffile = confdir;
		if ((rv = xbps_init(&xh)) != 0) {
			xbps_error_printf("xbps-uhelper: failed to "
			    "initialize libxbps: %s.\n", strerror(rv));
			exit(EXIT_FAILURE);
		}
	}

	in_chroot_env = getenv("in_chroot");
	if (in_chroot_env != NULL)
		in_chroot = true;

	if (strcasecmp(argv[0], "register") == 0) {
		/* Registers a package into the database */
		if (argc != 4)
			usage();

		dict = prop_dictionary_create();
		if (dict == NULL) {
			rv = -1;
			goto out;
		}
		prop_dictionary_set_cstring_nocopy(dict, "pkgname", argv[1]);
		prop_dictionary_set_cstring_nocopy(dict, "version", argv[2]);
		prop_dictionary_set_cstring_nocopy(dict, "short_desc", argv[3]);
		prop_dictionary_set_bool(dict, "automatic-install", false);

		tmp = xbps_xasprintf("%s-%s", argv[1], argv[2]);
		assert(tmp != NULL);
		prop_dictionary_set_cstring_nocopy(dict, "pkgver", tmp);

		pkgd = xbps_pkgdb_get_pkgd(&xh, argv[1], false);
		if (pkgd != NULL) {
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "pkgname", &pkgn);
			prop_dictionary_get_cstring_nocopy(pkgd,
			    "version", &version);
			prop_object_release(pkgd);
			fprintf(stderr, "%s%s=> ERROR: `%s-%s' is already "
			    "registered!%s\n", MSG_ERROR,
			    in_chroot ? "[chroot] " : "",
			    pkgn, version, MSG_RESET);
		} else {
			rv = xbps_set_pkg_state_installed(&xh, argv[1], argv[2],
			    XBPS_PKG_STATE_INSTALLED);
			if (rv != 0)
				goto out;

			rv = xbps_register_pkg(&xh, dict, true);
			if (rv != 0) {
				fprintf(stderr, "%s%s=> couldn't register %s-%s "
				    "(%s).%s\n", MSG_ERROR,
				    in_chroot ? "[chroot] " : "" , argv[1],
				    argv[2], strerror(rv), MSG_RESET);
			} else {
				printf("%s%s=> %s-%s registered successfully.%s\n",
				    MSG_NORMAL, in_chroot ? "[chroot] " : "",
				    argv[1], argv[2], MSG_RESET);
			}
		}
		prop_object_release(dict);
	} else if (strcasecmp(argv[0], "unregister") == 0) {
		/* Unregisters a package from the database */
		if (argc != 3)
			usage();

		rv = xbps_unregister_pkg(&xh, argv[1], argv[2], true);
		if (rv == ENOENT) {
			fprintf(stderr, "%s=> ERROR: %s not registered "
			    "in database.%s\n", MSG_WARN, argv[1], MSG_RESET);
		} else if (rv != 0 && rv != ENOENT) {
			fprintf(stderr, "%s=> ERROR: couldn't unregister %s "
			    "from database (%s)%s\n", MSG_ERROR,
			    argv[1], strerror(errno), MSG_RESET);
		} else {
			printf("%s%s=> %s-%s unregistered successfully.%s\n",
			    MSG_NORMAL, in_chroot ? "[chroot] " : "", argv[1],
			    argv[2], MSG_RESET);
		}
	} else if (strcasecmp(argv[0], "version") == 0) {
		/* Prints version of an installed package */
		if (argc != 2)
			usage();

		dict = xbps_pkgdb_get_pkgd(&xh, argv[1], false);
		if (dict == NULL) {
			rv = errno;
			goto out;
		}
		prop_dictionary_get_cstring_nocopy(dict, "version", &version);
		printf("%s\n", version);
		prop_object_release(dict);
	} else if (strcasecmp(argv[0], "sanitize-plist") == 0) {
		/* Sanitize a plist file (properly indent the file) */
		if (argc != 2)
			usage();

		dict = prop_dictionary_internalize_from_zfile(argv[1]);
		if (dict == NULL) {
			fprintf(stderr,
			    "=> ERROR: couldn't sanitize %s plist file "
			    "(%s)\n", argv[1], strerror(errno));
			exit(EXIT_FAILURE);
		}
		write_plist_file(dict, argv[1]);
	} else if (strcasecmp(argv[0], "getpkgversion") == 0) {
		/* Returns the version of a pkg string */
		if (argc != 2)
			usage();

		version = xbps_pkg_version(argv[1]);
		if (version == NULL) {
			fprintf(stderr,
			    "Invalid string, expected <string>-<version>\n");
			exit(EXIT_FAILURE);
		}
		printf("%s\n", version);
	} else if (strcasecmp(argv[0], "getpkgname") == 0) {
		/* Returns the name of a pkg string */
		if (argc != 2)
			usage();

		pkgname = xbps_pkg_name(argv[1]);
		if (pkgname == NULL) {
			fprintf(stderr,
			    "Invalid string, expected <string>-<version>\n");
			exit(EXIT_FAILURE);
		}
		printf("%s\n", pkgname);
		free(pkgname);
	} else if (strcasecmp(argv[0], "getpkgrevision") == 0) {
		/* Returns the revision of a pkg string */
		if (argc != 2)
			usage();

		version = xbps_pkg_revision(argv[1]);
		if (version == NULL)
			exit(EXIT_SUCCESS);

		printf("%s\n", version);
	} else if (strcasecmp(argv[0], "getpkgdepname") == 0) {
		/* Returns the pkgname of a dependency */
		if (argc != 2)
			usage();

		pkgname = xbps_pkgpattern_name(argv[1]);
		if (pkgname == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", pkgname);
		free(pkgname);
	} else if (strcasecmp(argv[0], "getpkgdepversion") == 0) {
		/* returns the version of a package pattern dependency */
		if (argc != 2)
			usage();

		version = xbps_pkgpattern_version(argv[1]);
		if (version == NULL)
			exit(EXIT_FAILURE);

		printf("%s\n", version);
	} else if (strcasecmp(argv[0], "pkgmatch") == 0) {
		/* Matches a pkg with a pattern */
		if (argc != 3)
			usage();

		exit(xbps_pkgpattern_match(argv[1], argv[2]));
	} else if (strcasecmp(argv[0], "cmpver") == 0) {
		/* Compare two version strings, installed vs required */
		if (argc != 3)
			usage();

		exit(xbps_cmpver(argv[1], argv[2]));
	} else if (strcasecmp(argv[0], "digest") == 0) {
		/* Prints SHA256 hashes for specified files */
		if (argc < 2)
			usage();

		for (i = 1; i < argc; i++) {
			hash = xbps_file_hash(argv[i]);
			if (hash == NULL) {
				fprintf(stderr,
				    "E: couldn't get hash for %s (%s)\n",
				    argv[i], strerror(errno));
				exit(EXIT_FAILURE);
			}
			printf("%s\n", hash);
			free(hash);
		}
	} else if (strcasecmp(argv[0], "fetch") == 0) {
		/* Fetch a file from specified URL */
		if (argc != 2)
			usage();

		for (i = 1; i < argc; i++) {
			rv = xbps_fetch_file(&xh, argv[i], ".", false, "v");
			if (rv == -1) {
				printf("%s: %s\n", argv[1],
				    xbps_fetch_error_string());
			} else if (rv == 0) {
				printf("%s: file is identical than remote.\n",
				    argv[1]);
			} else
				rv = 0;
		}
	} else if (strcasecmp(argv[0], "updatepkgdb") == 0) {
		/* update regpkgdb to pkgdb */
		plist = xbps_xasprintf("%s/regpkgdb.plist", xh.metadir);
	        if (plist == NULL) {
			rv = ENOMEM;
			goto out;
		}

		dict = prop_dictionary_internalize_from_zfile(plist);
		free(plist);
		if (dict != NULL) {
			array = prop_dictionary_get(dict, "packages");
			if (array == NULL) {
				prop_object_release(dict);
				rv = EINVAL;
				goto out;
			}
		        xh.pkgdb = prop_array_copy(array);
		        prop_object_release(dict);
			rv = xbps_pkgdb_update(&xh, true);
			if (rv == 0) {
				printf("Migrated regpkgdb to pkgdb "
				    "successfully.\n");
			} else {
				xbps_error_printf("failed to write "
				    "pkgdb plist: %s\n", strerror(rv));
			}
		}
	} else {
		usage();
	}
out:
	xbps_end(&xh);
	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
