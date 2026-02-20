/**
 * @file test_crypto_manager.c
 * @brief Unit tests for the crypto manager (keygen, ECDH, encrypt/decrypt).
 */

#include "unity.h"
#include "bitchat/usecase/crypto_manager.h"
#include "bitchat/domain/identity.h"
#include "bitchat/error.h"

#include <sodium.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_crypto_init(void)
{
    int rc = bc_crypto_init();
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
}

void test_crypto_keygen(void)
{
    uint8_t pub[BC_PUBKEY_SIZE];
    uint8_t sec[BC_PUBKEY_SIZE];

    int rc = bc_crypto_generate_keypair(pub, sec);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);

    /* Keys should not be all zeros. */
    uint8_t zeros[BC_PUBKEY_SIZE] = {0};
    TEST_ASSERT_FALSE(memcmp(pub, zeros, BC_PUBKEY_SIZE) == 0);
    TEST_ASSERT_FALSE(memcmp(sec, zeros, BC_PUBKEY_SIZE) == 0);
}

void test_crypto_ecdh_shared_secret(void)
{
    /* Alice and Bob generate keypairs. */
    uint8_t alice_pub[BC_PUBKEY_SIZE], alice_sec[BC_PUBKEY_SIZE];
    uint8_t bob_pub[BC_PUBKEY_SIZE],   bob_sec[BC_PUBKEY_SIZE];

    bc_crypto_generate_keypair(alice_pub, alice_sec);
    bc_crypto_generate_keypair(bob_pub, bob_sec);

    /* Derive shared secrets. */
    uint8_t alice_shared[BC_SHARED_KEY_SIZE];
    uint8_t bob_shared[BC_SHARED_KEY_SIZE];

    int rc1 = bc_crypto_derive_shared(alice_sec, bob_pub, alice_shared);
    int rc2 = bc_crypto_derive_shared(bob_sec, alice_pub, bob_shared);

    TEST_ASSERT_EQUAL_INT(BC_OK, rc1);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc2);

    /* Both sides should derive the same shared key. */
    TEST_ASSERT_EQUAL_MEMORY(alice_shared, bob_shared, BC_SHARED_KEY_SIZE);
}

void test_crypto_encrypt_decrypt_roundtrip(void)
{
    uint8_t key[BC_SHARED_KEY_SIZE];
    randombytes_buf(key, sizeof(key));

    const char *plaintext = "Hello, encrypted world!";
    size_t pt_len = strlen(plaintext);

    uint8_t ciphertext[256];
    uint8_t nonce[BC_NONCE_SIZE];

    int rc = bc_crypto_encrypt((const uint8_t *)plaintext, pt_len,
                               key, ciphertext, nonce);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);

    uint8_t decrypted[256];
    rc = bc_crypto_decrypt(ciphertext, pt_len + BC_TAG_SIZE,
                           key, nonce, decrypted);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);
    TEST_ASSERT_EQUAL_MEMORY(plaintext, decrypted, pt_len);
}

void test_crypto_decrypt_wrong_key_fails(void)
{
    uint8_t key[BC_SHARED_KEY_SIZE];
    uint8_t wrong_key[BC_SHARED_KEY_SIZE];
    randombytes_buf(key, sizeof(key));
    randombytes_buf(wrong_key, sizeof(wrong_key));

    const char *plaintext = "secret data";
    size_t pt_len = strlen(plaintext);

    uint8_t ciphertext[256];
    uint8_t nonce[BC_NONCE_SIZE];

    bc_crypto_encrypt((const uint8_t *)plaintext, pt_len,
                      key, ciphertext, nonce);

    uint8_t decrypted[256];
    int rc = bc_crypto_decrypt(ciphertext, pt_len + BC_TAG_SIZE,
                               wrong_key, nonce, decrypted);
    TEST_ASSERT_EQUAL_INT(BC_ERR_CRYPTO, rc);
}

void test_crypto_decrypt_tampered_ciphertext_fails(void)
{
    uint8_t key[BC_SHARED_KEY_SIZE];
    randombytes_buf(key, sizeof(key));

    const char *plaintext = "integrity check";
    size_t pt_len = strlen(plaintext);

    uint8_t ciphertext[256];
    uint8_t nonce[BC_NONCE_SIZE];

    bc_crypto_encrypt((const uint8_t *)plaintext, pt_len,
                      key, ciphertext, nonce);

    /* Tamper with one byte. */
    ciphertext[0] ^= 0xFF;

    uint8_t decrypted[256];
    int rc = bc_crypto_decrypt(ciphertext, pt_len + BC_TAG_SIZE,
                               key, nonce, decrypted);
    TEST_ASSERT_EQUAL_INT(BC_ERR_CRYPTO, rc);
}

void test_identity_generate_and_wipe(void)
{
    bc_identity_t id;
    int rc = bc_identity_generate(&id);
    TEST_ASSERT_EQUAL_INT(BC_OK, rc);

    uint8_t zeros[BC_SECRET_KEY_SIZE] = {0};
    TEST_ASSERT_FALSE(memcmp(id.secret_key, zeros, BC_SECRET_KEY_SIZE) == 0);

    bc_identity_wipe(&id);
    TEST_ASSERT_EQUAL_MEMORY(zeros, id.secret_key, BC_SECRET_KEY_SIZE);
}

int main(void)
{
    if (sodium_init() < 0) {
        return 1;
    }

    UNITY_BEGIN();

    RUN_TEST(test_crypto_init);
    RUN_TEST(test_crypto_keygen);
    RUN_TEST(test_crypto_ecdh_shared_secret);
    RUN_TEST(test_crypto_encrypt_decrypt_roundtrip);
    RUN_TEST(test_crypto_decrypt_wrong_key_fails);
    RUN_TEST(test_crypto_decrypt_tampered_ciphertext_fails);
    RUN_TEST(test_identity_generate_and_wipe);

    return UNITY_END();
}
