/**
 * @file message.c
 * @brief Implementation of the message entity.
 */

#include "bitchat/domain/message.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---------- helpers for big-endian wire encoding ---------- */

static void write_u32_be(uint8_t *dst, uint32_t val)
{
    dst[0] = (uint8_t)(val >> 24);
    dst[1] = (uint8_t)(val >> 16);
    dst[2] = (uint8_t)(val >> 8);
    dst[3] = (uint8_t)(val);
}

static uint32_t read_u32_be(const uint8_t *src)
{
    return ((uint32_t)src[0] << 24) |
           ((uint32_t)src[1] << 16) |
           ((uint32_t)src[2] << 8)  |
           ((uint32_t)src[3]);
}

static void write_u64_be(uint8_t *dst, uint64_t val)
{
    dst[0] = (uint8_t)(val >> 56);
    dst[1] = (uint8_t)(val >> 48);
    dst[2] = (uint8_t)(val >> 40);
    dst[3] = (uint8_t)(val >> 32);
    dst[4] = (uint8_t)(val >> 24);
    dst[5] = (uint8_t)(val >> 16);
    dst[6] = (uint8_t)(val >> 8);
    dst[7] = (uint8_t)(val);
}

static uint64_t read_u64_be(const uint8_t *src)
{
    return ((uint64_t)src[0] << 56) |
           ((uint64_t)src[1] << 48) |
           ((uint64_t)src[2] << 40) |
           ((uint64_t)src[3] << 32) |
           ((uint64_t)src[4] << 24) |
           ((uint64_t)src[5] << 16) |
           ((uint64_t)src[6] << 8)  |
           ((uint64_t)src[7]);
}

/* ---------- public API ---------- */

bc_message_t *bc_message_create(const uint8_t sender[BC_PUBKEY_SIZE],
                                const uint8_t receiver[BC_PUBKEY_SIZE],
                                const uint8_t *payload,
                                size_t payload_len)
{
    if (!sender || !receiver) {
        return NULL;
    }

    bc_message_t *msg = calloc(1, sizeof(*msg));
    if (!msg) {
        return NULL;
    }

    /* Generate a random 128-bit UUID. */
    randombytes_buf(msg->uuid, BC_UUID_SIZE);

    msg->ttl = BC_DEFAULT_TTL;
    memcpy(msg->sender_pubkey, sender, BC_PUBKEY_SIZE);
    memcpy(msg->receiver_pubkey, receiver, BC_PUBKEY_SIZE);
    msg->timestamp = (uint64_t)time(NULL);

    if (payload && payload_len > 0) {
        msg->payload = malloc(payload_len);
        if (!msg->payload) {
            free(msg);
            return NULL;
        }
        memcpy(msg->payload, payload, payload_len);
        msg->payload_len = payload_len;
    }

    return msg;
}

void bc_message_destroy(bc_message_t *msg)
{
    if (!msg) {
        return;
    }
    if (msg->payload) {
        sodium_memzero(msg->payload, msg->payload_len);
        free(msg->payload);
    }
    sodium_memzero(msg, sizeof(*msg));
    free(msg);
}

int bc_message_serialize(const bc_message_t *msg,
                         uint8_t *buf, size_t buf_len,
                         size_t *out_len)
{
    if (!msg || !buf || !out_len) {
        return BC_ERR_INVALID;
    }

    size_t needed = BC_MESSAGE_HEADER_SIZE + msg->payload_len;
    if (buf_len < needed) {
        *out_len = needed;
        return BC_ERR_BUFFER_TOO_SMALL;
    }

    uint8_t *p = buf;

    memcpy(p, msg->uuid, BC_UUID_SIZE);
    p += BC_UUID_SIZE;

    *p++ = msg->ttl;

    memcpy(p, msg->sender_pubkey, BC_PUBKEY_SIZE);
    p += BC_PUBKEY_SIZE;

    memcpy(p, msg->receiver_pubkey, BC_PUBKEY_SIZE);
    p += BC_PUBKEY_SIZE;

    write_u64_be(p, msg->timestamp);
    p += 8;

    write_u32_be(p, (uint32_t)msg->payload_len);
    p += 4;

    if (msg->payload_len > 0) {
        memcpy(p, msg->payload, msg->payload_len);
        p += msg->payload_len;
    }

    *out_len = (size_t)(p - buf);
    return BC_OK;
}

bc_message_t *bc_message_deserialize(const uint8_t *data, size_t data_len)
{
    if (!data || data_len < BC_MESSAGE_HEADER_SIZE) {
        return NULL;
    }

    const uint8_t *p = data;

    const uint8_t *uuid = p;
    p += BC_UUID_SIZE;

    uint8_t ttl = *p++;

    const uint8_t *sender = p;
    p += BC_PUBKEY_SIZE;

    const uint8_t *receiver = p;
    p += BC_PUBKEY_SIZE;

    uint64_t timestamp = read_u64_be(p);
    p += 8;

    uint32_t payload_len = read_u32_be(p);
    p += 4;

    if (data_len < BC_MESSAGE_HEADER_SIZE + payload_len) {
        return NULL;
    }

    bc_message_t *msg = calloc(1, sizeof(*msg));
    if (!msg) {
        return NULL;
    }

    memcpy(msg->uuid, uuid, BC_UUID_SIZE);
    msg->ttl = ttl;
    memcpy(msg->sender_pubkey, sender, BC_PUBKEY_SIZE);
    memcpy(msg->receiver_pubkey, receiver, BC_PUBKEY_SIZE);
    msg->timestamp = timestamp;

    if (payload_len > 0) {
        msg->payload = malloc(payload_len);
        if (!msg->payload) {
            free(msg);
            return NULL;
        }
        memcpy(msg->payload, p, payload_len);
        msg->payload_len = payload_len;
    }

    return msg;
}
