#ifndef XBPS_CRYPTO_H
#define XBPS_CRYPTO_H

#include <sys/types.h>

#include <stdint.h>

/** @addtogroup crypto */
/**@{*/

/**
 * @def SIGALG_HASHED
 * @brief Ed25519 public-key signature of the BLAKE2b hash of the message.
 */

/**
 * @def SIGALG
 * @brief Ed25519 public-key signature of the message.
 */

/**
 * @def KEYNUM_BYTES
 * @brief Number of bytes used for keynum.
 */
#define KEYNUM_BYTES             8

/**
 * @def COMMENTMAXBYTES
 * @brief Maximum bytes of untristed comments including trailing `\0`.
 */
#define COMMENTMAXBYTES         1024

/**
 * @def TRUSTEDCOMMENTMAXBYTES
 * @brief Maximum bytes of trusted comments including trailing `\0`.
 */
#define TRUSTEDCOMMENTMAXBYTES  8192

/**
 * @def SIG_BYTES
 * @brief Number of bytes of the Ed25519 public-key signature.
 */
#define SIG_BYTES 64

/**
 * @def PUBKEY_BYTES
 * @brief Number of bytes of the public-key.
 */
#define PUBKEY_BYTES 32

/**
 * @def SECKEY_BYTES
 * @brief Number of bytes of the secret-key.
 */
#define SECKEY_BYTES 32

/**
 * @def HASHBYTES
 * @brief Number of bytes for the BLAKE2b hash.
 */
#define HASH_BYTES 64

/**
 * @def HASHBYTES
 * @brief Number of bytes for the BLAKE2b hash.
 */
#define CHK_HASH_BYTES 32

/**
 * @struct xbps_hash
 * @brief BLAKE2b hash.
 */
struct xbps_hash {
	uint8_t mem[HASH_BYTES];
};

/**
 * @brief
 */
void xbps_wipe_secret(void *secret, size_t size);

/**
 * @brief Hash a file.
 *
 * @param[out] hash ::xbps_hash struct to store the hash.
 * @param[in] path Path of the file to hash.
 *
 * @return 0 on success or a negative \c errno from \c open(3) or \c read(3).
 */
int xbps_hash_file(struct xbps_hash *hash, const char *path);

/**
 * @struct xbps_pubkey
 * @brief minisign public-key.
 *
 * Algorithm id, keynum and Ed25519 public-key.
 */
struct xbps_pubkey {
	/**
	 * @var sig_alg
	 * @brief Algorithm identifier.
	 */
	uint8_t sig_alg[2];
	/**
	 * @var keynum_pk
	 * @brief wtf
	 */
	struct {
		/**
		 * @var keynum
		 * @brief key identifier
		 */
		uint8_t keynum[KEYNUM_BYTES];
		/**
		 * @var pk
		 * @brief Ed25519 public-key
		 */
		uint8_t pk[PUBKEY_BYTES];
	} keynum_pk;
};

/**
 * @brief Decode a base64 encoded minisign public-key.
 * @param[out] pubkey Structure to decode the public-key to.
 * @param[in] pubkey_s Encoded public-key.
 * @returns \c 0 on success or a negative \c errno.
 * @retval -ENOTSUP Public-key specifies a signature algorithm that is not supported.
 * @retval -EINVAL Public-key is invalid.
 */
int xbps_pubkey_decode(struct xbps_pubkey *pubkey, const char *pubkey_s);

/**
 * @brief Encode a minising public-key using base64.
 * @param[in] pubkey Public-key to encode.
 * @param[out] pubkey_s Buffer to store the encoded public-key.
 * @param[in] pubkey_s_len Size of the \p pubkey_s buffer.
 * @returns \c 0 on success or a negative \c errno.
 * @retval -ENOBUFS Encoded public-key would exceed the \p pubkey_s buffer.
 */
int xbps_pubkey_encode(const struct xbps_pubkey *pubkey, char *pubkey_s, size_t pubkey_s_len);

/**
 * @brief Read a minisign public-key file
 * @param[out] pubkey Structure to store the public-key to.
 * @param[in] fd File descriptor to read from
 * @returns \c 0 on success or a negative \c errno from ::xbps_pubkey_decode, \c open(3) or \c read(3).
 * @retval -ENOBUFS Comment or the encoded public-key exceed the maximum size.
 */
int xbps_pubkey_read(struct xbps_pubkey *pubkey, int fd);

int xbps_pubkey_write(const struct xbps_pubkey *pubkey, const char *path);

/**
 * @struct xbps_seckey
 * @brief minisign secret-key.
 *
 * Ed25519 secret-key, algorithm id and encryption data.
 */
struct xbps_seckey {
	uint8_t sig_alg[2];
	uint8_t kdf_alg[2];
	uint8_t chk_alg[2];
	union {
		struct {
			uint8_t salt[32];
			uint8_t opslimit_le[8];
			uint8_t memlimit_le[8];
		} kdf_minisign;
		struct {
			uint8_t salt[32];
			uint8_t num_blocks_le[4];
			uint8_t num_iterations_le[4];
		} kdf_xbps;
	};
	struct {
		uint8_t keynum[KEYNUM_BYTES];
		uint8_t sk[SECKEY_BYTES];
		uint8_t pk[PUBKEY_BYTES];
		/**
		 * @var chk
		 * @brief BLAKE2b hash of the secret-key.
		 *
		 * BLAKE2b hash of the secret-key used to check if the secret-key
		 * was successfully decrypted.
		 */
		uint8_t chk[CHK_HASH_BYTES];
	} keynum_sk;
};

/**
 * @brief Read and decrypt a secret-key.
 * @param[out] seckey Structure to store the read secret-key in.
 * @param[in] passphrase Optional passphrase to encrypt the secret-key.
 * @param[in] path File descriptor to read from.
 * @returns \c 0 on success or a negative \c errno.
 * @retval -ENOBUFS Comment or encoded secret-key exceed the maximum size.
 * @retval -ENOTSUP Secret-key used an algorithm or encryption that is not supported.
 * @retval -EINVAL Secret-key is invalid.
 * @retval -ERANGE Secret-key is encrypted but no passphrase was supplied or decryption failed.
 * @retval -ENOMEM Secret-key decryption failed due to resource limits.
 */
int xbps_seckey_read(struct xbps_seckey *seckey, const char *passphrase, const char *path);

/**
 * @brief Write secret-key to file.
 * @param[in] seckey Secret-key to write.
 * @param[in] passphrase Optional passphrase to encrypt the secret-key with.
 * @param[in] path Path to the file.
 * @returns \c 0 on success or a negative \c errno from \c open(3) or \c write(3).
 */
int xbps_seckey_write(const struct xbps_seckey *seckey, const char *passphrase, const char *path);

/**
 * @struct xbps_sig
 * @brief Ed25519 signature.
 */
struct xbps_sig {
	/**
	 * @var sig_alg
	 * @brief Signature algorithm.
	 *
	 * The signature algorithm, currently only ::SIGALG_HASHED.
	 */
	uint8_t sig_alg[2];
	/**
	 * @var keynum
	 * @brief Key identifier.
	 *
	 * Cryptographically insecure. This should not be presented to users
	 * and is just used as a fastpath to check if a signature was signed
	 * by a different key.
	 */
	uint8_t keynum[KEYNUM_BYTES];
	/**
	 * @var sig
	 * @brief Ed25519 public-key signature.
	 *
	 * Detached Ed25519 pubkey-key signature.
	 *
	 * The signature depends on the used ::xbps_sig.sig_alg:
	 * - ::SIGALG_HASHED: Signed BLAKE2b hash of the message.
	 * - ::SIGALG: Signature of the message itself, not supported.
	 */
	uint8_t sig[SIG_BYTES];
};

/**
 * @struct xbps_minisig
 * @brief minisig signature
 */
struct xbps_minisig {
	/**
	 * @var comment
	 * @brief Untrusted comment in the .minisig file.
	 */
	char comment[COMMENTMAXBYTES];
	/**
	 * @var sig
	 * @brief Algorithm, keynum and signature of the signed data.
	 */
	struct xbps_sig sig;
	/**
	 * @var trusted_comment
	 * @brief Trusted comment in the .minisig file.
	 */
	char trusted_comment[TRUSTEDCOMMENTMAXBYTES];
	/**
	 * @var global_sig
	 * @brief Signature of ::xbps_minisig.sig and ::xbps_minisig.trusted_comment.
	 *
	 * The global signature signs the signature and the trusted comment.
	 */
	uint8_t global_sig[SIG_BYTES];
};

/**
 * @brief Read a minisig file.
 * @param[out] minisig ::xbps_minisig struct to store the read data.
 * @param[in] path Path to the .minisig file.
 * @returns \c 0 on success or a negative \c errno from \c open(3), \c read(3).
 * @retval -ENOBUFS Comments or the encoded signature exceed the maximum size.
 */
int xbps_minisig_read(struct xbps_minisig *minisig, const char *path);

/**
 * @brief Write a minisig file.
 * @param[in] minisig The ::xbps_minisig structure that is written
 * @param[in] path Path to write to
 * @returns \c 0 on success or a negative \c errno from \c open(3), \c write(3).
 */
int xbps_minisig_write(const struct xbps_minisig *minisig, const char *path);

/**
 * @brief Sign a minisig ::xbps_minisig.
 * @param[out] minisig The ::xbps_minisig that is being signed and stores the signatures.
 * @param[in] seckey Secret-key used to sign.
 * @param[in] hash The hash of the message that is begin signed.
 * @returns \c 0 on success or a negative \c errno.
 * @retval -EINVAL Signing failed.
 */
int xbps_minisig_sign(struct xbps_minisig *minisig, const struct xbps_seckey *seckey,
		const struct xbps_hash *hash);

/**
 * @brief Verify a minisig ::xbps_minisig.
 * @param[in] minisig The ::xbps_minisig that is being verified.
 * @param[in] pubkey Public-key used to verify the ::xbps_minisig.
 * @param[in] hash The hash of the message that is being verified.
 * @returns \c 0 on success or a negative \c errno.
 * @retval -EINVAL \c keynum of \p pubkey does not match signature.
 * @retval -ERANGE Signature verification failed.
 */
int xbps_minisig_verify(const struct xbps_minisig *minisig, const struct xbps_pubkey *pubkey,
		const struct xbps_hash *hash);

/**
 * @brief Generate a new key pair.
 *
 * Generates a new public- and secret-key pair.
 *
 * @param[out] seckey ::xbps_seckey to store the generated secret-key.
 * @param[out] pubkey ::xbps_pubkey to store the generated public-key.
 *
 * @return 0 on success or a negative \c errno.
 * @retval -EINVAL Failure during keypair generation.
 */
int xbps_generate_keypair(struct xbps_seckey *seckey, struct xbps_pubkey *pubkey);

/**@}*/

#endif
