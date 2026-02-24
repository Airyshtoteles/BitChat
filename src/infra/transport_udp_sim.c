/**
 * @file transport_udp_sim.c
 * @brief UDP transport adapter for localhost mesh simulation.
 *
 * Each simulated node binds a UDP socket on 127.0.0.1:<port>.
 * - send():  broadcasts the serialized message to ALL configured peer ports
 *            via sendto(), simulating BLE/Wi-Fi Direct range.
 * - recv():  uses recvfrom() with MSG_DONTWAIT for non-blocking reads.
 * - discover_peers(): returns synthetic peers derived from the configured
 *            peer ports so the router has entries in its forwarding table.
 */

#define _POSIX_C_SOURCE 200809L

#include "bitchat/infra/transport_udp_sim.h"
#include "bitchat/port/transport.h"
#include "bitchat/error.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* ---------- private context ---------- */

typedef struct udp_ctx {
    int       sockfd;
    uint16_t  my_port;
    uint16_t *peer_ports;
    size_t    peer_count;
} udp_ctx_t;

/* ---------- vtable implementations ---------- */

static int udp_init(bc_transport_t *self)
{
    (void)self;
    return BC_OK; /* socket is already open from create() */
}

/**
 * Broadcast raw bytes to every configured peer port via sendto().
 * The peer_pubkey parameter is ignored — in the simulator all peers
 * are identified by port number, not by cryptographic identity.
 */
static int udp_send(bc_transport_t *self,
                    const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                    const uint8_t *data, size_t len)
{
    (void)peer_pubkey;

    if (!self || !data || len == 0) {
        return BC_ERR_INVALID;
    }

    udp_ctx_t *ctx = (udp_ctx_t *)self->ctx;
    int sent_any = 0;

    for (size_t i = 0; i < ctx->peer_count; i++) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(ctx->peer_ports[i]);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        ssize_t n = sendto(ctx->sockfd, data, len, 0,
                           (struct sockaddr *)&addr, sizeof(addr));
        if (n == (ssize_t)len) {
            sent_any = 1;
        }
    }

    return sent_any ? BC_OK : BC_ERR_TRANSPORT;
}

/**
 * Non-blocking receive via recvfrom() with MSG_DONTWAIT.
 * Returns BC_OK if data was read, BC_ERR if nothing available.
 */
static int udp_recv(bc_transport_t *self,
                    uint8_t *buf, size_t buf_len, size_t *out_len,
                    uint8_t peer_pubkey_out[BC_PUBKEY_SIZE])
{
    if (!self || !buf || !out_len) {
        return BC_ERR_INVALID;
    }

    udp_ctx_t *ctx = (udp_ctx_t *)self->ctx;

    struct sockaddr_in src_addr;
    socklen_t addr_len = sizeof(src_addr);

    ssize_t n = recvfrom(ctx->sockfd, buf, buf_len, MSG_DONTWAIT,
                         (struct sockaddr *)&src_addr, &addr_len);

    if (n <= 0) {
        *out_len = 0;
        return BC_ERR;
    }

    *out_len = (size_t)n;

    /* We do not know the sender's real pubkey from UDP alone.
       The pubkey is embedded in the serialized message itself. */
    if (peer_pubkey_out) {
        memset(peer_pubkey_out, 0, BC_PUBKEY_SIZE);
    }

    return BC_OK;
}

/**
 * Return synthetic peers derived from configured peer ports.
 * Each peer gets a pubkey that encodes its port number in the first
 * two bytes (little-endian), with the rest zeroed. This is enough
 * to get entries into the router's forwarding table.
 */
static int udp_discover_peers(bc_transport_t *self,
                              bc_peer_t *peers_out, size_t max_peers,
                              size_t *found)
{
    if (!self || !found) {
        return BC_ERR_INVALID;
    }

    udp_ctx_t *ctx = (udp_ctx_t *)self->ctx;

    size_t n = ctx->peer_count;
    if (n > max_peers) {
        n = max_peers;
    }

    for (size_t i = 0; i < n; i++) {
        memset(&peers_out[i], 0, sizeof(bc_peer_t));
        /* Encode port in first 2 bytes of the synthetic pubkey. */
        peers_out[i].pubkey[0] = (uint8_t)(ctx->peer_ports[i] & 0xFF);
        peers_out[i].pubkey[1] = (uint8_t)((ctx->peer_ports[i] >> 8) & 0xFF);
        peers_out[i].transport = BC_TRANSPORT_WIFI_DIRECT;
    }

    *found = n;
    return BC_OK;
}

static void udp_destroy(bc_transport_t *self)
{
    if (!self) {
        return;
    }

    udp_ctx_t *ctx = (udp_ctx_t *)self->ctx;
    if (ctx) {
        if (ctx->sockfd >= 0) {
            close(ctx->sockfd);
        }
        free(ctx->peer_ports);
        free(ctx);
    }
}

/* ---------- public constructor ---------- */

bc_transport_t *bc_transport_udp_create(uint16_t my_port,
                                        const uint16_t *peer_ports,
                                        size_t peer_count)
{
    udp_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->sockfd   = -1;
    ctx->my_port  = my_port;

    /* Copy peer port list. */
    if (peer_count > 0 && peer_ports) {
        ctx->peer_ports = malloc(peer_count * sizeof(uint16_t));
        if (!ctx->peer_ports) {
            free(ctx);
            return NULL;
        }
        memcpy(ctx->peer_ports, peer_ports, peer_count * sizeof(uint16_t));
        ctx->peer_count = peer_count;
    }

    /* Create UDP socket. */
    ctx->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ctx->sockfd < 0) {
        free(ctx->peer_ports);
        free(ctx);
        return NULL;
    }

    /* Allow port reuse for quick restart. */
    int opt = 1;
    setsockopt(ctx->sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind to 127.0.0.1:<my_port>. */
    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(my_port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(ctx->sockfd, (struct sockaddr *)&bind_addr,
             sizeof(bind_addr)) < 0) {
        close(ctx->sockfd);
        free(ctx->peer_ports);
        free(ctx);
        return NULL;
    }

    /* Build transport vtable. */
    bc_transport_t *t = calloc(1, sizeof(*t));
    if (!t) {
        close(ctx->sockfd);
        free(ctx->peer_ports);
        free(ctx);
        return NULL;
    }

    t->init           = udp_init;
    t->send           = udp_send;
    t->recv           = udp_recv;
    t->discover_peers = udp_discover_peers;
    t->destroy        = udp_destroy;
    t->ctx            = ctx;

    return t;
}
