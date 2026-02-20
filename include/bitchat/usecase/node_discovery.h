/**
 * @file node_discovery.h
 * @brief Peer discovery manager.
 *
 * Coordinates peer scanning across transports and maintains a list of
 * known neighbors. When a new peer is found, registered callbacks are
 * invoked so the router can begin exchanging messages.
 */

#ifndef BITCHAT_USECASE_NODE_DISCOVERY_H
#define BITCHAT_USECASE_NODE_DISCOVERY_H

#include <stddef.h>
#include <stdint.h>
#include "bitchat/domain/peer.h"
#include "bitchat/port/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback invoked when a new peer is discovered.
 *
 * @param peer      The discovered peer.
 * @param userdata  Opaque pointer from registration.
 */
typedef void (*bc_on_peer_found_fn)(const bc_peer_t *peer, void *userdata);

/**
 * @brief Discovery manager instance.
 */
typedef struct bc_discovery {
    bc_transport_t *transport;     /**< Transport used for scanning.   */

    bc_peer_t peers[BC_MAX_PEERS]; /**< Known peers.                   */
    size_t    peer_count;          /**< Current number of known peers.  */

    bc_on_peer_found_fn on_found;  /**< New-peer callback.              */
    void               *userdata;  /**< Callback context.               */
} bc_discovery_t;

/**
 * @brief Create a discovery manager.
 *
 * @param transport  Transport adapter for scanning.
 * @return New discovery instance, or NULL on failure.
 */
bc_discovery_t *bc_discovery_create(bc_transport_t *transport);

/**
 * @brief Register a callback for newly discovered peers.
 */
void bc_discovery_on_peer_found(bc_discovery_t *disc,
                                bc_on_peer_found_fn callback,
                                void *userdata);

/**
 * @brief Trigger a peer scan.
 *
 * Calls the transport's discover_peers function. For each newly-seen peer,
 * the registered callback is invoked.
 *
 * @param disc Discovery instance.
 * @return BC_OK on success, or negative error code.
 */
int bc_discovery_scan(bc_discovery_t *disc);

/**
 * @brief Get the current list of known peers.
 *
 * @param disc      Discovery instance.
 * @param peers_out Array to fill.
 * @param max       Maximum entries.
 * @return Number of peers copied.
 */
size_t bc_discovery_get_peers(const bc_discovery_t *disc,
                              bc_peer_t *peers_out, size_t max);

/**
 * @brief Destroy the discovery manager.
 * @param disc Instance to destroy (NULL-safe).
 */
void bc_discovery_destroy(bc_discovery_t *disc);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_USECASE_NODE_DISCOVERY_H */
