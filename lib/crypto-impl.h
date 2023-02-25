#ifndef CRYPTO_IMPL_H
#define CRYPTO_IMPL_H

#include <assert.h>
#include <errno.h>

#include "xbps/crypto.h"

#include "macro.h"

#ifndef HIDDEN
#if HAVE_VISIBILITY
#define HIDDEN __attribute__ ((visibility("hidden")))
#else
#define HIDDEN
#endif
#endif

#define PASSWORDMAXBYTES               1024
#define SIGALG                         "Ed"
#define SIGALG_HASHED                  "ED"
#define KDFALG                         "Sc"
#define KDFNONE                        "\0\0"
#define CHKALG                         "B2"
#define COMMENT_PREFIX                 "untrusted comment: "
#define DEFAULT_COMMENT                "signature from minisign secret key"
#define SECRETKEY_DEFAULT_COMMENT      "minisign encrypted secret key"
#define TRUSTED_COMMENT_PREFIX         "trusted comment: "
#define SIG_DEFAULT_CONFIG_DIR         ".minisign"
#define SIG_DEFAULT_CONFIG_DIR_ENV_VAR "MINISIGN_CONFIG_DIR"
#define SIG_DEFAULT_PKFILE             "minisign.pub"
#define SIG_DEFAULT_SKFILE             "minisign.key"
#define SIG_SUFFIX                     ".minisig"
#define VERSION_STRING                 "minisign 0.11"

int HIDDEN encrypt_key(struct xbps_seckey *seckey, const char *passphrase);
int HIDDEN decrypt_key(struct xbps_seckey *seckey, const char *passphrase);
int HIDDEN randombytes_buf(void *buf, size_t buflen);

#endif /*!CRYPTO_IMPL_H*/
