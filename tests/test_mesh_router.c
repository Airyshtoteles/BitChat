/**
 * @file test_mesh_router.c
 * @brief Unit tests for the gossip mesh router.
 */

#include "unity.h"
#include "bitchat/usecase/mesh_router.h"
#include "bitchat/domain/message.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <string.h>

/* ---------- mock transport ---------- */

typedef struct {
    uint8_t sent_data[4096];
    size_t  sent_len;
    int     send_count;
} mock_transport_ctx_t;

static int mock_send(bc_transport_t *self,
                     const uint8_t peer_pubkey[BC_PUBKEY_SIZE],
                     const uint8_t *data, size_t len)
{
    (void)peer_pubkey;
    mock_transport_ctx_t *ctx = (mock_transport_ctx_t *)self->ctx;
    if (len <= sizeof(ctx->sent_data)) {
        memcpy(ctx->sent_data, data, len);
        ctx->sent_len = len;
    }
    ctx->send_count++;
    return BC_OK;
}

static int mock_recv(bc_transport_t *self,
                     uint8_t *buf, size_t buf_len, size_t *out_len,
                     uint8_t peer_pubkey_out[BC_PUBKEY_SIZE])
{
    (void)self; (void)buf; (void)buf_len; (void)peer_pubkey_out;
    *out_len = 0;
    return BC_ERR;
}

static void mock_destroy(bc_transport_t *self)
{
    (void)self;
}

static bc_transport_t g_mock_transport;
static mock_transport_ctx_t g_mock_ctx;

static void setup_mock_transport(void)
{
    memset(&g_mock_ctx, 0, sizeof(g_mock_ctx));
    memset(&g_mock_transport, 0, sizeof(g_mock_transport));
    g_mock_transport.send    = mock_send;
    g_mock_transport.recv    = mock_recv;
    g_mock_transport.destroy = mock_destroy;
    g_mock_transport.ctx     = &g_mock_ctx;
}

/* ---------- test message delivery callback ---------- */

static int g_delivered_count;
static uint8_t g_delivered_uuid[BC_UUID_SIZE];

static void on_message(const bc_message_t *msg, void *userdata)
{
    (void)userdata;
    g_delivered_count++;
    memcpy(g_delivered_uuid, msg->uuid, BC_UUID_SIZE);
}

/* ---------- test fixtures ---------- */

void setUp(void)
{
    setup_mock_transport();
    g_delivered_count = 0;
    memset(g_delivered_uuid, 0, BC_UUID_SIZE);
}

void tearDown(void) {}

/* ---------- tests ---------- */

void test_router_create_destroy(void)
{
    uint8_t pubkey[BC_PUBKEY_SIZE] = {0};
    bc_router_t *router = bc_router_create(&g_mock_transport, NULL, pubkey, NULL);
    TEST_ASSERT_NOT_NULL(router);
    bc_router_destroy(router);
}

void test_router_broadcast_sends_to_peers(void)
{
    uint8_t my_pubkey[BC_PUBKEY_SIZE];
    randombytes_buf(my_pubkey, sizeof(my_pubkey));

    bc_router_t *router = bc_router_create(&g_mock_transport, NULL, my_pubkey, NULL);
    TEST_ASSERT_NOT_NULL(router);

    /* Add a peer. */
    bc_peer_t peer;
    uint8_t peer_pubkey[BC_PUBKEY_SIZE];
    randombytes_buf(peer_pubkey, sizeof(peer_pubkey));
    bc_peer_init(&peer, peer_pubkey, BC_TRANSPORT_BLE);
    bc_router_add_peer(router, &peer);

    /* Broadcast a message. */
    uint8_t receiver[BC_PUBKEY_SIZE];
    randombytes_buf(receiver, sizeof(receiver));

    bc_message_t *msg = bc_message_create(my_pubkey, receiver,
                                          (const uint8_t *)"hello", 5);
    TEST_ASSERT_NOT_NULL(msg);

    int rc = bc_router_broadcast(router, msg);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, g_mock_ctx.send_count);

    bc_message_destroy(msg);
    bc_router_destroy(router);
}

void test_router_receive_dedup_rejects_duplicate(void)
{
    uint8_t my_pubkey[BC_PUBKEY_SIZE];
    randombytes_buf(my_pubkey, sizeof(my_pubkey));

    bc_router_t *router = bc_router_create(&g_mock_transport, NULL, my_pubkey, NULL);

    /* Create a message. */
    uint8_t sender[BC_PUBKEY_SIZE];
    randombytes_buf(sender, sizeof(sender));
    bc_message_t *msg = bc_message_create(sender, my_pubkey,
                                          (const uint8_t *)"data", 4);
    TEST_ASSERT_NOT_NULL(msg);

    /* Serialize it. */
    uint8_t buf[1024];
    size_t out_len;
    bc_message_serialize(msg, buf, sizeof(buf), &out_len);

    /* First receive should succeed. */
    bc_router_set_on_message(router, on_message, NULL);
    int rc = bc_router_receive(router, buf, out_len);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, g_delivered_count);

    /* Second receive of same message should be rejected. */
    rc = bc_router_receive(router, buf, out_len);
    TEST_ASSERT_EQUAL_INT(BC_ERR_DUPLICATE, rc);
    TEST_ASSERT_EQUAL_INT(1, g_delivered_count);

    bc_message_destroy(msg);
    bc_router_destroy(router);
}

void test_router_receive_ttl_zero_drops(void)
{
    uint8_t my_pubkey[BC_PUBKEY_SIZE];
    uint8_t other_pubkey[BC_PUBKEY_SIZE];
    randombytes_buf(my_pubkey, sizeof(my_pubkey));
    randombytes_buf(other_pubkey, sizeof(other_pubkey));

    bc_router_t *router = bc_router_create(&g_mock_transport, NULL, my_pubkey, NULL);
    bc_router_set_on_message(router, on_message, NULL);

    /* Create a message with TTL=0 addressed to someone else. */
    bc_message_t *msg = bc_message_create(other_pubkey, other_pubkey,
                                          (const uint8_t *)"x", 1);
    msg->ttl = 0;

    uint8_t buf[1024];
    size_t out_len;
    bc_message_serialize(msg, buf, sizeof(buf), &out_len);

    int rc = bc_router_receive(router, buf, out_len);
    TEST_ASSERT_EQUAL_INT(BC_ERR_TTL_EXPIRED, rc);

    bc_message_destroy(msg);
    bc_router_destroy(router);
}

void test_router_receive_delivers_to_local(void)
{
    uint8_t my_pubkey[BC_PUBKEY_SIZE];
    uint8_t sender_key[BC_PUBKEY_SIZE];
    randombytes_buf(my_pubkey, sizeof(my_pubkey));
    randombytes_buf(sender_key, sizeof(sender_key));

    bc_router_t *router = bc_router_create(&g_mock_transport, NULL, my_pubkey, NULL);
    bc_router_set_on_message(router, on_message, NULL);

    /* Message addressed to us. */
    bc_message_t *msg = bc_message_create(sender_key, my_pubkey,
                                          (const uint8_t *)"for me!", 7);

    uint8_t buf[1024];
    size_t out_len;
    bc_message_serialize(msg, buf, sizeof(buf), &out_len);

    int rc = bc_router_receive(router, buf, out_len);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, g_delivered_count);
    TEST_ASSERT_EQUAL_MEMORY(msg->uuid, g_delivered_uuid, BC_UUID_SIZE);

    bc_message_destroy(msg);
    bc_router_destroy(router);
}

void test_router_fanout_limit(void)
{
    uint8_t my_pubkey[BC_PUBKEY_SIZE];
    randombytes_buf(my_pubkey, sizeof(my_pubkey));

    bc_router_config_t config = {
        .fanout = 2,
        .dedup_cap = 100,
        .dedup_max_age = 60
    };

    bc_router_t *router = bc_router_create(&g_mock_transport, NULL,
                                           my_pubkey, &config);

    /* Add 5 peers. */
    for (int i = 0; i < 5; i++) {
        bc_peer_t peer;
        uint8_t pk[BC_PUBKEY_SIZE] = {0};
        pk[0] = (uint8_t)(i + 1);
        bc_peer_init(&peer, pk, BC_TRANSPORT_BLE);
        bc_router_add_peer(router, &peer);
    }

    /* Broadcast. */
    uint8_t receiver[BC_PUBKEY_SIZE] = {0xFF};
    bc_message_t *msg = bc_message_create(my_pubkey, receiver,
                                          (const uint8_t *)"fan", 3);

    bc_router_broadcast(router, msg);

    /* Should have sent to exactly fanout=2 peers. */
    TEST_ASSERT_EQUAL_INT(2, g_mock_ctx.send_count);

    bc_message_destroy(msg);
    bc_router_destroy(router);
}

int main(void)
{
    if (sodium_init() < 0) {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_router_create_destroy);
    RUN_TEST(test_router_broadcast_sends_to_peers);
    RUN_TEST(test_router_receive_dedup_rejects_duplicate);
    RUN_TEST(test_router_receive_ttl_zero_drops);
    RUN_TEST(test_router_receive_delivers_to_local);
    RUN_TEST(test_router_fanout_limit);

    return UNITY_END();
}
