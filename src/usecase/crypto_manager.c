/**
 * @file crypto_manager.c
 * @brief Implementation of E2EE operations using libsodium.
 */

#include "bitchat/usecase/crypto_manager.h"
#include "bitchat/domain/identity.h"
#include "bitchat/error.h"

#include <sodium.h>

int bc_crypto_init(void)
{
    if (sodium_init() < 0) {
        return BC_ERR_CRYPTO;
    }
    return BC_OK;
}

int bc_crypto_generate_keypair(uint8_t pubkey_out[BC_PUBKEY_SIZE],
                               uint8_t secret_out[BC_PUBKEY_SIZE])
{
    if (!pubkey_out || !secret_out) {
        return BC_ERR_INVALID;
    }
    crypto_box_keypair(pubkey_out, secret_out);
    return BC_OK;
}

int bc_crypto_derive_shared(const uint8_t my_secret[BC_PUBKEY_SIZE],
                            const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                            uint8_t shared_out[BC_SHARED_KEY_SIZE])
{
    if (!my_secret || !peer_pubkey || !shared_out) {
        return BC_ERR_INVALID;
    }

    /*
     * crypto_box_beforenm computes:
     *   shared = HSalsa20(scalarmult(my_secret, peer_pubkey))
     * The HSalsa20 step acts as a key whitening / KDF.
     */
    if (crypto_box_beforenm(shared_out, peer_pubkey, my_secret) != 0) {
        return BC_ERR_CRYPTO;
    }

    return BC_OK;
}

int bc_crypto_encrypt(const uint8_t *plaintext, size_t plaintext_len,
                      const uint8_t shared_key[BC_SHARED_KEY_SIZE],
                      uint8_t *ciphertext_out,
                      uint8_t nonce_out[BC_NONCE_SIZE])
{
    if (!plaintext || !shared_key || !ciphertext_out || !nonce_out) {
        return BC_ERR_INVALID;
    }

    /* Generate a random 192-bit nonce. */
    randombytes_buf(nonce_out, BC_NONCE_SIZE);

    /*
     * XChaCha20-Poly1305 AEAD.
     * Output: ciphertext || 16-byte auth tag.
     */
    if (crypto_aead_xchacha20poly1305_ietf_encrypt(
            ciphertext_out, NULL,
            plaintext, (unsigned long long)plaintext_len,
            NULL, 0,   /* no additional data */
            NULL,      /* nsec (unused) */
            nonce_out,
            shared_key) != 0) {
        return BC_ERR_CRYPTO;
    }

    return BC_OK;
}

int bc_crypto_decrypt(const uint8_t *ciphertext, size_t ciphertext_len,
                      const uint8_t shared_key[BC_SHARED_KEY_SIZE],
                      const uint8_t nonce[BC_NONCE_SIZE],
                      uint8_t *plaintext_out)
{
    if (!ciphertext || !shared_key || !nonce || !plaintext_out) {
        return BC_ERR_INVALID;
    }

    if (ciphertext_len < BC_TAG_SIZE) {
        return BC_ERR_INVALID;
    }

    if (crypto_aead_xchacha20poly1305_ietf_decrypt(
            plaintext_out, NULL,
            NULL,      /* nsec (unused) */
            ciphertext, (unsigned long long)ciphertext_len,
            NULL, 0,   /* no additional data */
            nonce,
            shared_key) != 0) {
        return BC_ERR_CRYPTO;
    }

    return BC_OK;
}
