/**
 * @file main.c
 * @brief BitChat CLI Simulator — end-to-end Gossip Protocol test harness.
 *
 * Assembles all components (Crypto, Storage, Router, UDP Transport) and
 * provides an interactive terminal interface for sending and receiving
 * encrypted mesh messages over localhost UDP.
 *
 * Usage:
 *   ./bitchat_cli <my_port> [peer_port1 peer_port2 ...]
 *
 * Example (3-node mesh):
 *   Terminal 1:  ./bitchat_cli 5001 5002 5003
 *   Terminal 2:  ./bitchat_cli 5002 5001 5003
 *   Terminal 3:  ./bitchat_cli 5003 5001 5002
 *
 * Commands:
 *   /info                          Print this node's public key (hex)
 *   /peers                         List known peers
 *   /msg <hex_pubkey> <message>    Send encrypted message to a peer
 *   /quit                          Shut down gracefully
 */

#define _DEFAULT_SOURCE   /* usleep, etc. */
#define _POSIX_C_SOURCE 200809L

#include "bitchat/infra/transport_udp_sim.h"
#include "bitchat/infra/storage_sqlite.h"
#include "bitchat/usecase/crypto_manager.h"
#include "bitchat/usecase/mesh_router.h"
#include "bitchat/usecase/node_discovery.h"
#include "bitchat/domain/identity.h"
#include "bitchat/domain/message.h"
#include "bitchat/domain/peer.h"
#include "bitchat/error.h"

#include <pthread.h>
#include <signal.h>
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- hex conversion utilities ---------- */

/**
 * Convert a byte array to a lowercase hex string.
 * @p out must be at least (len * 2 + 1) bytes.
 */
static void bytes_to_hex(const uint8_t *data, size_t len, char *out)
{
    static const char hex_chars[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2]     = hex_chars[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex_chars[data[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

/**
 * Convert a hex string to a byte array.
 * @p hex must contain exactly (out_len * 2) hex characters.
 * @return 0 on success, -1 on invalid input.
 */
static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    size_t hex_len = strlen(hex);
    if (hex_len != out_len * 2) {
        return -1;
    }

    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        char tmp[3] = { hex[i * 2], hex[i * 2 + 1], '\0' };

        /* Validate hex characters. */
        for (int j = 0; j < 2; j++) {
            char c = tmp[j];
            if (!((c >= '0' && c <= '9') ||
                  (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F'))) {
                return -1;
            }
        }

        if (sscanf(tmp, "%02x", &byte) != 1) {
            return -1;
        }
        out[i] = (uint8_t)byte;
    }
    return 0;
}

/* ---------- global state ---------- */

static volatile int g_running = 1;
static bc_identity_t    g_identity;
static bc_transport_t  *g_transport  = NULL;
static bc_storage_t    *g_storage    = NULL;
static bc_router_t     *g_router     = NULL;

/* ---------- message callback ---------- */

/**
 * Called by the mesh router when a message addressed to us arrives.
 * Decrypts and prints to stdout.
 */
static void on_message_received(const bc_message_t *msg, void *userdata)
{
    (void)userdata;

    if (!msg || !msg->payload ||
        msg->payload_len < BC_NONCE_SIZE + BC_TAG_SIZE) {
        return;
    }

    /* Derive shared key from sender. */
    uint8_t shared_key[BC_SHARED_KEY_SIZE];
    int rc = bc_crypto_derive_shared(g_identity.secret_key,
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

    uint8_t *plaintext = malloc(pt_len + 1);
    if (!plaintext) {
        sodium_memzero(shared_key, sizeof(shared_key));
        return;
    }

    rc = bc_crypto_decrypt(ct, ct_len, shared_key, nonce, plaintext);
    sodium_memzero(shared_key, sizeof(shared_key));

    if (rc == BC_OK) {
        plaintext[pt_len] = '\0'; /* null-terminate for printf */

        char sender_hex[BC_PUBKEY_SIZE * 2 + 1];
        bytes_to_hex(msg->sender_pubkey, BC_PUBKEY_SIZE, sender_hex);

        printf("\n[PESAN MASUK dari %s]:\n  %s\n> ",
               sender_hex, (char *)plaintext);
        fflush(stdout);
    } else {
        fprintf(stderr, "[WARN] Decryption failed (rc=%d)\n", rc);
    }

    sodium_memzero(plaintext, pt_len);
    free(plaintext);
}

/* ---------- background tick thread ---------- */

static void *tick_thread_func(void *arg)
{
    (void)arg;

    while (g_running) {
        bc_router_tick(g_router);
        usleep(100000); /* 100ms */
    }

    return NULL;
}

/* ---------- signal handler ---------- */

static void signal_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ---------- CLI command handlers ---------- */

static void cmd_info(void)
{
    char hex[BC_PUBKEY_SIZE * 2 + 1];
    bytes_to_hex(g_identity.pubkey, BC_PUBKEY_SIZE, hex);
    printf("Public Key: %s\n", hex);
}

static void cmd_peers(void)
{
    if (g_router->peer_count == 0) {
        printf("(no peers)\n");
        return;
    }

    for (size_t i = 0; i < g_router->peer_count; i++) {
        char hex[BC_PUBKEY_SIZE * 2 + 1];
        bytes_to_hex(g_router->peers[i].pubkey, BC_PUBKEY_SIZE, hex);
        printf("  [%zu] %s\n", i, hex);
    }
}

static void cmd_msg(const char *args)
{
    if (!args || strlen(args) < BC_PUBKEY_SIZE * 2 + 2) {
        printf("Usage: /msg <64-char hex pubkey> <message text>\n");
        return;
    }

    /* Parse hex pubkey (first 64 hex chars). */
    char hex_str[BC_PUBKEY_SIZE * 2 + 1];
    memcpy(hex_str, args, BC_PUBKEY_SIZE * 2);
    hex_str[BC_PUBKEY_SIZE * 2] = '\0';

    uint8_t recipient[BC_PUBKEY_SIZE];
    if (hex_to_bytes(hex_str, recipient, BC_PUBKEY_SIZE) != 0) {
        printf("Error: invalid hex public key\n");
        return;
    }

    /* Skip the space after the pubkey. */
    const char *text = args + BC_PUBKEY_SIZE * 2;
    while (*text == ' ') text++;

    if (*text == '\0') {
        printf("Error: message text is empty\n");
        return;
    }

    size_t text_len = strlen(text);

    /* Derive shared key. */
    uint8_t shared_key[BC_SHARED_KEY_SIZE];
    int rc = bc_crypto_derive_shared(g_identity.secret_key,
                                     recipient, shared_key);
    if (rc != BC_OK) {
        printf("Error: failed to derive shared key (rc=%d)\n", rc);
        return;
    }

    /* Encrypt. */
    size_t ct_len = text_len + BC_TAG_SIZE;
    uint8_t *ciphertext = malloc(BC_NONCE_SIZE + ct_len);
    if (!ciphertext) {
        sodium_memzero(shared_key, sizeof(shared_key));
        printf("Error: out of memory\n");
        return;
    }

    uint8_t *nonce = ciphertext;
    uint8_t *ct    = ciphertext + BC_NONCE_SIZE;

    rc = bc_crypto_encrypt((const uint8_t *)text, text_len,
                           shared_key, ct, nonce);
    sodium_memzero(shared_key, sizeof(shared_key));

    if (rc != BC_OK) {
        free(ciphertext);
        printf("Error: encryption failed (rc=%d)\n", rc);
        return;
    }

    /* Build and broadcast message. */
    bc_message_t *msg = bc_message_create(g_identity.pubkey, recipient,
                                          ciphertext, BC_NONCE_SIZE + ct_len);
    free(ciphertext);

    if (!msg) {
        printf("Error: failed to create message\n");
        return;
    }

    rc = bc_router_broadcast(g_router, msg);
    bc_message_destroy(msg);

    if (rc == BC_OK) {
        printf("Message sent.\n");
    } else {
        printf("Error: broadcast failed (rc=%d)\n", rc);
    }
}

/* ---------- main ---------- */

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <my_port> [peer_port1 peer_port2 ...]\n",
                argv[0]);
        return 1;
    }

    uint16_t my_port = (uint16_t)atoi(argv[1]);
    if (my_port == 0) {
        fprintf(stderr, "Error: invalid port '%s'\n", argv[1]);
        return 1;
    }

    /* Collect peer ports from remaining arguments. */
    size_t peer_count = (size_t)(argc - 2);
    uint16_t *peer_ports = NULL;

    if (peer_count > 0) {
        peer_ports = malloc(peer_count * sizeof(uint16_t));
        if (!peer_ports) {
            fprintf(stderr, "Error: out of memory\n");
            return 1;
        }
        for (size_t i = 0; i < peer_count; i++) {
            peer_ports[i] = (uint16_t)atoi(argv[2 + (int)i]);
        }
    }

    /* 1. Initialize crypto subsystem. */
    if (bc_crypto_init() != BC_OK) {
        fprintf(stderr, "Error: failed to initialize libsodium\n");
        free(peer_ports);
        return 1;
    }

    /* 2. Load or generate identity.
     *
     * The secret key is persisted in node_{port}.key so the node keeps
     * the same public key across restarts.  This is critical for
     * store-and-forward: messages stored for our pubkey must still be
     * decryptable after a restart. */
    char key_path[64];
    snprintf(key_path, sizeof(key_path), "node_%u.key", my_port);

    FILE *kf = fopen(key_path, "rb");
    if (kf) {
        /* Key file exists — load secret key and derive pubkey. */
        size_t n = fread(g_identity.secret_key, 1, BC_SECRET_KEY_SIZE, kf);
        fclose(kf);

        if (n != BC_SECRET_KEY_SIZE) {
            fprintf(stderr, "Error: key file '%s' is corrupted (%zu bytes)\n",
                    key_path, n);
            free(peer_ports);
            return 1;
        }

        /* Derive X25519 public key from the stored secret key. */
        if (crypto_scalarmult_base(g_identity.pubkey,
                                   g_identity.secret_key) != 0) {
            fprintf(stderr, "Error: failed to derive public key\n");
            sodium_memzero(g_identity.secret_key, BC_SECRET_KEY_SIZE);
            free(peer_ports);
            return 1;
        }

        fprintf(stderr, "Identity loaded from %s\n", key_path);
    } else {
        /* No key file — generate a fresh keypair and save it. */
        if (bc_identity_generate(&g_identity) != BC_OK) {
            fprintf(stderr, "Error: failed to generate identity\n");
            free(peer_ports);
            return 1;
        }

        kf = fopen(key_path, "wb");
        if (!kf) {
            fprintf(stderr, "Error: cannot write key file '%s'\n", key_path);
            free(peer_ports);
            return 1;
        }
        fwrite(g_identity.secret_key, 1, BC_SECRET_KEY_SIZE, kf);
        fclose(kf);

        fprintf(stderr, "New identity saved to %s\n", key_path);
    }

    char pubkey_hex[BC_PUBKEY_SIZE * 2 + 1];
    bytes_to_hex(g_identity.pubkey, BC_PUBKEY_SIZE, pubkey_hex);

    /* 3. Open SQLite storage (per-port database). */
    char db_path[64];
    snprintf(db_path, sizeof(db_path), "node_%u.db", my_port);

    g_storage = bc_storage_sqlite_create(db_path);
    if (!g_storage) {
        fprintf(stderr, "Error: failed to open database '%s'\n", db_path);
        free(peer_ports);
        return 1;
    }

    /* 4. Create UDP transport. */
    g_transport = bc_transport_udp_create(my_port, peer_ports, peer_count);
    free(peer_ports);

    if (!g_transport) {
        fprintf(stderr, "Error: failed to bind UDP port %u\n", my_port);
        g_storage->destroy(g_storage);
        return 1;
    }

    /* 5. Create mesh router. */
    g_router = bc_router_create(g_transport, g_storage,
                                g_identity.pubkey, NULL);
    if (!g_router) {
        fprintf(stderr, "Error: failed to create router\n");
        g_transport->destroy(g_transport);
        free(g_transport);
        g_storage->destroy(g_storage);
        return 1;
    }

    bc_router_set_on_message(g_router, on_message_received, NULL);

    /* 6. Discover peers (adds synthetic entries to the router). */
    bc_peer_t discovered[BC_MAX_PEERS];
    size_t found = 0;
    if (g_transport->discover_peers(g_transport, discovered,
                                    BC_MAX_PEERS, &found) == BC_OK) {
        for (size_t i = 0; i < found; i++) {
            bc_router_add_peer(g_router, &discovered[i]);
        }
    }

    /* 7. Install signal handler for graceful shutdown. */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* 8. Start background tick thread. */
    pthread_t tick_tid;
    if (pthread_create(&tick_tid, NULL, tick_thread_func, NULL) != 0) {
        fprintf(stderr, "Error: failed to create tick thread\n");
        bc_router_destroy(g_router);
        g_transport->destroy(g_transport);
        free(g_transport);
        g_storage->destroy(g_storage);
        return 1;
    }

    /* 9. Print banner. */
    printf("=== BitChat CLI Simulator ===\n");
    printf("Port      : %u\n", my_port);
    printf("Public Key: %s\n", pubkey_hex);
    printf("Key File  : %s\n", key_path);
    printf("Database  : %s\n", db_path);
    printf("Peers     : %zu\n", found);
    printf("\nCommands:\n");
    printf("  /info                      Show public key\n");
    printf("  /peers                     List connected peers\n");
    printf("  /msg <hex_pubkey> <text>   Send encrypted message\n");
    printf("  /quit                      Exit\n");
    printf("\n");

    /* 10. Main input loop. */
    char line[4096];

    while (g_running) {
        printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            /* EOF on stdin — run as headless relay until signal.
             * This happens when launched with '< /dev/null' or as a
             * background process.  The tick thread keeps handling UDP
             * receive + store-and-forward rebroadcast. */
            while (g_running) {
                sleep(1);
            }
            break;
        }

        /* Strip trailing newline. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
            len--;
        }

        if (len == 0) {
            continue;
        }

        if (strcmp(line, "/info") == 0) {
            cmd_info();
        } else if (strcmp(line, "/peers") == 0) {
            cmd_peers();
        } else if (strncmp(line, "/msg ", 5) == 0) {
            cmd_msg(line + 5);
        } else if (strcmp(line, "/quit") == 0) {
            g_running = 0;
        } else {
            printf("Unknown command. Type /info, /peers, /msg, or /quit.\n");
        }
    }

    /* 11. Shutdown. */
    printf("\nShutting down...\n");

    g_running = 0;
    pthread_join(tick_tid, NULL);

    bc_router_destroy(g_router);
    g_transport->destroy(g_transport);
    free(g_transport);
    g_storage->destroy(g_storage);
    bc_identity_wipe(&g_identity);

    printf("Goodbye.\n");
    return 0;
}
