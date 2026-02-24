/**
 * @file storage.h
 * @brief Abstract storage interface (vtable) for message persistence.
 *
 * The store-and-forward mechanism requires messages to survive app restarts.
 * This interface abstracts the underlying storage engine (SQLite, flat file,
 * etc.) so the use-case layer remains decoupled.
 */

#ifndef BITCHAT_PORT_STORAGE_H
#define BITCHAT_PORT_STORAGE_H

#include <stddef.h>
#include <stdint.h>
#include "bitchat/domain/message.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Abstract storage interface.
 */
typedef struct bc_storage {
    /**
     * @brief Persist a message for store-and-forward.
     * @param msg Message to store (copied internally).
     * @return BC_OK or negative error code.
     */
    int (*store_message)(struct bc_storage *self, const bc_message_t *msg);

    /**
     * @brief Retrieve pending messages, optionally filtered by recipient.
     *
     * If @p peer_pubkey is non-NULL, returns only messages destined for
     * that specific recipient. If @p peer_pubkey is NULL, returns ALL
     * non-expired pending messages (used for gossip-style broadcast sync).
     *
     * The caller owns the returned array and each message within it.
     * Free each message with @ref bc_message_destroy, then free the array.
     *
     * @param peer_pubkey  Recipient public key to filter by, or NULL for all.
     * @param msgs_out     Pointer to output array.
     * @param count        Number of messages returned.
     * @return BC_OK or negative error code.
     */
    int (*get_pending)(struct bc_storage *self,
                       const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                       bc_message_t ***msgs_out, size_t *count);

    /**
     * @brief Delete a message by UUID (after successful delivery).
     * @param uuid  Message UUID (BC_UUID_SIZE bytes).
     * @return BC_OK or negative error code.
     */
    int (*delete_message)(struct bc_storage *self,
                          const uint8_t uuid[BC_UUID_SIZE]);

    /**
     * @brief Evict messages older than @p max_age_sec seconds.
     * @param max_age_sec  Maximum message age in seconds.
     * @return BC_OK or negative error code.
     */
    int (*evict_expired)(struct bc_storage *self, uint64_t max_age_sec);

    /**
     * @brief Tear down storage and free resources.
     */
    void (*destroy)(struct bc_storage *self);

    /** Adapter-private context. */
    void *ctx;
} bc_storage_t;

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_PORT_STORAGE_H */
