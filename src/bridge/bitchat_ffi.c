/**
 * @file bitchat_ffi.c
 * @brief Implementation of the public FFI surface.
 *
 * This file wires together all internal modules (crypto, router, storage,
 * discovery) behind the simple flat C API defined in bitchat_ffi.h.
 *
 * Thread safety: every public function acquires g_state.mutex (recursive)
 * before touching shared state.
 */

/* Expose POSIX threading APIs (PTHREAD_MUTEX_RECURSIVE, etc.). */
#define _POSIX_C_SOURCE 200809L

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

#include <pthread.h>
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

    /* Received-message ring buffer for poll_messages (backward compat). */
    bc_message_t   *inbox[256];
    size_t          inbox_head;
    size_t          inbox_tail;

    /* Push-based message callback. */
    bitchat_on_message_fn user_callback;

    /* Thread safety. */
    pthread_mutex_t mutex;
    int             mutex_initialized;
} g_state;

/* ---------- mutex helpers ---------- */

static int ensure_mutex(void)
{
    if (g_state.mutex_initialized) {
        return BC_OK;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);

    int rc = pthread_mutex_init(&g_state.mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    if (rc != 0) {
        return BC_ERR;
    }

    g_state.mutex_initialized = 1;
    return BC_OK;
}

#define LOCK()   pthread_mutex_lock(&g_state.mutex)
#define UNLOCK() pthread_mutex_unlock(&g_state.mutex)

/* ---------- internal callback ---------- */

/**
 * Called by the mesh router when a message addressed to us arrives.
 * Runs under the mutex (called from bitchat_tick -> router_tick).
 */
static void on_message_received(const bc_message_t *msg, void *userdata)
{
    (void)userdata;

    /* 1. Copy the message into the inbox ring buffer (for poll_messages). */
    size_t next = (g_state.inbox_head + 1) % 256;
    if (next != g_state.inbox_tail) {
        bc_message_t *copy = bc_message_create(msg->sender_pubkey,
                                               msg->receiver_pubkey,
                                               msg->payload,
                                               msg->payload_len);
        if (copy) {
            memcpy(copy->uuid, msg->uuid, BC_UUID_SIZE);
            copy->ttl = msg->ttl;
            copy->timestamp = msg->timestamp;

            g_state.inbox[g_state.inbox_head] = copy;
            g_state.inbox_head = next;
        }
    }

    /* 2. If a push callback is registered, decrypt and deliver. */
    if (!g_state.user_callback) {
        return;
    }

    if (!msg->payload ||
        msg->payload_len < BC_NONCE_SIZE + BC_TAG_SIZE) {
        return;
    }

    /* Derive shared key from sender. */
    uint8_t shared_key[BC_SHARED_KEY_SIZE];
    int rc = bc_crypto_derive_shared(g_state.identity.secret_key,
                                     msg->sender_pubkey, shared_key);
    if (rc != BC_OK) {
        sodium_memzero(shared_key, sizeof(shared_key));
        return;
    }

    /* Decrypt: payload = nonce(24) || ciphertext+tag(N). */
    const uint8_t *nonce = msg->payload;
    const uint8_t *ct    = msg->payload + BC_NONCE_SIZE;
    size_t ct_len = msg->payload_len - BC_NONCE_SIZE;
    size_t pt_len = ct_len - BC_TAG_SIZE;

    uint8_t *plaintext = malloc(pt_len);
    if (!plaintext) {
        sodium_memzero(shared_key, sizeof(shared_key));
        return;
    }

    rc = bc_crypto_decrypt(ct, ct_len, shared_key, nonce, plaintext);
    sodium_memzero(shared_key, sizeof(shared_key));

    if (rc == BC_OK) {
        g_state.user_callback(msg->sender_pubkey, plaintext, pt_len);
    }

    sodium_memzero(plaintext, pt_len);
    free(plaintext);
}

/* ---------- public API ---------- */

int bitchat_init(const char *storage_path)
{
    int rc = ensure_mutex();
    if (rc != BC_OK) {
        return rc;
    }

    LOCK();

    if (g_state.initialized) {
        UNLOCK();
        return BC_OK;
    }

    rc = bc_crypto_init();
    if (rc != BC_OK) {
        UNLOCK();
        return rc;
    }

    /* Open storage. */
    g_state.storage = bc_storage_sqlite_create(storage_path);
    if (!g_state.storage) {
        UNLOCK();
        return BC_ERR_STORAGE;
    }

    /* Create transport (BLE as default, can be swapped). */
    g_state.transport = bc_transport_ble_create();

    g_state.initialized = 1;
    UNLOCK();
    return BC_OK;
}

int bitchat_generate_identity(uint8_t pubkey_out[32],
                               uint8_t secret_out[32])
{
    LOCK();

    if (!g_state.initialized) {
        UNLOCK();
        return BC_ERR;
    }

    int rc = bc_identity_generate(&g_state.identity);
    if (rc != BC_OK) {
        UNLOCK();
        return rc;
    }

    if (pubkey_out) {
        memcpy(pubkey_out, g_state.identity.pubkey, BC_PUBKEY_SIZE);
    }

    if (secret_out) {
        memcpy(secret_out, g_state.identity.secret_key, BC_SECRET_KEY_SIZE);
    }

    /* Now create the router with our identity. */
    if (g_state.router) {
        bc_router_destroy(g_state.router);
    }
    g_state.router = bc_router_create(g_state.transport, g_state.storage,
                                      g_state.identity.pubkey, NULL);
    if (!g_state.router) {
        UNLOCK();
        return BC_ERR_NOMEM;
    }
    bc_router_set_on_message(g_state.router, on_message_received, NULL);

    /* Create discovery. */
    if (g_state.discovery) {
        bc_discovery_destroy(g_state.discovery);
    }
    g_state.discovery = bc_discovery_create(g_state.transport);

    UNLOCK();
    return BC_OK;
}

int bitchat_load_identity(const uint8_t *identity_data, size_t len)
{
    LOCK();

    if (!g_state.initialized || !identity_data) {
        UNLOCK();
        return BC_ERR_INVALID;
    }

    int rc = bc_identity_deserialize(&g_state.identity, identity_data, len);
    if (rc != BC_OK) {
        UNLOCK();
        return rc;
    }

    /* Recreate router with loaded identity. */
    if (g_state.router) {
        bc_router_destroy(g_state.router);
    }
    g_state.router = bc_router_create(g_state.transport, g_state.storage,
                                      g_state.identity.pubkey, NULL);
    if (!g_state.router) {
        UNLOCK();
        return BC_ERR_NOMEM;
    }
    bc_router_set_on_message(g_state.router, on_message_received, NULL);

    if (g_state.discovery) {
        bc_discovery_destroy(g_state.discovery);
    }
    g_state.discovery = bc_discovery_create(g_state.transport);

    UNLOCK();
    return BC_OK;
}

int bitchat_send_message(const uint8_t recipient[32],
                         const uint8_t *plaintext, size_t len)
{
    LOCK();

    if (!g_state.initialized || !g_state.router || !recipient || !plaintext) {
        UNLOCK();
        return BC_ERR_INVALID;
    }

    /* Derive shared key. */
    uint8_t shared_key[BC_SHARED_KEY_SIZE];
    int rc = bc_crypto_derive_shared(g_state.identity.secret_key,
                                     recipient, shared_key);
    if (rc != BC_OK) {
        UNLOCK();
        return rc;
    }

    /* Encrypt. */
    size_t ct_len = len + BC_TAG_SIZE;
    uint8_t *ciphertext = malloc(BC_NONCE_SIZE + ct_len);
    if (!ciphertext) {
        sodium_memzero(shared_key, sizeof(shared_key));
        UNLOCK();
        return BC_ERR_NOMEM;
    }

    uint8_t *nonce = ciphertext;                /* first 24 bytes = nonce */
    uint8_t *ct    = ciphertext + BC_NONCE_SIZE; /* then ciphertext+tag   */

    rc = bc_crypto_encrypt(plaintext, len, shared_key, ct, nonce);
    sodium_memzero(shared_key, sizeof(shared_key));
    if (rc != BC_OK) {
        free(ciphertext);
        UNLOCK();
        return rc;
    }

    /* Build and broadcast message. */
    bc_message_t *msg = bc_message_create(g_state.identity.pubkey, recipient,
                                          ciphertext, BC_NONCE_SIZE + ct_len);
    free(ciphertext);
    if (!msg) {
        UNLOCK();
        return BC_ERR_NOMEM;
    }

    rc = bc_router_broadcast(g_state.router, msg);
    bc_message_destroy(msg);
    UNLOCK();
    return rc;
}

int bitchat_poll_messages(uint8_t *buf, size_t buf_len,
                          size_t *msg_len,
                          uint8_t sender_pubkey_out[32])
{
    LOCK();

    if (!g_state.initialized || !buf || !msg_len) {
        UNLOCK();
        return BC_ERR_INVALID;
    }

    if (g_state.inbox_tail == g_state.inbox_head) {
        *msg_len = 0;
        UNLOCK();
        return BC_OK;
    }

    bc_message_t *msg = g_state.inbox[g_state.inbox_tail];
    g_state.inbox_tail = (g_state.inbox_tail + 1) % 256;

    if (!msg || !msg->payload || msg->payload_len < BC_NONCE_SIZE + BC_TAG_SIZE) {
        bc_message_destroy(msg);
        *msg_len = 0;
        UNLOCK();
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
        UNLOCK();
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
        UNLOCK();
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
    UNLOCK();
    return rc;
}

int bitchat_start_discovery(void)
{
    LOCK();

    if (!g_state.initialized || !g_state.discovery) {
        UNLOCK();
        return BC_ERR;
    }

    int rc = bc_discovery_scan(g_state.discovery);
    UNLOCK();
    return rc;
}

void bitchat_tick(void)
{
    LOCK();

    if (!g_state.initialized || !g_state.router) {
        UNLOCK();
        return;
    }
    bc_router_tick(g_state.router);

    UNLOCK();
}

int bitchat_set_on_message_callback(bitchat_on_message_fn callback)
{
    LOCK();

    if (!g_state.initialized) {
        UNLOCK();
        return BC_ERR;
    }

    g_state.user_callback = callback;

    UNLOCK();
    return BC_OK;
}

void bitchat_shutdown(void)
{
    LOCK();

    if (!g_state.initialized) {
        UNLOCK();
        return;
    }

    /* Clear callback first to prevent firing during teardown. */
    g_state.user_callback = NULL;

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

    g_state.initialized = 0;
    g_state.transport = NULL;
    g_state.storage = NULL;
    g_state.router = NULL;
    g_state.discovery = NULL;
    g_state.inbox_head = 0;
    g_state.inbox_tail = 0;

    UNLOCK();

    /* Destroy mutex after unlocking. */
    pthread_mutex_destroy(&g_state.mutex);
    g_state.mutex_initialized = 0;
}
