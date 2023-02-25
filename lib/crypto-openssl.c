#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <openssl/evp.h>

#include "crypto-impl.h"

int
xbps_hash_file(struct xbps_hash *hash, const char *path)
{
	char buf[BUFSIZ];
	EVP_MD_CTX *mdctx;
	const EVP_MD *md = EVP_blake2b512();
	int fd = -1;
	int r = 0;

	if (!md)
		return -ENOTSUP;

	assert(EVP_MD_size(md) == sizeof(hash->mem));

	mdctx = EVP_MD_CTX_new();
	if (!mdctx)
		return -ENOTSUP;

	if (!EVP_DigestInit_ex(mdctx, md, NULL))
		return -ENOTSUP;

	fd = open(path, O_RDONLY|O_CLOEXEC);
	if (fd == -1) {
		r = -errno;
		goto err;
	}

	for (;;) {
		ssize_t rd = read(fd, buf, sizeof(buf));
		if (rd < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			r = -errno;
			goto err;
		}
		if (rd == 0)
			break;
		if (!EVP_DigestUpdate(mdctx, buf, rd)) {
			r = -ENOTSUP;
			goto err;
		}
	}

	if (!EVP_DigestFinal_ex(mdctx, hash->mem, NULL))
		r = -ENOTSUP;
err:
	if (fd != -1)
		close(fd);
	EVP_MD_CTX_free(mdctx);
	close(fd);
	return r;
}
