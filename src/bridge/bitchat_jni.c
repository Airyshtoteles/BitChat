/**
 * @file bitchat_jni.c
 * @brief JNI bridge for Android — maps Kotlin/Java methods to the C FFI.
 *
 * Java/Kotlin class: com.bitchat.core.BitChatCore
 *
 * Thread safety:
 * - JavaVM* is cached in JNI_OnLoad (process-wide, never changes).
 * - The Kotlin MessageListener is held as a GlobalRef behind g_listener_mutex.
 * - The C-level on_message callback uses AttachCurrentThread when invoked
 *   from a non-JVM thread (e.g. the tick thread).
 *
 * Null safety:
 * - Every jbyteArray parameter is checked for NULL and correct length
 *   before calling GetByteArrayElements.
 * - Returns BC_ERR_INVALID (-3) on bad input instead of crashing.
 */

#ifdef BITCHAT_BUILD_JNI

#include <jni.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "bitchat/bridge/bitchat_ffi.h"
#include "bitchat/error.h"

#include <sodium.h>

/* ---------- Helper macros ---------- */

#define JNI_FUNC(ret, name) \
    JNIEXPORT ret JNICALL Java_com_bitchat_core_BitChatCore_##name

/* ---------- Global JVM state ---------- */

static JavaVM *g_jvm = NULL;

static pthread_mutex_t g_listener_mutex = PTHREAD_MUTEX_INITIALIZER;
static jobject    g_listener_ref        = NULL;  /* GlobalRef to Kotlin listener */
static jmethodID  g_on_message_method   = NULL;  /* cached methodID             */

/* ---------- JNI_OnLoad / JNI_OnUnload ---------- */

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved)
{
    (void)reserved;
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved)
{
    (void)reserved;

    pthread_mutex_lock(&g_listener_mutex);

    if (g_listener_ref) {
        JNIEnv *env = NULL;
        if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) == JNI_OK && env) {
            (*env)->DeleteGlobalRef(env, g_listener_ref);
        }
        g_listener_ref = NULL;
    }
    g_on_message_method = NULL;

    pthread_mutex_unlock(&g_listener_mutex);

    g_jvm = NULL;
}

/* ---------- C callback -> JNI -> Kotlin ---------- */

/**
 * Registered as the bitchat_on_message_fn callback.
 * Called from the C layer (possibly from a non-JVM thread).
 */
static void jni_on_message_callback(const uint8_t sender[32],
                                    const uint8_t *payload,
                                    size_t len)
{
    if (!g_jvm) {
        return;
    }

    pthread_mutex_lock(&g_listener_mutex);

    if (!g_listener_ref || !g_on_message_method) {
        pthread_mutex_unlock(&g_listener_mutex);
        return;
    }

    JNIEnv *env = NULL;
    int did_attach = 0;

    jint get_rc = (*g_jvm)->GetEnv(g_jvm, (void **)&env, JNI_VERSION_1_6);
    if (get_rc == JNI_EDETACHED) {
        if ((*g_jvm)->AttachCurrentThread(g_jvm, (void **)&env, NULL) != JNI_OK) {
            pthread_mutex_unlock(&g_listener_mutex);
            return;
        }
        did_attach = 1;
    } else if (get_rc != JNI_OK) {
        pthread_mutex_unlock(&g_listener_mutex);
        return;
    }

    /* Create byte arrays for sender and payload. */
    jbyteArray j_sender  = (*env)->NewByteArray(env, 32);
    jbyteArray j_payload = (*env)->NewByteArray(env, (jsize)len);

    if (j_sender && j_payload) {
        (*env)->SetByteArrayRegion(env, j_sender, 0, 32,
                                   (const jbyte *)sender);
        (*env)->SetByteArrayRegion(env, j_payload, 0, (jsize)len,
                                   (const jbyte *)payload);

        (*env)->CallVoidMethod(env, g_listener_ref, g_on_message_method,
                               j_sender, j_payload);

        /* Prevent unhandled Java exception from crashing the C side. */
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionClear(env);
        }
    }

    if (j_sender)  (*env)->DeleteLocalRef(env, j_sender);
    if (j_payload) (*env)->DeleteLocalRef(env, j_payload);

    pthread_mutex_unlock(&g_listener_mutex);

    if (did_attach) {
        (*g_jvm)->DetachCurrentThread(g_jvm);
    }
}

/* ---------- JNI methods ---------- */

JNI_FUNC(jint, init)(JNIEnv *env, jobject thiz, jstring storage_path)
{
    (void)thiz;

    if (!storage_path) {
        return (jint)BC_ERR_INVALID;
    }

    const char *path = (*env)->GetStringUTFChars(env, storage_path, NULL);
    if (!path) {
        return (jint)BC_ERR_INVALID;
    }

    int rc = bitchat_init(path);
    (*env)->ReleaseStringUTFChars(env, storage_path, path);
    return (jint)rc;
}

JNI_FUNC(jbyteArray, generateIdentity)(JNIEnv *env, jobject thiz)
{
    (void)thiz;

    uint8_t pubkey[32];
    uint8_t secret[32];

    int rc = bitchat_generate_identity(pubkey, secret);
    if (rc != 0) {
        return NULL;
    }

    /* Return 64 bytes: pubkey(32) || secret(32). */
    jbyteArray result = (*env)->NewByteArray(env, 64);
    if (result) {
        (*env)->SetByteArrayRegion(env, result, 0, 32,
                                   (const jbyte *)pubkey);
        (*env)->SetByteArrayRegion(env, result, 32, 32,
                                   (const jbyte *)secret);
    }

    sodium_memzero(secret, sizeof(secret));
    return result;
}

JNI_FUNC(jint, loadIdentity)(JNIEnv *env, jobject thiz,
                              jbyteArray identity_data)
{
    (void)thiz;

    if (!identity_data) {
        return (jint)BC_ERR_INVALID;
    }

    jsize len = (*env)->GetArrayLength(env, identity_data);
    if (len < 64) {
        return (jint)BC_ERR_INVALID;
    }

    jbyte *data = (*env)->GetByteArrayElements(env, identity_data, NULL);
    if (!data) {
        return (jint)BC_ERR_INVALID;
    }

    int rc = bitchat_load_identity((const uint8_t *)data, (size_t)len);
    (*env)->ReleaseByteArrayElements(env, identity_data, data, JNI_ABORT);
    return (jint)rc;
}

JNI_FUNC(jint, sendMessage)(JNIEnv *env, jobject thiz,
                             jbyteArray recipient, jbyteArray plaintext)
{
    (void)thiz;

    if (!recipient || !plaintext) {
        return (jint)BC_ERR_INVALID;
    }

    jsize rcpt_len = (*env)->GetArrayLength(env, recipient);
    jsize pt_len   = (*env)->GetArrayLength(env, plaintext);

    if (rcpt_len != 32 || pt_len <= 0) {
        return (jint)BC_ERR_INVALID;
    }

    jbyte *rcpt = (*env)->GetByteArrayElements(env, recipient, NULL);
    jbyte *pt   = (*env)->GetByteArrayElements(env, plaintext, NULL);

    if (!rcpt || !pt) {
        if (rcpt) (*env)->ReleaseByteArrayElements(env, recipient, rcpt, JNI_ABORT);
        if (pt)   (*env)->ReleaseByteArrayElements(env, plaintext, pt, JNI_ABORT);
        return (jint)BC_ERR_INVALID;
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

    /* Return: sender(32) || plaintext(msg_len). */
    jbyteArray result = (*env)->NewByteArray(env, (jsize)(32 + msg_len));
    if (result) {
        (*env)->SetByteArrayRegion(env, result, 0, 32, (const jbyte *)sender);
        (*env)->SetByteArrayRegion(env, result, 32, (jsize)msg_len,
                                   (const jbyte *)buf);
    }

    sodium_memzero(buf, msg_len);
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

JNI_FUNC(jint, nativeSetOnMessageCallback)(JNIEnv *env, jobject thiz,
                                            jobject listener)
{
    (void)thiz;

    pthread_mutex_lock(&g_listener_mutex);

    /* Release previous listener if any. */
    if (g_listener_ref) {
        (*env)->DeleteGlobalRef(env, g_listener_ref);
        g_listener_ref = NULL;
        g_on_message_method = NULL;
    }

    if (listener) {
        g_listener_ref = (*env)->NewGlobalRef(env, listener);
        if (!g_listener_ref) {
            pthread_mutex_unlock(&g_listener_mutex);
            return (jint)BC_ERR_NOMEM;
        }

        /* Cache the methodID for onMessageReceived([B[B)V. */
        jclass cls = (*env)->GetObjectClass(env, listener);
        g_on_message_method = (*env)->GetMethodID(env, cls,
                                                   "onMessageReceived",
                                                   "([B[B)V");
        (*env)->DeleteLocalRef(env, cls);

        if (!g_on_message_method) {
            (*env)->DeleteGlobalRef(env, g_listener_ref);
            g_listener_ref = NULL;
            if ((*env)->ExceptionCheck(env)) {
                (*env)->ExceptionClear(env);
            }
            pthread_mutex_unlock(&g_listener_mutex);
            return (jint)BC_ERR_INVALID;
        }
    }

    pthread_mutex_unlock(&g_listener_mutex);

    /* Register or unregister the C-level callback. */
    return (jint)bitchat_set_on_message_callback(
        listener ? jni_on_message_callback : NULL);
}

JNI_FUNC(void, shutdown)(JNIEnv *env, jobject thiz)
{
    (void)thiz;

    /* Clear GlobalRef BEFORE shutdown to prevent callbacks during teardown. */
    pthread_mutex_lock(&g_listener_mutex);
    if (g_listener_ref) {
        (*env)->DeleteGlobalRef(env, g_listener_ref);
        g_listener_ref = NULL;
        g_on_message_method = NULL;
    }
    pthread_mutex_unlock(&g_listener_mutex);

    bitchat_shutdown();
}

#endif /* BITCHAT_BUILD_JNI */
