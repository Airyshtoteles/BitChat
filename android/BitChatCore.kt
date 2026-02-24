/**
 * BitChatCore — Kotlin singleton wrapping the native C library.
 *
 * Usage:
 *   1. Copy this file into your Android project under com/bitchat/core/.
 *   2. Place libbitchat_jni.so into jniLibs/<abi>/.
 *   3. Call BitChatCore.init(context.filesDir.absolutePath + "/bitchat.db")
 *
 * All methods return Int status codes (0 = success, negative = error).
 * See error.h for the full list.
 */
package com.bitchat.core

object BitChatCore {

    init {
        System.loadLibrary("bitchat_jni")
    }

    /** Callback interface for incoming decrypted messages. */
    interface MessageListener {
        /**
         * Called when a message addressed to us arrives and is decrypted.
         *
         * WARNING: This may fire from a background thread (the tick thread).
         * Do NOT call BitChatCore methods from within this callback.
         *
         * @param senderPubKey  Sender's public key (32 bytes).
         * @param payload       Decrypted plaintext.
         */
        fun onMessageReceived(senderPubKey: ByteArray, payload: ByteArray)
    }

    /**
     * Initialize the BitChat core.
     * @param storagePath  Path to the SQLite database file.
     * @return 0 on success.
     */
    external fun init(storagePath: String): Int

    /**
     * Generate a new X25519 identity (keypair).
     * @return 64 bytes: pubkey(32) || secret(32), or null on failure.
     */
    external fun generateIdentity(): ByteArray?

    /**
     * Load an existing identity from a serialized buffer.
     * @param identityData  64 bytes: pubkey(32) || secret(32).
     * @return 0 on success.
     */
    external fun loadIdentity(identityData: ByteArray): Int

    /**
     * Send an encrypted message to a recipient.
     * @param recipient  Recipient's public key (32 bytes).
     * @param plaintext  Message content.
     * @return 0 on success.
     */
    external fun sendMessage(recipient: ByteArray, plaintext: ByteArray): Int

    /**
     * Poll for one incoming message (non-blocking).
     * @return sender(32) || plaintext(N) bytes, or null if no message.
     */
    external fun pollMessages(): ByteArray?

    /** Start peer discovery on available transports. */
    external fun startDiscovery(): Int

    /** Run one maintenance cycle (call periodically, e.g. every 5s). */
    external fun tick()

    /** Shut down the core, releasing all resources. */
    external fun shutdown()

    /* Native callback registration — private, use setMessageListener(). */
    private external fun nativeSetOnMessageCallback(listener: MessageListener?): Int

    /**
     * Register a listener for push-based message delivery.
     * Pass null to unregister.
     *
     * @param listener  MessageListener implementation, or null.
     * @return 0 on success.
     */
    fun setMessageListener(listener: MessageListener?): Int {
        return nativeSetOnMessageCallback(listener)
    }
}
