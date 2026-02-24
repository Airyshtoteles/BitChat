/**
 * @file mesh_router.h
 * @brief Gossip Protocol engine with Store-and-Forward.
 *
 * The mesh router is the heart of BitChat's networking. It implements
 * epidemic-style gossip flooding with:
 * - **Fanout limit** (default 3): each message is forwarded to at most N
 *   neighbors, preventing broadcast storms.
 * - **TTL decrement**: messages expire after a fixed number of hops.
 * - **Deduplication**: a hash-set of recently-seen UUIDs prevents
 *   re-processing messages already handled.
 * - **Store-and-Forward**: messages for unreachable peers are persisted
 *   and retried when those peers come online.
 */

#ifndef BITCHAT_USECASE_MESH_ROUTER_H
#define BITCHAT_USECASE_MESH_ROUTER_H

#include <stddef.h>
#include <stdint.h>
#include "bitchat/domain/message.h"
#include "bitchat/domain/peer.h"
#include "bitchat/port/transport.h"
#include "bitchat/port/storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default gossip fanout (max neighbors to forward to per message). */
#define BC_DEFAULT_FANOUT       3

/** Default dedup cache capacity (number of UUIDs). */
#define BC_DEFAULT_DEDUP_CAP    10000

/** Default dedup entry max age in seconds (10 minutes). */
#define BC_DEFAULT_DEDUP_AGE    600

/** Store-and-forward rebroadcast interval in seconds. */
#define BC_SF_REBROADCAST_SEC   3

/**
 * @brief Router configuration.
 */
typedef struct bc_router_config {
    uint8_t  fanout;       /**< Max peers to forward each message to.   */
    size_t   dedup_cap;    /**< Max entries in the dedup cache.          */
    uint64_t dedup_max_age; /**< Max age of dedup entries (seconds).    */
} bc_router_config_t;

/**
 * @brief Single entry in the dedup hash set.
 */
typedef struct bc_dedup_entry {
    uint8_t  uuid[BC_UUID_SIZE]; /**< Message UUID.                      */
    uint64_t seen_at;            /**< Timestamp when first seen.         */
    uint8_t  occupied;           /**< 1 if slot is in use, 0 otherwise.  */
} bc_dedup_entry_t;

/**
 * @brief Callback invoked when a message addressed to us arrives.
 *
 * @param msg       The received message (caller retains ownership).
 * @param userdata  Opaque pointer passed during registration.
 */
typedef void (*bc_on_message_fn)(const bc_message_t *msg, void *userdata);

/**
 * @brief The mesh router instance.
 */
typedef struct bc_router {
    bc_transport_t     *transport;  /**< Network transport adapter.       */
    bc_storage_t       *storage;    /**< Persistent message store.        */
    bc_router_config_t  config;     /**< Operating parameters.            */

    /* Local identity (so we know which messages are for us). */
    uint8_t local_pubkey[BC_PUBKEY_SIZE];

    /* Dedup cache (open-addressed hash set). */
    bc_dedup_entry_t *dedup_table;
    size_t            dedup_cap;

    /* Known neighbors. */
    bc_peer_t peers[BC_MAX_PEERS];
    size_t    peer_count;

    /* Delivery callback. */
    bc_on_message_fn on_message;
    void            *on_message_userdata;

    /* Store-and-forward rate limiting. */
    uint64_t last_sf_tick;  /**< Last rebroadcast timestamp (seconds).  */
} bc_router_t;

/**
 * @brief Create and initialize a mesh router.
 *
 * @param transport   Transport adapter (ownership not transferred).
 * @param storage     Storage adapter (ownership not transferred).
 * @param local_pubkey Our public key (BC_PUBKEY_SIZE bytes).
 * @param config      Configuration (NULL for defaults).
 * @return New router, or NULL on failure.
 */
bc_router_t *bc_router_create(bc_transport_t *transport,
                              bc_storage_t *storage,
                              const uint8_t local_pubkey[BC_PUBKEY_SIZE],
                              const bc_router_config_t *config);

/**
 * @brief Register a callback for messages addressed to us.
 */
void bc_router_set_on_message(bc_router_t *router,
                              bc_on_message_fn callback,
                              void *userdata);

/**
 * @brief Broadcast a message into the mesh.
 *
 * The message's TTL is set and it is forwarded to up to @c fanout neighbors.
 * If no neighbors are reachable, the message is stored for later forwarding.
 *
 * @param router  Router instance.
 * @param msg     Message to broadcast (ownership not transferred).
 * @return BC_OK on success.
 */
int bc_router_broadcast(bc_router_t *router, const bc_message_t *msg);

/**
 * @brief Process a raw incoming payload from the transport layer.
 *
 * Deserializes the message, checks dedup, decrements TTL, delivers locally
 * if addressed to us, and re-broadcasts otherwise.
 *
 * @param router  Router instance.
 * @param data    Raw serialized message bytes.
 * @param len     Length of @p data.
 * @return BC_OK, BC_ERR_DUPLICATE, or BC_ERR_TTL_EXPIRED.
 */
int bc_router_receive(bc_router_t *router,
                      const uint8_t *data, size_t len);

/**
 * @brief Periodic maintenance tick.
 *
 * Should be called regularly (e.g. every 5-10 seconds). Actions:
 * - Evicts stale dedup entries.
 * - Polls the transport for incoming data.
 * - Retries store-and-forward for newly-discovered peers.
 *
 * @param router Router instance.
 */
void bc_router_tick(bc_router_t *router);

/**
 * @brief Notify the router that a new peer has been discovered.
 *
 * @param router  Router instance.
 * @param peer    Peer to add/update.
 */
void bc_router_add_peer(bc_router_t *router, const bc_peer_t *peer);

/**
 * @brief Destroy the router and free all resources.
 * @param router Router to destroy (NULL-safe).
 */
void bc_router_destroy(bc_router_t *router);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_USECASE_MESH_ROUTER_H */
