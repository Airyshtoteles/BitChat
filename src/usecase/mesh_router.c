/**
 * @file mesh_router.c
 * @brief Implementation of the Gossip Protocol mesh router.
 */

#include "bitchat/usecase/mesh_router.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------- dedup hash set internals ---------- */

/**
 * @brief FNV-1a hash of a UUID, used to index into the dedup table.
 */
static size_t dedup_hash(const uint8_t uuid[BC_UUID_SIZE], size_t cap)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < BC_UUID_SIZE; i++) {
        h ^= uuid[i];
        h *= 16777619u;
    }
    return (size_t)(h % cap);
}

/**
 * @brief Check if a UUID is already in the dedup cache.
 * @return 1 if duplicate, 0 if new.
 */
static int dedup_contains(const bc_router_t *router,
                          const uint8_t uuid[BC_UUID_SIZE])
{
    size_t idx = dedup_hash(uuid, router->dedup_cap);

    /* Linear probing. */
    for (size_t i = 0; i < router->dedup_cap; i++) {
        size_t slot = (idx + i) % router->dedup_cap;
        const bc_dedup_entry_t *e = &router->dedup_table[slot];

        if (!e->occupied) {
            return 0;
        }
        if (sodium_memcmp(e->uuid, uuid, BC_UUID_SIZE) == 0) {
            return 1;
        }
    }
    return 0; /* table full, treat as not found */
}

/**
 * @brief Insert a UUID into the dedup cache.
 */
static void dedup_insert(bc_router_t *router,
                         const uint8_t uuid[BC_UUID_SIZE])
{
    size_t idx = dedup_hash(uuid, router->dedup_cap);
    uint64_t now = (uint64_t)time(NULL);

    for (size_t i = 0; i < router->dedup_cap; i++) {
        size_t slot = (idx + i) % router->dedup_cap;
        bc_dedup_entry_t *e = &router->dedup_table[slot];

        if (!e->occupied) {
            memcpy(e->uuid, uuid, BC_UUID_SIZE);
            e->seen_at = now;
            e->occupied = 1;
            return;
        }

        /* Overwrite expired entries. */
        if (now - e->seen_at > router->config.dedup_max_age) {
            memcpy(e->uuid, uuid, BC_UUID_SIZE);
            e->seen_at = now;
            return;
        }
    }

    /* Table is full of fresh entries — overwrite the probed slot. */
    size_t slot = idx % router->dedup_cap;
    memcpy(router->dedup_table[slot].uuid, uuid, BC_UUID_SIZE);
    router->dedup_table[slot].seen_at = now;
}

/**
 * @brief Evict entries older than dedup_max_age.
 */
static void dedup_evict(bc_router_t *router)
{
    uint64_t now = (uint64_t)time(NULL);

    for (size_t i = 0; i < router->dedup_cap; i++) {
        bc_dedup_entry_t *e = &router->dedup_table[i];
        if (e->occupied && (now - e->seen_at > router->config.dedup_max_age)) {
            e->occupied = 0;
        }
    }
}

/* ---------- internal forwarding ---------- */

/**
 * @brief Forward a serialized message to up to `fanout` neighbors.
 */
static int forward_to_peers(bc_router_t *router,
                            const uint8_t *data, size_t len)
{
    if (!router->transport || !router->transport->send) {
        return BC_ERR_TRANSPORT;
    }

    size_t sent = 0;
    for (size_t i = 0; i < router->peer_count && sent < router->config.fanout; i++) {
        int rc = router->transport->send(router->transport,
                                         router->peers[i].pubkey,
                                         data, len);
        if (rc == BC_OK) {
            sent++;
        }
    }

    return (sent > 0) ? BC_OK : BC_ERR_TRANSPORT;
}

/* ---------- public API ---------- */

bc_router_t *bc_router_create(bc_transport_t *transport,
                              bc_storage_t *storage,
                              const uint8_t local_pubkey[BC_PUBKEY_SIZE],
                              const bc_router_config_t *config)
{
    if (!local_pubkey) {
        return NULL;
    }

    bc_router_t *r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }

    r->transport = transport;
    r->storage = storage;
    memcpy(r->local_pubkey, local_pubkey, BC_PUBKEY_SIZE);

    /* Apply configuration or defaults. */
    if (config) {
        r->config = *config;
    } else {
        r->config.fanout       = BC_DEFAULT_FANOUT;
        r->config.dedup_cap    = BC_DEFAULT_DEDUP_CAP;
        r->config.dedup_max_age = BC_DEFAULT_DEDUP_AGE;
    }

    /* Allocate dedup table. */
    r->dedup_cap = r->config.dedup_cap;
    r->dedup_table = calloc(r->dedup_cap, sizeof(bc_dedup_entry_t));
    if (!r->dedup_table) {
        free(r);
        return NULL;
    }

    return r;
}

void bc_router_set_on_message(bc_router_t *router,
                              bc_on_message_fn callback,
                              void *userdata)
{
    if (!router) {
        return;
    }
    router->on_message = callback;
    router->on_message_userdata = userdata;
}

int bc_router_broadcast(bc_router_t *router, const bc_message_t *msg)
{
    if (!router || !msg) {
        return BC_ERR_INVALID;
    }

    /* Mark as seen so we don't re-process our own message. */
    dedup_insert(router, msg->uuid);

    /* Serialize. */
    size_t needed = BC_MESSAGE_HEADER_SIZE + msg->payload_len;
    uint8_t *buf = malloc(needed);
    if (!buf) {
        return BC_ERR_NOMEM;
    }

    size_t out_len;
    int rc = bc_message_serialize(msg, buf, needed, &out_len);
    if (rc != BC_OK) {
        free(buf);
        return rc;
    }

    /* Try to send to peers. */
    rc = forward_to_peers(router, buf, out_len);

    /* If no peers reachable, store for later. */
    if (rc != BC_OK && router->storage && router->storage->store_message) {
        router->storage->store_message(router->storage, msg);
    }

    free(buf);
    return BC_OK;
}

int bc_router_receive(bc_router_t *router,
                      const uint8_t *data, size_t len)
{
    if (!router || !data || len == 0) {
        return BC_ERR_INVALID;
    }

    bc_message_t *msg = bc_message_deserialize(data, len);
    if (!msg) {
        return BC_ERR_INVALID;
    }

    /* Dedup check. */
    if (dedup_contains(router, msg->uuid)) {
        bc_message_destroy(msg);
        return BC_ERR_DUPLICATE;
    }
    dedup_insert(router, msg->uuid);

    /* TTL check. */
    if (msg->ttl == 0) {
        bc_message_destroy(msg);
        return BC_ERR_TTL_EXPIRED;
    }
    msg->ttl--;

    /* Is this message for us? */
    if (sodium_memcmp(msg->receiver_pubkey,
                      router->local_pubkey, BC_PUBKEY_SIZE) == 0) {
        if (router->on_message) {
            router->on_message(msg, router->on_message_userdata);
        }
        bc_message_destroy(msg);
        return BC_OK;
    }

    /* Not for us — re-serialize with decremented TTL and forward. */
    size_t needed = BC_MESSAGE_HEADER_SIZE + msg->payload_len;
    uint8_t *buf = malloc(needed);
    if (buf) {
        size_t out_len;
        if (bc_message_serialize(msg, buf, needed, &out_len) == BC_OK) {
            int rc = forward_to_peers(router, buf, out_len);
            /* Store if forwarding failed (no peers). */
            if (rc != BC_OK && router->storage &&
                router->storage->store_message) {
                router->storage->store_message(router->storage, msg);
            }
        }
        free(buf);
    }

    bc_message_destroy(msg);
    return BC_OK;
}

void bc_router_add_peer(bc_router_t *router, const bc_peer_t *peer)
{
    if (!router || !peer) {
        return;
    }

    /* Update existing peer or add new one. */
    for (size_t i = 0; i < router->peer_count; i++) {
        if (sodium_memcmp(router->peers[i].pubkey,
                          peer->pubkey, BC_PUBKEY_SIZE) == 0) {
            router->peers[i] = *peer;
            bc_peer_touch(&router->peers[i]);
            return;
        }
    }

    if (router->peer_count < BC_MAX_PEERS) {
        router->peers[router->peer_count] = *peer;
        router->peer_count++;
    }
}

void bc_router_tick(bc_router_t *router)
{
    if (!router) {
        return;
    }

    /* 1. Evict stale dedup entries. */
    dedup_evict(router);

    /* 2. Poll transport for incoming data. */
    if (router->transport && router->transport->recv) {
        uint8_t buf[4096];
        size_t  out_len = 0;
        uint8_t sender_pubkey[BC_PUBKEY_SIZE];

        while (router->transport->recv(router->transport,
                                       buf, sizeof(buf), &out_len,
                                       sender_pubkey) == BC_OK && out_len > 0) {
            bc_router_receive(router, buf, out_len);
            out_len = 0;
        }
    }

    /* 3. Retry store-and-forward for known peers. */
    if (router->storage && router->storage->get_pending) {
        for (size_t i = 0; i < router->peer_count; i++) {
            bc_message_t **msgs = NULL;
            size_t count = 0;

            if (router->storage->get_pending(router->storage,
                                             router->peers[i].pubkey,
                                             &msgs, &count) != BC_OK) {
                continue;
            }

            for (size_t j = 0; j < count; j++) {
                size_t needed = BC_MESSAGE_HEADER_SIZE + msgs[j]->payload_len;
                uint8_t *buf = malloc(needed);
                if (buf) {
                    size_t out_len;
                    if (bc_message_serialize(msgs[j], buf, needed,
                                            &out_len) == BC_OK) {
                        if (router->transport->send(router->transport,
                                                    router->peers[i].pubkey,
                                                    buf, out_len) == BC_OK) {
                            router->storage->delete_message(
                                router->storage, msgs[j]->uuid);
                        }
                    }
                    free(buf);
                }
                bc_message_destroy(msgs[j]);
            }
            free(msgs);
        }
    }

    /* 4. Evict expired stored messages. */
    if (router->storage && router->storage->evict_expired) {
        router->storage->evict_expired(router->storage, 72 * 3600);
    }
}

void bc_router_destroy(bc_router_t *router)
{
    if (!router) {
        return;
    }
    free(router->dedup_table);
    free(router);
}
