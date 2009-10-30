#include <stdio.h>
#include <stdlib.h>
#include <xbps_api.h>

int
main(int argc, char **argv)
{
	int rv = 0;

	if (argc != 2) {
		printf("Usage: xbps-fetch [options] URL\n");
		exit(EXIT_FAILURE);
	}

	rv = xbps_fetch_file(argv[1], ".");
	if (rv != 0) {
		printf("xbps-fetch: couldn't download %s!\n", argv[1]);
		printf("xbps-fetch: %s\n", xbps_fetch_error_string());
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
