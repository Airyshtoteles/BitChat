/**
 * @file identity.h
 * @brief Local user identity based on an X25519 keypair.
 *
 * In BitChat, identity is defined solely by a cryptographic keypair.
 * There are no usernames, phone numbers, or centralized accounts.
 */

#ifndef BITCHAT_DOMAIN_IDENTITY_H
#define BITCHAT_DOMAIN_IDENTITY_H

#include <stdint.h>
#include "bitchat/domain/message.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Size of a secret key in bytes (X25519). */
#define BC_SECRET_KEY_SIZE 32

/**
 * @brief Local identity holding the user's keypair.
 */
typedef struct bc_identity {
    uint8_t pubkey[BC_PUBKEY_SIZE];        /**< Public key (shared freely).  */
    uint8_t secret_key[BC_SECRET_KEY_SIZE]; /**< Secret key (never leaves device). */
} bc_identity_t;

/**
 * @brief Generate a new random identity (keypair).
 *
 * Uses libsodium's CSPRNG internally.
 *
 * @param id  Output identity struct.
 * @return BC_OK on success, BC_ERR_CRYPTO on failure.
 */
int bc_identity_generate(bc_identity_t *id);

/**
 * @brief Serialize an identity to a byte buffer for secure storage.
 *
 * @param id       Identity to serialize.
 * @param buf      Output buffer (must be >= BC_PUBKEY_SIZE + BC_SECRET_KEY_SIZE).
 * @param buf_len  Size of @p buf.
 * @return BC_OK on success, BC_ERR_BUFFER_TOO_SMALL if buf is too small.
 */
int bc_identity_serialize(const bc_identity_t *id,
                          uint8_t *buf, size_t buf_len);

/**
 * @brief Deserialize an identity from a byte buffer.
 *
 * @param id       Output identity struct.
 * @param data     Input buffer.
 * @param data_len Length of @p data.
 * @return BC_OK on success, BC_ERR_INVALID if data is malformed.
 */
int bc_identity_deserialize(bc_identity_t *id,
                            const uint8_t *data, size_t data_len);

/**
 * @brief Securely wipe the secret key from memory.
 * @param id Identity to wipe.
 */
void bc_identity_wipe(bc_identity_t *id);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_DOMAIN_IDENTITY_H */
