/**
 * @file bitchat_jni.c
 * @brief JNI bridge for Android — maps Java methods to the C FFI.
 *
 * Java class: com.bitchat.core.BitChatCore
 *
 * This bridge translates JNI types (jbyteArray, jstring) to the C types
 * expected by bitchat_ffi.h functions. Each JNI method is a thin wrapper
 * around the corresponding FFI function.
 */

#ifdef BITCHAT_BUILD_JNI

#include <jni.h>
#include "bitchat/bridge/bitchat_ffi.h"

#include <string.h>
#include <stdlib.h>

/* ---------- Helper macros ---------- */

#define JNI_FUNC(ret, name) \
    JNIEXPORT ret JNICALL Java_com_bitchat_core_BitChatCore_##name

/* ---------- JNI methods ---------- */

JNI_FUNC(jint, init)(JNIEnv *env, jobject thiz, jstring storage_path)
{
    (void)thiz;

    const char *path = (*env)->GetStringUTFChars(env, storage_path, NULL);
    if (!path) {
        return -1;
    }

    int rc = bitchat_init(path);
    (*env)->ReleaseStringUTFChars(env, storage_path, path);
    return (jint)rc;
}

JNI_FUNC(jbyteArray, generateIdentity)(JNIEnv *env, jobject thiz)
{
    (void)thiz;

    uint8_t pubkey[32];
    int rc = bitchat_generate_identity(pubkey);
    if (rc != 0) {
        return NULL;
    }

    jbyteArray result = (*env)->NewByteArray(env, 32);
    if (result) {
        (*env)->SetByteArrayRegion(env, result, 0, 32, (const jbyte *)pubkey);
    }
    return result;
}

JNI_FUNC(jint, loadIdentity)(JNIEnv *env, jobject thiz, jbyteArray identity_data)
{
    (void)thiz;

    jsize len = (*env)->GetArrayLength(env, identity_data);
    jbyte *data = (*env)->GetByteArrayElements(env, identity_data, NULL);
    if (!data) {
        return -1;
    }

    int rc = bitchat_load_identity((const uint8_t *)data, (size_t)len);
    (*env)->ReleaseByteArrayElements(env, identity_data, data, JNI_ABORT);
    return (jint)rc;
}

JNI_FUNC(jint, sendMessage)(JNIEnv *env, jobject thiz,
                            jbyteArray recipient, jbyteArray plaintext)
{
    (void)thiz;

    jbyte *rcpt = (*env)->GetByteArrayElements(env, recipient, NULL);
    jbyte *pt   = (*env)->GetByteArrayElements(env, plaintext, NULL);
    jsize  pt_len = (*env)->GetArrayLength(env, plaintext);

    if (!rcpt || !pt) {
        if (rcpt) (*env)->ReleaseByteArrayElements(env, recipient, rcpt, JNI_ABORT);
        if (pt)   (*env)->ReleaseByteArrayElements(env, plaintext, pt, JNI_ABORT);
        return -1;
    }

    int rc = bitchat_send_message((const uint8_t *)rcpt,
                                  (const uint8_t *)pt, (size_t)pt_len);

    (*env)->ReleaseByteArrayElements(env, recipient, rcpt, JNI_ABORT);
    (*env)->ReleaseByteArrayElements(env, plaintext, pt, JNI_ABORT);
    return (jint)rc;
}

JNI_FUNC(jbyteArray, pollMessages)(JNIEnv *env, jobject thiz)
{
    (void)thiz;

    uint8_t buf[4096];
    size_t  msg_len = 0;
    uint8_t sender[32];

    int rc = bitchat_poll_messages(buf, sizeof(buf), &msg_len, sender);
    if (rc != 0 || msg_len == 0) {
        return NULL;
    }

    /* Return: sender(32) || plaintext(msg_len) */
    jbyteArray result = (*env)->NewByteArray(env, (jsize)(32 + msg_len));
    if (result) {
        (*env)->SetByteArrayRegion(env, result, 0, 32, (const jbyte *)sender);
        (*env)->SetByteArrayRegion(env, result, 32, (jsize)msg_len,
                                   (const jbyte *)buf);
    }
    return result;
}

JNI_FUNC(jint, startDiscovery)(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    return (jint)bitchat_start_discovery();
}

JNI_FUNC(void, tick)(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    bitchat_tick();
}

JNI_FUNC(void, shutdown)(JNIEnv *env, jobject thiz)
{
    (void)env; (void)thiz;
    bitchat_shutdown();
}

#endif /* BITCHAT_BUILD_JNI */
