/**
 * @file transport_udp_sim.h
 * @brief UDP-based transport adapter for localhost simulation.
 *
 * Simulates a mesh network on a single machine by mapping each "node"
 * to a UDP port on 127.0.0.1. Messages are broadcast to all configured
 * peer ports, mimicking BLE/Wi-Fi Direct range.
 */

#ifndef BITCHAT_INFRA_TRANSPORT_UDP_SIM_H
#define BITCHAT_INFRA_TRANSPORT_UDP_SIM_H

#include "bitchat/port/transport.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a UDP transport adapter bound to 127.0.0.1:@p my_port.
 *
 * @param my_port     Local port to bind.
 * @param peer_ports  Array of peer ports (neighbors in the simulated mesh).
 * @param peer_count  Number of entries in @p peer_ports.
 * @return Fully wired bc_transport_t, or NULL on failure.
 */
bc_transport_t *bc_transport_udp_create(uint16_t my_port,
                                        const uint16_t *peer_ports,
                                        size_t peer_count);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_INFRA_TRANSPORT_UDP_SIM_H */
