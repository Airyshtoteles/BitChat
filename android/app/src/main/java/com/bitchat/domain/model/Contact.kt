package com.bitchat.domain.model

data class Contact(
    val pubKey: ByteArray,
    val displayName: String,
    val avatarColor: Long,
    val addedAt: Long,
    val lastMessagePreview: String? = null,
    val lastMessageAt: Long? = null
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is Contact) return false
        return pubKey.contentEquals(other.pubKey)
    }

    override fun hashCode(): Int = pubKey.contentHashCode()
}
