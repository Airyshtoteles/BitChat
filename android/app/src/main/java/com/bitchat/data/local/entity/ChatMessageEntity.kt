package com.bitchat.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.Index
import androidx.room.PrimaryKey

@Entity(
    tableName = "chat_messages",
    indices = [
        Index("peer_pub_key"),
        Index("timestamp")
    ]
)
data class ChatMessageEntity(
    @PrimaryKey(autoGenerate = true)
    val id: Long = 0,

    @ColumnInfo(name = "peer_pub_key", typeAffinity = ColumnInfo.BLOB)
    val peerPubKey: ByteArray,

    @ColumnInfo(name = "content")
    val content: String,

    @ColumnInfo(name = "is_outgoing")
    val isOutgoing: Boolean,

    @ColumnInfo(name = "timestamp")
    val timestamp: Long,

    @ColumnInfo(name = "status")
    val status: Int = 0
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ChatMessageEntity) return false
        return id == other.id
    }

    override fun hashCode(): Int = id.hashCode()
}
