/**
 * @file crypto_manager.h
 * @brief End-to-end encryption operations using X25519 ECDH + XChaCha20-Poly1305.
 *
 * All cryptographic operations are channeled through this module:
 * - Key generation (X25519 keypairs)
 * - Shared secret derivation (ECDH)
 * - Authenticated encryption / decryption (XChaCha20-Poly1305 AEAD)
 *
 * Uses libsodium exclusively — no hand-rolled crypto.
 */

#ifndef BITCHAT_USECASE_CRYPTO_MANAGER_H
#define BITCHAT_USECASE_CRYPTO_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include "bitchat/domain/message.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Size of a shared secret (output of ECDH). */
#define BC_SHARED_KEY_SIZE 32

/** Size of the XChaCha20-Poly1305 nonce. */
#define BC_NONCE_SIZE      24

/** Size of the AEAD authentication tag. */
#define BC_TAG_SIZE        16

/**
 * @brief Initialize the cryptographic subsystem.
 *
 * Must be called once before any other crypto function. Internally calls
 * @c sodium_init().
 *
 * @return BC_OK on success, BC_ERR_CRYPTO if libsodium init fails.
 */
int bc_crypto_init(void);

/**
 * @brief Generate a new X25519 keypair.
 *
 * @param pubkey_out  Output public key (BC_PUBKEY_SIZE bytes).
 * @param secret_out  Output secret key (BC_SECRET_KEY_SIZE bytes).
 * @return BC_OK on success.
 */
int bc_crypto_generate_keypair(uint8_t pubkey_out[BC_PUBKEY_SIZE],
                               uint8_t secret_out[BC_PUBKEY_SIZE]);

/**
 * @brief Derive a shared secret via X25519 ECDH.
 *
 * The shared secret is computed as: shared = scalarmult(my_secret, peer_pubkey),
 * then hashed through HSalsa20 (via crypto_box_beforenm) for key whitening.
 *
 * @param my_secret     Our secret key.
 * @param peer_pubkey   Peer's public key.
 * @param shared_out    Output shared key (BC_SHARED_KEY_SIZE bytes).
 * @return BC_OK on success, BC_ERR_CRYPTO on failure.
 */
int bc_crypto_derive_shared(const uint8_t my_secret[BC_PUBKEY_SIZE],
                            const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                            uint8_t shared_out[BC_SHARED_KEY_SIZE]);

/**
 * @brief Encrypt plaintext using XChaCha20-Poly1305.
 *
 * The nonce is generated randomly and written to @p nonce_out.
 * The ciphertext includes the 16-byte authentication tag appended.
 *
 * @param plaintext      Input data.
 * @param plaintext_len  Length of @p plaintext.
 * @param shared_key     Symmetric key (from ECDH derivation).
 * @param ciphertext_out Output buffer (must be >= plaintext_len + BC_TAG_SIZE).
 * @param nonce_out      Output nonce (BC_NONCE_SIZE bytes).
 * @return BC_OK on success, BC_ERR_CRYPTO on failure.
 */
int bc_crypto_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                      const uint8_t shared_key[BC_SHARED_KEY_SIZE],
                      uint8_t *ciphertext_out,
                      uint8_t nonce_out[BC_NONCE_SIZE]);

/**
 * @brief Decrypt ciphertext using XChaCha20-Poly1305.
 *
 * @param ciphertext     Input ciphertext (includes appended tag).
 * @param ciphertext_len Length of @p ciphertext (plaintext_len + BC_TAG_SIZE).
 * @param shared_key     Symmetric key.
 * @param nonce          Nonce used during encryption.
 * @param plaintext_out  Output buffer (must be >= ciphertext_len - BC_TAG_SIZE).
 * @return BC_OK on success, BC_ERR_CRYPTO if authentication fails.
 */
int bc_crypto_decrypt(const uint8_t *ciphertext, size_t ciphertext_len,
                      const uint8_t shared_key[BC_SHARED_KEY_SIZE],
                      const uint8_t nonce[BC_NONCE_SIZE],
                      uint8_t *plaintext_out);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_USECASE_CRYPTO_MANAGER_H */
