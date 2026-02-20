/**
 * @file message.h
 * @brief Core message entity for the BitChat mesh network.
 *
 * Each message carries a unique 128-bit UUID for deduplication, a TTL counter
 * that decrements on each hop, and an encrypted payload addressed by public
 * key rather than any centralized identifier.
 */

#ifndef BITCHAT_DOMAIN_MESSAGE_H
#define BITCHAT_DOMAIN_MESSAGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Size of a public key in bytes (X25519). */
#define BC_PUBKEY_SIZE  32

/** Size of a message UUID in bytes. */
#define BC_UUID_SIZE    16

/** Default hop limit for new messages. */
#define BC_DEFAULT_TTL  10

/**
 * @brief A message transiting the BitChat mesh.
 *
 * Messages are the fundamental unit of data exchange. They are created by the
 * sender, encrypted for the recipient, and propagated through the mesh via the
 * Gossip protocol until delivered or until their TTL reaches zero.
 */
typedef struct bc_message {
    uint8_t  uuid[BC_UUID_SIZE];            /**< Unique ID for deduplication.   */
    uint8_t  ttl;                           /**< Remaining hop count.           */
    uint8_t  sender_pubkey[BC_PUBKEY_SIZE]; /**< Sender's public key.           */
    uint8_t  receiver_pubkey[BC_PUBKEY_SIZE]; /**< Recipient's public key.      */
    uint64_t timestamp;                     /**< Creation time (UNIX seconds).  */
    uint8_t *payload;                       /**< Encrypted ciphertext (heap).   */
    size_t   payload_len;                   /**< Length of payload in bytes.     */
} bc_message_t;

/**
 * @brief Allocate and initialize a new message with a random UUID.
 *
 * The caller owns the returned pointer and must free it with
 * @ref bc_message_destroy.
 *
 * @param sender    Sender public key (BC_PUBKEY_SIZE bytes).
 * @param receiver  Recipient public key (BC_PUBKEY_SIZE bytes).
 * @param payload   Encrypted payload bytes (copied internally).
 * @param payload_len Length of @p payload.
 * @return Pointer to the new message, or NULL on allocation failure.
 */
bc_message_t *bc_message_create(const uint8_t sender[BC_PUBKEY_SIZE],
                                const uint8_t receiver[BC_PUBKEY_SIZE],
                                const uint8_t *payload,
                                size_t payload_len);

/**
 * @brief Free all resources held by a message.
 * @param msg Message to destroy (NULL-safe).
 */
void bc_message_destroy(bc_message_t *msg);

/**
 * @brief Serialize a message into a contiguous byte buffer.
 *
 * Wire format (big-endian):
 *   [uuid:16][ttl:1][sender:32][receiver:32][timestamp:8][payload_len:4][payload:N]
 *
 * @param msg      Message to serialize.
 * @param buf      Output buffer.
 * @param buf_len  Size of @p buf.
 * @param out_len  On success, number of bytes written.
 * @return BC_OK on success, BC_ERR_BUFFER_TOO_SMALL if buf is too small.
 */
int bc_message_serialize(const bc_message_t *msg,
                         uint8_t *buf, size_t buf_len,
                         size_t *out_len);

/**
 * @brief Deserialize a message from a byte buffer.
 *
 * The caller owns the returned pointer and must free it with
 * @ref bc_message_destroy.
 *
 * @param data    Input buffer.
 * @param data_len Length of @p data.
 * @return Pointer to the deserialized message, or NULL on error.
 */
bc_message_t *bc_message_deserialize(const uint8_t *data, size_t data_len);

/**
 * @brief Minimum serialized size of a message (with zero-length payload).
 */
#define BC_MESSAGE_HEADER_SIZE (BC_UUID_SIZE + 1 + BC_PUBKEY_SIZE * 2 + 8 + 4)

#ifdef __cplusplus
}
#endif

#endif /* BITCHAT_DOMAIN_MESSAGE_H */
