/**
 * @file transport_ble.c
 * @brief BLE transport adapter (stub).
 *
 * This is a placeholder implementation. Real BLE I/O is platform-specific
 * (Android BluetoothGatt, iOS CoreBluetooth) and will be injected through
 * the bridge layer at runtime.
 */

#include "bitchat/port/transport.h"
#include "bitchat/error.h"

#include <stdlib.h>

static int ble_init(bc_transport_t *self)
{
    (void)self;
    return BC_ERR_NOT_IMPLEMENTED;
}

static int ble_send(bc_transport_t *self,
                    const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                    const uint8_t *data, size_t len)
{
    (void)self; (void)peer_pubkey; (void)data; (void)len;
    return BC_ERR_NOT_IMPLEMENTED;
}

static int ble_recv(bc_transport_t *self,
                    uint8_t *buf, size_t buf_len, size_t *out_len,
                    uint8_t peer_pubkey_out[BC_PUBKEY_SIZE])
{
    (void)self; (void)buf; (void)buf_len; (void)out_len; (void)peer_pubkey_out;
    return BC_ERR_NOT_IMPLEMENTED;
}

static int ble_discover(bc_transport_t *self,
                        bc_peer_t *peers_out, size_t max_peers,
                        size_t *found)
{
    (void)self; (void)peers_out; (void)max_peers;
    if (found) {
        *found = 0;
    }
    return BC_ERR_NOT_IMPLEMENTED;
}

static void ble_destroy(bc_transport_t *self)
{
    if (self) {
        free(self->ctx);
    }
}

bc_transport_t *bc_transport_ble_create(void)
{
    bc_transport_t *t = calloc(1, sizeof(*t));
    if (!t) {
        return NULL;
    }

    t->init           = ble_init;
    t->send           = ble_send;
    t->recv           = ble_recv;
    t->discover_peers = ble_discover;
    t->destroy        = ble_destroy;
    t->ctx            = NULL;

    return t;
}
