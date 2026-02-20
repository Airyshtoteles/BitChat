/**
 * @file identity.c
 * @brief Implementation of local identity (keypair) management.
 */

#include "bitchat/domain/identity.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <string.h>

#define BC_IDENTITY_SERIALIZED_SIZE (BC_PUBKEY_SIZE + BC_SECRET_KEY_SIZE)

int bc_identity_generate(bc_identity_t *id)
{
    if (!id) {
        return BC_ERR_INVALID;
    }

    if (crypto_box_keypair(id->pubkey, id->secret_key) != 0) {
        return BC_ERR_CRYPTO;
    }

    return BC_OK;
}

int bc_identity_serialize(const bc_identity_t *id,
                          uint8_t *buf, size_t buf_len)
{
    if (!id || !buf) {
        return BC_ERR_INVALID;
    }
    if (buf_len < BC_IDENTITY_SERIALIZED_SIZE) {
        return BC_ERR_BUFFER_TOO_SMALL;
    }

    memcpy(buf, id->pubkey, BC_PUBKEY_SIZE);
    memcpy(buf + BC_PUBKEY_SIZE, id->secret_key, BC_SECRET_KEY_SIZE);

    return BC_OK;
}

int bc_identity_deserialize(bc_identity_t *id,
                            const uint8_t *data, size_t data_len)
{
    if (!id || !data) {
        return BC_ERR_INVALID;
    }
    if (data_len < BC_IDENTITY_SERIALIZED_SIZE) {
        return BC_ERR_INVALID;
    }

    memcpy(id->pubkey, data, BC_PUBKEY_SIZE);
    memcpy(id->secret_key, data + BC_PUBKEY_SIZE, BC_SECRET_KEY_SIZE);

    return BC_OK;
}

void bc_identity_wipe(bc_identity_t *id)
{
    if (!id) {
        return;
    }
    sodium_memzero(id->secret_key, BC_SECRET_KEY_SIZE);
}
