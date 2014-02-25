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
#include <unistd.h>
#include <getopt.h>
#include <syslog.h>

#include <xbps.h>

static void __attribute__((noreturn))
usage(bool fail)
{
	fprintf(stdout,
	    "Usage: xbps-reconfigure [OPTIONS] [PKGNAME...]\n\n"
	    "OPTIONS\n"
	    " -a --all            Process all packages\n"
	    " -C --config <file>  Full path to configuration file\n"
	    " -d --debug          Debug mode shown to stderr\n"
	    " -f --force          Force reconfiguration\n"
	    " -h --help           Print usage help\n"
	    " -r --rootdir <dir>  Full path to rootdir\n"
	    " -v --verbose        Verbose messages\n"
	    " -V --version        Show XBPS version\n");
	exit(fail ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int
state_cb(struct xbps_state_cb_data *xscd, void *cbd _unused)
{
	if (xscd->xhp->syslog) {
		openlog("xbps-reconfigure", LOG_CONS, LOG_USER);
	}

	switch (xscd->state) {
	/* notifications */
	case XBPS_STATE_CONFIGURE:
		printf("%s: configuring ...\n", xscd->arg);
		if (xscd->xhp->syslog)
			syslog(LOG_NOTICE, "%s: configuring ...", xscd->arg);
		break;
	case XBPS_STATE_CONFIGURE_DONE:
		printf("%s: configured successfully.\n", xscd->arg);
		if (xscd->xhp->syslog)
			syslog(LOG_NOTICE,
			    "%s: configured successfully.", xscd->arg);
		break;
	/* errors */
	case XBPS_STATE_CONFIGURE_FAIL:
		xbps_error_printf("%s\n", xscd->desc);
		if (xscd->xhp->syslog)
			syslog(LOG_ERR, "%s", xscd->desc);
		break;
	default:
		xbps_dbg_printf(xscd->xhp,
		    "%s: unknown state %d\n", xscd->arg, xscd->state);
		break;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	const char *shortopts = "aC:dfhr:Vv";
	const struct option longopts[] = {
		{ "all", no_argument, NULL, 'a' },
		{ "config", required_argument, NULL, 'C' },
		{ "debug", no_argument, NULL, 'd' },
		{ "force", no_argument, NULL, 'f' },
		{ "help", no_argument, NULL, 'h' },
		{ "rootdir", required_argument, NULL, 'r' },
		{ "verbose", no_argument, NULL, 'v' },
		{ "version", no_argument, NULL, 'V' },
		{ NULL, 0, NULL, 0 }
	};
	struct xbps_handle xh;
	const char *conffile = NULL, *rootdir = NULL;
	int c, i, rv, flags = 0;
	bool all = false;

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
		case 'f':
			flags |= XBPS_FLAG_FORCE_CONFIGURE;
			break;
		case 'h':
			usage(false);
			/* NOTREACHED */
		case 'r':
			rootdir = optarg;
			break;
		case 'v':
			flags |= XBPS_FLAG_VERBOSE;
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
	xh.state_cb = state_cb;
	if (rootdir)
		strncpy(xh.rootdir, rootdir, sizeof(xh.rootdir));
	xh.conffile = conffile;
	xh.flags = flags;

	if ((rv = xbps_init(&xh)) != 0) {
		xbps_error_printf("Failed to initialize libxbps: %s\n",
		    strerror(rv));
		exit(EXIT_FAILURE);
	}

	if (all) {
		rv = xbps_configure_packages(&xh, true);
	} else {
		for (i = optind; i < argc; i++) {
			rv = xbps_configure_pkg(&xh, argv[i],
			    true, false, true);
			if (rv != 0)
				fprintf(stderr, "Failed to reconfigure "
				    "`%s': %s\n", argv[i], strerror(rv));
		}
	}

	exit(rv ? EXIT_FAILURE : EXIT_SUCCESS);
}
