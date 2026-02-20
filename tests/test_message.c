/**
 * @file test_message.c
 * @brief Unit tests for the message entity (create, serialize, deserialize).
 */

#include "unity.h"
#include "bitchat/domain/message.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_message_create_and_destroy(void)
{
    uint8_t sender[BC_PUBKEY_SIZE];
    uint8_t receiver[BC_PUBKEY_SIZE];
    randombytes_buf(sender, sizeof(sender));
    randombytes_buf(receiver, sizeof(receiver));

    const uint8_t payload[] = "Hello, BitChat!";
    bc_message_t *msg = bc_message_create(sender, receiver,
                                          payload, sizeof(payload));

    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_UINT8(BC_DEFAULT_TTL, msg->ttl);
    TEST_ASSERT_EQUAL_MEMORY(sender, msg->sender_pubkey, BC_PUBKEY_SIZE);
    TEST_ASSERT_EQUAL_MEMORY(receiver, msg->receiver_pubkey, BC_PUBKEY_SIZE);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), msg->payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, msg->payload, sizeof(payload));
    TEST_ASSERT_TRUE(msg->timestamp > 0);

    bc_message_destroy(msg);
}

void test_message_uuid_uniqueness(void)
{
    uint8_t sender[BC_PUBKEY_SIZE] = {0};
    uint8_t receiver[BC_PUBKEY_SIZE] = {0};

    bc_message_t *m1 = bc_message_create(sender, receiver, NULL, 0);
    bc_message_t *m2 = bc_message_create(sender, receiver, NULL, 0);

    TEST_ASSERT_NOT_NULL(m1);
    TEST_ASSERT_NOT_NULL(m2);
    TEST_ASSERT_FALSE(memcmp(m1->uuid, m2->uuid, BC_UUID_SIZE) == 0);

    bc_message_destroy(m1);
    bc_message_destroy(m2);
}

void test_message_serialize_deserialize_roundtrip(void)
{
    uint8_t sender[BC_PUBKEY_SIZE];
    uint8_t receiver[BC_PUBKEY_SIZE];
    randombytes_buf(sender, sizeof(sender));
    randombytes_buf(receiver, sizeof(receiver));

    const uint8_t payload[] = "test payload data 1234567890";
    bc_message_t *original = bc_message_create(sender, receiver,
                                               payload, sizeof(payload));
    TEST_ASSERT_NOT_NULL(original);
    original->ttl = 7;

    /* Serialize. */
    uint8_t buf[1024];
    size_t out_len;
    int rc = bc_message_serialize(original, buf, sizeof(buf), &out_len);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
    TEST_ASSERT_EQUAL_size_t(BC_MESSAGE_HEADER_SIZE + sizeof(payload), out_len);

    /* Deserialize. */
    bc_message_t *decoded = bc_message_deserialize(buf, out_len);
    TEST_ASSERT_NOT_NULL(decoded);

    /* Compare all fields. */
    TEST_ASSERT_EQUAL_MEMORY(original->uuid, decoded->uuid, BC_UUID_SIZE);
    TEST_ASSERT_EQUAL_UINT8(7, decoded->ttl);
    TEST_ASSERT_EQUAL_MEMORY(sender, decoded->sender_pubkey, BC_PUBKEY_SIZE);
    TEST_ASSERT_EQUAL_MEMORY(receiver, decoded->receiver_pubkey, BC_PUBKEY_SIZE);
    TEST_ASSERT_EQUAL_UINT64(original->timestamp, decoded->timestamp);
    TEST_ASSERT_EQUAL_size_t(sizeof(payload), decoded->payload_len);
    TEST_ASSERT_EQUAL_MEMORY(payload, decoded->payload, sizeof(payload));

    bc_message_destroy(original);
    bc_message_destroy(decoded);
}

void test_message_serialize_buffer_too_small(void)
{
    uint8_t key[BC_PUBKEY_SIZE] = {0};
    const uint8_t payload[] = "data";
    bc_message_t *msg = bc_message_create(key, key, payload, sizeof(payload));
    TEST_ASSERT_NOT_NULL(msg);

    uint8_t small_buf[10];
    size_t out_len;
    int rc = bc_message_serialize(msg, small_buf, sizeof(small_buf), &out_len);
    TEST_ASSERT_EQUAL_INT(BC_ERR_BUFFER_TOO_SMALL, rc);

    bc_message_destroy(msg);
}

void test_message_deserialize_invalid(void)
{
    /* Too short. */
    uint8_t garbage[5] = {1, 2, 3, 4, 5};
    bc_message_t *msg = bc_message_deserialize(garbage, sizeof(garbage));
    TEST_ASSERT_NULL(msg);

    /* NULL input. */
    msg = bc_message_deserialize(NULL, 100);
    TEST_ASSERT_NULL(msg);
}

void test_message_destroy_null_safe(void)
{
    bc_message_destroy(NULL); /* Should not crash. */
}

int main(void)
{
    if (sodium_init() < 0) {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_message_create_and_destroy);
    RUN_TEST(test_message_uuid_uniqueness);
    RUN_TEST(test_message_serialize_deserialize_roundtrip);
    RUN_TEST(test_message_serialize_buffer_too_small);
    RUN_TEST(test_message_deserialize_invalid);
    RUN_TEST(test_message_destroy_null_safe);

    return UNITY_END();
}
