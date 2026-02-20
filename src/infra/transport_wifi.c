/**
 * @file transport_wifi.c
 * @brief Wi-Fi Direct transport adapter (stub).
 *
 * Placeholder implementation. Real Wi-Fi Direct I/O is platform-specific
 * and will be injected through the bridge layer at runtime.
 */

#include "bitchat/port/transport.h"
#include "bitchat/error.h"

#include <stdlib.h>

static int wifi_init(bc_transport_t *self)
{
    (void)self;
    return BC_ERR_NOT_IMPLEMENTED;
}

static int wifi_send(bc_transport_t *self,
                     const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                     const uint8_t *data, size_t len)
{
    (void)self; (void)peer_pubkey; (void)data; (void)len;
    return BC_ERR_NOT_IMPLEMENTED;
}

static int wifi_recv(bc_transport_t *self,
                     uint8_t *buf, size_t buf_len, size_t *out_len,
                     uint8_t peer_pubkey_out[BC_PUBKEY_SIZE])
{
    (void)self; (void)buf; (void)buf_len; (void)out_len; (void)peer_pubkey_out;
    return BC_ERR_NOT_IMPLEMENTED;
}

static int wifi_discover(bc_transport_t *self,
                         bc_peer_t *peers_out, size_t max_peers,
                         size_t *found)
{
    (void)self; (void)peers_out; (void)max_peers;
    if (found) {
        *found = 0;
    }
    return BC_ERR_NOT_IMPLEMENTED;
}

static void wifi_destroy(bc_transport_t *self)
{
    if (self) {
        free(self->ctx);
    }
}

bc_transport_t *bc_transport_wifi_create(void)
{
    bc_transport_t *t = calloc(1, sizeof(*t));
    if (!t) {
        return NULL;
    }

    t->init           = wifi_init;
    t->send           = wifi_send;
    t->recv           = wifi_recv;
    t->discover_peers = wifi_discover;
    t->destroy        = wifi_destroy;
    t->ctx            = NULL;

    return t;
}
