/**
 * @file message_store.c
 * @brief Implementation of the store-and-forward business logic.
 */

#include "bitchat/usecase/message_store.h"
#include "bitchat/error.h"

#include <stdlib.h>

bc_msg_store_t *bc_store_create(bc_storage_t *storage, uint64_t max_age_sec)
{
    if (!storage) {
        return NULL;
    }

    bc_msg_store_t *store = calloc(1, sizeof(*store));
    if (!store) {
        return NULL;
    }

    store->storage = storage;
    store->max_age_sec = (max_age_sec > 0) ? max_age_sec : BC_STORE_MAX_AGE_SEC;

    return store;
}

int bc_store_save(bc_msg_store_t *store, const bc_message_t *msg)
{
    if (!store || !msg) {
        return BC_ERR_INVALID;
    }
    if (!store->storage->store_message) {
        return BC_ERR_NOT_IMPLEMENTED;
    }
    return store->storage->store_message(store->storage, msg);
}

int bc_store_get_for_peer(bc_msg_store_t *store,
                          const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                          bc_message_t ***msgs_out, size_t *count)
{
    if (!store || !peer_pubkey || !msgs_out || !count) {
        return BC_ERR_INVALID;
    }
    if (!store->storage->get_pending) {
        return BC_ERR_NOT_IMPLEMENTED;
    }
    return store->storage->get_pending(store->storage, peer_pubkey,
                                       msgs_out, count);
}

int bc_store_delete(bc_msg_store_t *store,
                    const uint8_t uuid[BC_UUID_SIZE])
{
    if (!store || !uuid) {
        return BC_ERR_INVALID;
    }
    if (!store->storage->delete_message) {
        return BC_ERR_NOT_IMPLEMENTED;
    }
    return store->storage->delete_message(store->storage, uuid);
}

int bc_store_cleanup(bc_msg_store_t *store)
{
    if (!store) {
        return BC_ERR_INVALID;
    }
    if (!store->storage->evict_expired) {
        return BC_ERR_NOT_IMPLEMENTED;
    }
    return store->storage->evict_expired(store->storage, store->max_age_sec);
}

void bc_store_destroy(bc_msg_store_t *store)
{
    free(store);
}
