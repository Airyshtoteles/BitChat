/**
 * @file bitchat_ffi.c
 * @brief Implementation of the public FFI surface.
 *
 * This file wires together all internal modules (crypto, router, storage,
 * discovery) behind the simple flat C API defined in bitchat_ffi.h.
 */

#include "bitchat/bridge/bitchat_ffi.h"
#include "bitchat/domain/identity.h"
#include "bitchat/domain/message.h"
#include "bitchat/usecase/crypto_manager.h"
#include "bitchat/usecase/mesh_router.h"
#include "bitchat/usecase/node_discovery.h"
#include "bitchat/usecase/message_store.h"
#include "bitchat/port/storage.h"
#include "bitchat/port/transport.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations for transport constructors (from infra). */
extern bc_transport_t *bc_transport_ble_create(void);
extern bc_transport_t *bc_transport_wifi_create(void);
extern bc_storage_t   *bc_storage_sqlite_create(const char *db_path);

/* ---------- global singleton state ---------- */

static struct {
    int             initialized;
    bc_identity_t   identity;
    bc_transport_t *transport;
    bc_storage_t   *storage;
    bc_router_t    *router;
    bc_discovery_t *discovery;

    /* Received-message ring buffer for poll_messages. */
    bc_message_t   *inbox[256];
    size_t          inbox_head;
    size_t          inbox_tail;
} g_state;

/* ---------- internal callback ---------- */

static void on_message_received(const bc_message_t *msg, void *userdata)
{
    (void)userdata;

    /* Copy the message into the inbox ring buffer. */
    size_t next = (g_state.inbox_head + 1) % 256;
    if (next == g_state.inbox_tail) {
        return; /* inbox full, drop oldest by overwriting */
    }

    bc_message_t *copy = bc_message_create(msg->sender_pubkey,
                                           msg->receiver_pubkey,
                                           msg->payload,
                                           msg->payload_len);
    if (!copy) {
        return;
    }
    memcpy(copy->uuid, msg->uuid, BC_UUID_SIZE);
    copy->ttl = msg->ttl;
    copy->timestamp = msg->timestamp;

    g_state.inbox[g_state.inbox_head] = copy;
    g_state.inbox_head = next;
}

/* ---------- public API ---------- */

int bitchat_init(const char *storage_path)
{
    if (g_state.initialized) {
        return BC_OK;
    }

    int rc = bc_crypto_init();
    if (rc != BC_OK) {
        return rc;
    }

    /* Open storage. */
    g_state.storage = bc_storage_sqlite_create(storage_path);
    if (!g_state.storage) {
        return BC_ERR_STORAGE;
    }

    /* Create transport (BLE as default, can be swapped). */
    g_state.transport = bc_transport_ble_create();

    g_state.initialized = 1;
    return BC_OK;
}

int bitchat_generate_identity(uint8_t pubkey_out[32])
{
    if (!g_state.initialized) {
        return BC_ERR;
    }

    int rc = bc_identity_generate(&g_state.identity);
    if (rc != BC_OK) {
        return rc;
    }

    if (pubkey_out) {
        memcpy(pubkey_out, g_state.identity.pubkey, BC_PUBKEY_SIZE);
    }

    /* Now create the router with our identity. */
    if (g_state.router) {
        bc_router_destroy(g_state.router);
    }
    g_state.router = bc_router_create(g_state.transport, g_state.storage,
                                      g_state.identity.pubkey, NULL);
    if (!g_state.router) {
        return BC_ERR_NOMEM;
    }
    bc_router_set_on_message(g_state.router, on_message_received, NULL);

    /* Create discovery. */
    if (g_state.discovery) {
        bc_discovery_destroy(g_state.discovery);
    }
    g_state.discovery = bc_discovery_create(g_state.transport);

    return BC_OK;
}

int bitchat_load_identity(const uint8_t *identity_data, size_t len)
{
    if (!g_state.initialized || !identity_data) {
        return BC_ERR_INVALID;
    }

    int rc = bc_identity_deserialize(&g_state.identity, identity_data, len);
    if (rc != BC_OK) {
        return rc;
    }

    /* Recreate router with loaded identity. */
    if (g_state.router) {
        bc_router_destroy(g_state.router);
    }
    g_state.router = bc_router_create(g_state.transport, g_state.storage,
                                      g_state.identity.pubkey, NULL);
    if (!g_state.router) {
        return BC_ERR_NOMEM;
    }
    bc_router_set_on_message(g_state.router, on_message_received, NULL);

    if (g_state.discovery) {
        bc_discovery_destroy(g_state.discovery);
    }
    g_state.discovery = bc_discovery_create(g_state.transport);

    return BC_OK;
}

int bitchat_send_message(const uint8_t recipient[32],
                         const uint8_t *plaintext, size_t len)
{
    if (!g_state.initialized || !g_state.router || !recipient || !plaintext) {
        return BC_ERR_INVALID;
    }

    /* Derive shared key. */
    uint8_t shared_key[BC_SHARED_KEY_SIZE];
    int rc = bc_crypto_derive_shared(g_state.identity.secret_key,
                                     recipient, shared_key);
    if (rc != BC_OK) {
        return rc;
    }

    /* Encrypt. */
    size_t ct_len = len + BC_TAG_SIZE;
    uint8_t *ciphertext = malloc(BC_NONCE_SIZE + ct_len);
    if (!ciphertext) {
        sodium_memzero(shared_key, sizeof(shared_key));
        return BC_ERR_NOMEM;
    }

    uint8_t *nonce = ciphertext;                /* first 24 bytes = nonce */
    uint8_t *ct    = ciphertext + BC_NONCE_SIZE; /* then ciphertext+tag   */

    rc = bc_crypto_encrypt(plaintext, len, shared_key, ct, nonce);
    sodium_memzero(shared_key, sizeof(shared_key));
    if (rc != BC_OK) {
        free(ciphertext);
        return rc;
    }

    /* Build and broadcast message. */
    bc_message_t *msg = bc_message_create(g_state.identity.pubkey, recipient,
                                          ciphertext, BC_NONCE_SIZE + ct_len);
    free(ciphertext);
    if (!msg) {
        return BC_ERR_NOMEM;
    }

    rc = bc_router_broadcast(g_state.router, msg);
    bc_message_destroy(msg);
    return rc;
}

int bitchat_poll_messages(uint8_t *buf, size_t buf_len,
                          size_t *msg_len,
                          uint8_t sender_pubkey_out[32])
{
    if (!g_state.initialized || !buf || !msg_len) {
        return BC_ERR_INVALID;
    }

    if (g_state.inbox_tail == g_state.inbox_head) {
        *msg_len = 0;
        return BC_OK;
    }

    bc_message_t *msg = g_state.inbox[g_state.inbox_tail];
    g_state.inbox_tail = (g_state.inbox_tail + 1) % 256;

    if (!msg || !msg->payload || msg->payload_len < BC_NONCE_SIZE + BC_TAG_SIZE) {
        bc_message_destroy(msg);
        *msg_len = 0;
        return BC_OK;
    }

    /* Derive shared key from sender. */
    uint8_t shared_key[BC_SHARED_KEY_SIZE];
    int rc = bc_crypto_derive_shared(g_state.identity.secret_key,
                                     msg->sender_pubkey, shared_key);
    if (rc != BC_OK) {
        sodium_memzero(shared_key, sizeof(shared_key));
        bc_message_destroy(msg);
        *msg_len = 0;
        return rc;
    }

    /* Decrypt: payload = nonce(24) || ciphertext+tag(N) */
    const uint8_t *nonce = msg->payload;
    const uint8_t *ct    = msg->payload + BC_NONCE_SIZE;
    size_t ct_len = msg->payload_len - BC_NONCE_SIZE;
    size_t pt_len = ct_len - BC_TAG_SIZE;

    if (buf_len < pt_len) {
        sodium_memzero(shared_key, sizeof(shared_key));
        bc_message_destroy(msg);
        *msg_len = pt_len;
        return BC_ERR_BUFFER_TOO_SMALL;
    }

    rc = bc_crypto_decrypt(ct, ct_len, shared_key, nonce, buf);
    sodium_memzero(shared_key, sizeof(shared_key));

    if (rc == BC_OK) {
        *msg_len = pt_len;
        if (sender_pubkey_out) {
            memcpy(sender_pubkey_out, msg->sender_pubkey, BC_PUBKEY_SIZE);
        }
    } else {
        *msg_len = 0;
    }

    bc_message_destroy(msg);
    return rc;
}

int bitchat_start_discovery(void)
{
    if (!g_state.initialized || !g_state.discovery) {
        return BC_ERR;
    }
    return bc_discovery_scan(g_state.discovery);
}

void bitchat_tick(void)
{
    if (!g_state.initialized || !g_state.router) {
        return;
    }
    bc_router_tick(g_state.router);
}

void bitchat_shutdown(void)
{
    if (!g_state.initialized) {
        return;
    }

    /* Drain inbox. */
    while (g_state.inbox_tail != g_state.inbox_head) {
        bc_message_destroy(g_state.inbox[g_state.inbox_tail]);
        g_state.inbox_tail = (g_state.inbox_tail + 1) % 256;
    }

    if (g_state.discovery) {
        bc_discovery_destroy(g_state.discovery);
    }
    if (g_state.router) {
        bc_router_destroy(g_state.router);
    }
    if (g_state.storage) {
        g_state.storage->destroy(g_state.storage);
    }
    if (g_state.transport) {
        g_state.transport->destroy(g_state.transport);
        free(g_state.transport);
    }

    bc_identity_wipe(&g_state.identity);
    memset(&g_state, 0, sizeof(g_state));
}
