/**
 * @file message_store.h
 * @brief Store-and-forward business logic layer.
 *
 * Wraps the raw storage port with business rules: TTL-aware retention,
 * automatic cleanup, and peer-targeted retrieval.
 */

#ifndef BITCHAT_USECASE_MESSAGE_STORE_H
#define BITCHAT_USECASE_MESSAGE_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "bitchat/domain/message.h"
#include "bitchat/port/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default message retention period: 72 hours. */
#define BC_STORE_MAX_AGE_SEC (72 * 3600)

/**
 * @brief Message store instance.
 */
typedef struct bc_msg_store {
    bc_storage_t *storage;       /**< Underlying storage adapter.      */
    uint64_t      max_age_sec;   /**< Max retention (seconds).         */
} bc_msg_store_t;

/**
 * @brief Create a message store.
 *
 * @param storage      Storage adapter (ownership not transferred).
 * @param max_age_sec  Maximum message age (0 for default 72h).
 * @return New store, or NULL on failure.
 */
bc_msg_store_t *bc_store_create(bc_storage_t *storage, uint64_t max_age_sec);

/**
 * @brief Save a message for store-and-forward.
 *
 * @param store  Store instance.
 * @param msg    Message to persist.
 * @return BC_OK or negative error code.
 */
int bc_store_save(bc_msg_store_t *store, const bc_message_t *msg);

/**
 * @brief Retrieve pending messages for a specific peer.
 *
 * Caller owns the returned messages and must free them.
 *
 * @param store       Store instance.
 * @param peer_pubkey Recipient's public key.
 * @param msgs_out    Output array of messages.
 * @param count       Number of messages returned.
 * @return BC_OK or negative error code.
 */
int bc_store_get_for_peer(bc_msg_store_t *store,
                          const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                          bc_message_t ***msgs_out, size_t *count);

/**
 * @brief Delete a delivered message.
 *
 * @param store Store instance.
 * @param uuid  Message UUID.
 * @return BC_OK or negative error code.
 */
int bc_store_delete(bc_msg_store_t *store,
                    const uint8_t uuid[BC_UUID_SIZE]);

/**
 * @brief Clean up expired messages.
 *
 * @param store Store instance.
 * @return BC_OK or negative error code.
 */
int bc_store_cleanup(bc_msg_store_t *store);

/**
 * @brief Destroy the store.
 * @param store Instance to destroy (NULL-safe).
 */
void bc_store_destroy(bc_msg_store_t *store);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_USECASE_MESSAGE_STORE_H */
