#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xbps_api.h>
#include "fetch.h"

static void
usage(void)
{
	printf("usage: xbps-fetch [-v] URL\n");
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	char flags[8];
	int c, rv = 0;

	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
		case 'v':
			strcat(flags, "v");
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	rv = xbps_fetch_file(argv[0], ".", false, flags);
	if (rv != 0) {
		printf("%s: %s\n", argv[0], xbps_fetch_error_string());
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
