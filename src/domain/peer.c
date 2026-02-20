/**
 * @file peer.c
 * @brief Implementation of peer identity operations.
 */

#include "bitchat/domain/peer.h"

#include <string.h>
#include <time.h>

void bc_peer_init(bc_peer_t *peer,
                  const uint8_t pubkey[BC_PUBKEY_SIZE],
                  bc_transport_type_t transport)
{
    if (!peer || !pubkey) {
        return;
    }
    memset(peer, 0, sizeof(*peer));
    memcpy(peer->pubkey, pubkey, BC_PUBKEY_SIZE);
    peer->transport = transport;
    peer->last_seen = (uint64_t)time(NULL);
    peer->rssi = 0;
}

void bc_peer_touch(bc_peer_t *peer)
{
    if (!peer) {
        return;
    }
    peer->last_seen = (uint64_t)time(NULL);
}
