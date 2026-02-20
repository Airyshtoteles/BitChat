/**
 * @file bitchat_ffi.h
 * @brief Public C API for BitChat — the FFI surface for mobile platforms.
 *
 * This header exposes a minimal, FFI-safe interface that can be called from
 * any language with C FFI support: Swift, Kotlin/JNI, Dart, React Native, etc.
 *
 * Design constraints:
 * - All functions use only primitive types and byte arrays.
 * - No struct-by-value in parameters or returns.
 * - No function pointers in return types.
 * - Thread-safe: all state is internal, guarded where necessary.
 *
 * Typical lifecycle:
 *   bitchat_init() -> bitchat_generate_identity() -> bitchat_start_discovery()
 *   -> bitchat_send_message() / bitchat_poll_messages() -> bitchat_shutdown()
 */

#ifndef BITCHAT_BRIDGE_FFI_H
#define BITCHAT_BRIDGE_FFI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ensure symbols are exported in shared library builds. */
#ifdef _WIN32
#  define BC_EXPORT __declspec(dllexport)
#else
#  define BC_EXPORT __attribute__((visibility("default")))
#endif

/**
 * @brief Initialize the BitChat core.
 *
 * Sets up the crypto subsystem, opens the message database, and prepares
 * the mesh router. Must be called before any other function.
 *
 * @param storage_path  Path to the SQLite database file.
 * @return 0 on success, negative error code on failure.
 */
BC_EXPORT int bitchat_init(const char *storage_path);

/**
 * @brief Generate a new identity (X25519 keypair).
 *
 * The public key is written to @p pubkey_out. The secret key is stored
 * internally and never exposed.
 *
 * @param pubkey_out  Output buffer for the public key (32 bytes).
 * @return 0 on success, negative error code on failure.
 */
BC_EXPORT int bitchat_generate_identity(uint8_t pubkey_out[32]);

/**
 * @brief Load an existing identity from a serialized buffer.
 *
 * @param identity_data  Serialized identity (64 bytes: pubkey + secret).
 * @param len            Length of @p identity_data.
 * @return 0 on success, negative error code on failure.
 */
BC_EXPORT int bitchat_load_identity(const uint8_t *identity_data, size_t len);

/**
 * @brief Send an encrypted message to a recipient.
 *
 * The plaintext is encrypted using the shared key derived from our secret
 * key and the recipient's public key. The encrypted message is then
 * broadcast into the mesh.
 *
 * @param recipient   Recipient's public key (32 bytes).
 * @param plaintext   Message content.
 * @param len         Length of @p plaintext.
 * @return 0 on success, negative error code on failure.
 */
BC_EXPORT int bitchat_send_message(const uint8_t recipient[32],
                                   const uint8_t *plaintext, size_t len);

/**
 * @brief Poll for incoming messages.
 *
 * Non-blocking. If a message is available, it is decrypted and written
 * to @p buf. If no message is available, @p msg_len is set to 0.
 *
 * @param buf       Output buffer for the decrypted plaintext.
 * @param buf_len   Size of @p buf.
 * @param msg_len   Actual message length (0 if none available).
 * @param sender_pubkey_out  Sender's public key (32 bytes, may be NULL).
 * @return 0 on success, negative error code on failure.
 */
BC_EXPORT int bitchat_poll_messages(uint8_t *buf, size_t buf_len,
                                    size_t *msg_len,
                                    uint8_t sender_pubkey_out[32]);

/**
 * @brief Start peer discovery.
 *
 * Triggers a scan on all available transports (BLE, Wi-Fi Direct).
 *
 * @return 0 on success, negative error code on failure.
 */
BC_EXPORT int bitchat_start_discovery(void);

/**
 * @brief Run one maintenance cycle.
 *
 * Should be called periodically (e.g. every 5 seconds) by the UI layer.
 * Handles: incoming data polling, store-and-forward retries, dedup cleanup.
 */
BC_EXPORT void bitchat_tick(void);

/**
 * @brief Shut down the BitChat core.
 *
 * Closes the database, frees all memory, and wipes sensitive key material.
 */
BC_EXPORT void bitchat_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_BRIDGE_FFI_H */
