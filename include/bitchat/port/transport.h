/**
 * @file transport.h
 * @brief Abstract transport interface (vtable) for BLE and Wi-Fi Direct.
 *
 * This is the Dependency Inversion boundary. The use-case layer programs
 * against this interface; concrete adapters (BLE, Wi-Fi Direct) implement it.
 * The vtable pattern gives us polymorphism in plain C.
 */

#ifndef BITCHAT_PORT_TRANSPORT_H
#define BITCHAT_PORT_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include "bitchat/domain/peer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Abstract transport interface.
 *
 * Each concrete transport (BLE, Wi-Fi Direct) provides a filled-in instance
 * of this struct. The @c ctx pointer carries adapter-private state.
 */
typedef struct bc_transport {
    /**
     * @brief Initialize the transport hardware/subsystem.
     * @return BC_OK on success, or a negative error code.
     */
    int (*init)(struct bc_transport *self);

    /**
     * @brief Send raw bytes to a specific peer.
     *
     * @param peer_pubkey  Recipient's public key (used to look up connection).
     * @param data         Bytes to send.
     * @param len          Number of bytes.
     * @return BC_OK or negative error code.
     */
    int (*send)(struct bc_transport *self,
                const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                const uint8_t *data, size_t len);

    /**
     * @brief Receive raw bytes from any peer (non-blocking).
     *
     * @param buf             Output buffer.
     * @param buf_len         Size of @p buf.
     * @param out_len         Actual bytes received.
     * @param peer_pubkey_out Public key of the sender.
     * @return BC_OK if data was received, BC_ERR if nothing available.
     */
    int (*recv)(struct bc_transport *self,
                uint8_t *buf, size_t buf_len, size_t *out_len,
                uint8_t peer_pubkey_out[BC_PUBKEY_SIZE]);

    /**
     * @brief Scan for nearby peers.
     *
     * @param peers_out  Array to fill with discovered peers.
     * @param max_peers  Maximum entries to write.
     * @param found      Actual number found.
     * @return BC_OK or negative error code.
     */
    int (*discover_peers)(struct bc_transport *self,
                          bc_peer_t *peers_out, size_t max_peers,
                          size_t *found);

    /**
     * @brief Tear down the transport and free resources.
     */
    void (*destroy)(struct bc_transport *self);

    /** Adapter-private context (opaque pointer). */
    void *ctx;
} bc_transport_t;

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_PORT_TRANSPORT_H */
