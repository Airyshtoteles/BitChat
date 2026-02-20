/**
 * @file test_node_discovery.c
 * @brief Unit tests for the peer discovery manager.
 */

#include "unity.h"
#include "bitchat/usecase/node_discovery.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <string.h>

/* ---------- mock transport ---------- */

static bc_peer_t g_mock_found_peers[4];
static size_t g_mock_found_count = 0;

static int mock_discover(bc_transport_t *self,
                         bc_peer_t *peers_out, size_t max_peers,
                         size_t *found)
{
    (void)self;
    size_t n = (g_mock_found_count < max_peers) ? g_mock_found_count : max_peers;
    memcpy(peers_out, g_mock_found_peers, n * sizeof(bc_peer_t));
    *found = n;
    return BC_OK;
}

static void mock_destroy(bc_transport_t *self) { (void)self; }

static bc_transport_t g_mock_transport;

/* ---------- callback tracking ---------- */

static int g_callback_count;
static uint8_t g_last_found_pubkey[BC_PUBKEY_SIZE];

static void on_peer_found(const bc_peer_t *peer, void *userdata)
{
    (void)userdata;
    g_callback_count++;
    memcpy(g_last_found_pubkey, peer->pubkey, BC_PUBKEY_SIZE);
}

/* ---------- fixtures ---------- */

void setUp(void)
{
    memset(&g_mock_transport, 0, sizeof(g_mock_transport));
    g_mock_transport.discover_peers = mock_discover;
    g_mock_transport.destroy = mock_destroy;

    g_mock_found_count = 0;
    g_callback_count = 0;
    memset(g_last_found_pubkey, 0, BC_PUBKEY_SIZE);
}

void tearDown(void) {}

/* ---------- tests ---------- */

void test_discovery_create_destroy(void)
{
    bc_discovery_t *disc = bc_discovery_create(&g_mock_transport);
    TEST_ASSERT_NOT_NULL(disc);
    TEST_ASSERT_EQUAL_size_t(0, disc->peer_count);
    bc_discovery_destroy(disc);
}

void test_discovery_scan_finds_new_peer(void)
{
    uint8_t pk[BC_PUBKEY_SIZE];
    randombytes_buf(pk, sizeof(pk));

    bc_peer_init(&g_mock_found_peers[0], pk, BC_TRANSPORT_BLE);
    g_mock_found_count = 1;

    bc_discovery_t *disc = bc_discovery_create(&g_mock_transport);
    bc_discovery_on_peer_found(disc, on_peer_found, NULL);

    int rc = bc_discovery_scan(disc);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);
    TEST_ASSERT_EQUAL_size_t(1, disc->peer_count);
    TEST_ASSERT_EQUAL_MEMORY(pk, g_last_found_pubkey, BC_PUBKEY_SIZE);

    bc_discovery_destroy(disc);
}

void test_discovery_scan_existing_peer_no_duplicate_callback(void)
{
    uint8_t pk[BC_PUBKEY_SIZE];
    randombytes_buf(pk, sizeof(pk));

    bc_peer_init(&g_mock_found_peers[0], pk, BC_TRANSPORT_BLE);
    g_mock_found_count = 1;

    bc_discovery_t *disc = bc_discovery_create(&g_mock_transport);
    bc_discovery_on_peer_found(disc, on_peer_found, NULL);

    /* First scan: new peer. */
    bc_discovery_scan(disc);
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);

    /* Second scan: same peer — should NOT trigger callback again. */
    bc_discovery_scan(disc);
    TEST_ASSERT_EQUAL_INT(1, g_callback_count);
    TEST_ASSERT_EQUAL_size_t(1, disc->peer_count);

    bc_discovery_destroy(disc);
}

void test_discovery_get_peers(void)
{
    uint8_t pk1[BC_PUBKEY_SIZE] = {1};
    uint8_t pk2[BC_PUBKEY_SIZE] = {2};

    bc_peer_init(&g_mock_found_peers[0], pk1, BC_TRANSPORT_BLE);
    bc_peer_init(&g_mock_found_peers[1], pk2, BC_TRANSPORT_WIFI_DIRECT);
    g_mock_found_count = 2;

    bc_discovery_t *disc = bc_discovery_create(&g_mock_transport);
    bc_discovery_scan(disc);

    bc_peer_t out[4];
    size_t n = bc_discovery_get_peers(disc, out, 4);
    TEST_ASSERT_EQUAL_size_t(2, n);
    TEST_ASSERT_EQUAL_MEMORY(pk1, out[0].pubkey, BC_PUBKEY_SIZE);
    TEST_ASSERT_EQUAL_MEMORY(pk2, out[1].pubkey, BC_PUBKEY_SIZE);

    bc_discovery_destroy(disc);
}

int main(void)
{
    if (sodium_init() < 0) {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_discovery_create_destroy);
    RUN_TEST(test_discovery_scan_finds_new_peer);
    RUN_TEST(test_discovery_scan_existing_peer_no_duplicate_callback);
    RUN_TEST(test_discovery_get_peers);

    return UNITY_END();
}
