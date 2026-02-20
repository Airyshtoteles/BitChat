/**
 * @file peer.h
 * @brief Peer identity and connection metadata.
 */

#ifndef BITCHAT_DOMAIN_PEER_H
#define BITCHAT_DOMAIN_PEER_H

#include <stdint.h>
#include "bitchat/domain/message.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of peers tracked simultaneously. */
#define BC_MAX_PEERS 64

/** Transport type through which a peer was discovered. */
typedef enum bc_transport_type {
    BC_TRANSPORT_BLE         = 0,
    BC_TRANSPORT_WIFI_DIRECT = 1
} bc_transport_type_t;

/**
 * @brief A known peer in the mesh network.
 */
typedef struct bc_peer {
    uint8_t            pubkey[BC_PUBKEY_SIZE]; /**< Peer's public key.          */
    uint64_t           last_seen;              /**< Last contact (UNIX secs).   */
    bc_transport_type_t transport;             /**< How we reach this peer.     */
    int8_t             rssi;                   /**< Signal strength (dBm).      */
} bc_peer_t;

/**
 * @brief Initialize a peer struct.
 *
 * @param peer       Peer to initialize.
 * @param pubkey     Public key (BC_PUBKEY_SIZE bytes).
 * @param transport  Transport type.
 */
void bc_peer_init(bc_peer_t *peer,
                  const uint8_t pubkey[BC_PUBKEY_SIZE],
                  bc_transport_type_t transport);

/**
 * @brief Update the last-seen timestamp to the current time.
 * @param peer Peer to touch.
 */
void bc_peer_touch(bc_peer_t *peer);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_DOMAIN_PEER_H */
