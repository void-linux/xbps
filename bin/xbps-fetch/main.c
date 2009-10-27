#include <stdio.h>
#include <stdlib.h>
#include <xbps_api.h>

int
main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: xbps-fetch [options] URL\n");
		exit(EXIT_FAILURE);
	}

	return xbps_fetch_file(argv[1], ".");
}
