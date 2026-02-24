package com.bitchat.domain.model

data class ChatMessage(
    val id: Long,
    val peerPubKey: ByteArray,
    val content: String,
    val isOutgoing: Boolean,
    val timestamp: Long,
    val status: MessageStatus
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ChatMessage) return false
        return id == other.id
    }

    override fun hashCode(): Int = id.hashCode()
}

enum class MessageStatus {
    SENT,
    PENDING,
    FAILED
}
