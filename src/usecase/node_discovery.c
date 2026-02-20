/**
 * @file node_discovery.c
 * @brief Implementation of the peer discovery manager.
 */

#include "bitchat/usecase/node_discovery.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <stdlib.h>
#include <string.h>

bc_discovery_t *bc_discovery_create(bc_transport_t *transport)
{
    bc_discovery_t *disc = calloc(1, sizeof(*disc));
    if (!disc) {
        return NULL;
    }
    disc->transport = transport;
    return disc;
}

void bc_discovery_on_peer_found(bc_discovery_t *disc,
                                bc_on_peer_found_fn callback,
                                void *userdata)
{
    if (!disc) {
        return;
    }
    disc->on_found = callback;
    disc->userdata = userdata;
}

int bc_discovery_scan(bc_discovery_t *disc)
{
    if (!disc || !disc->transport || !disc->transport->discover_peers) {
        return BC_ERR_INVALID;
    }

    bc_peer_t found[BC_MAX_PEERS];
    size_t found_count = 0;

    int rc = disc->transport->discover_peers(disc->transport,
                                             found, BC_MAX_PEERS,
                                             &found_count);
    if (rc != BC_OK) {
        return rc;
    }

    for (size_t i = 0; i < found_count; i++) {
        /* Check if this peer is already known. */
        int is_new = 1;
        for (size_t j = 0; j < disc->peer_count; j++) {
            if (sodium_memcmp(disc->peers[j].pubkey,
                              found[i].pubkey, BC_PUBKEY_SIZE) == 0) {
                /* Existing peer — update last_seen. */
                bc_peer_touch(&disc->peers[j]);
                disc->peers[j].rssi = found[i].rssi;
                is_new = 0;
                break;
            }
        }

        if (is_new && disc->peer_count < BC_MAX_PEERS) {
            disc->peers[disc->peer_count] = found[i];
            disc->peer_count++;

            if (disc->on_found) {
                disc->on_found(&found[i], disc->userdata);
            }
        }
    }

    return BC_OK;
}

size_t bc_discovery_get_peers(const bc_discovery_t *disc,
                              bc_peer_t *peers_out, size_t max)
{
    if (!disc || !peers_out || max == 0) {
        return 0;
    }

    size_t n = (disc->peer_count < max) ? disc->peer_count : max;
    memcpy(peers_out, disc->peers, n * sizeof(bc_peer_t));
    return n;
}

void bc_discovery_destroy(bc_discovery_t *disc)
{
    free(disc);
}
