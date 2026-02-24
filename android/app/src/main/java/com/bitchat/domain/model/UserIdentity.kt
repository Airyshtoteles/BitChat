package com.bitchat.domain.model

data class UserIdentity(
    val pubKey: ByteArray,
    val secretKey: ByteArray,
    val displayName: String,
    val avatarColor: Long
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is UserIdentity) return false
        return pubKey.contentEquals(other.pubKey)
    }

    override fun hashCode(): Int = pubKey.contentHashCode()
}
