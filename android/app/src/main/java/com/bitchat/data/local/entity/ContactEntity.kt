package com.bitchat.data.local.entity

import androidx.room.ColumnInfo
import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "contacts")
data class ContactEntity(
    @PrimaryKey
    @ColumnInfo(name = "pub_key", typeAffinity = ColumnInfo.BLOB)
    val pubKey: ByteArray,

    @ColumnInfo(name = "display_name")
    val displayName: String? = null,

    @ColumnInfo(name = "added_at")
    val addedAt: Long,

    @ColumnInfo(name = "last_message_at")
    val lastMessageAt: Long? = null
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (other !is ContactEntity) return false
        return pubKey.contentEquals(other.pubKey)
    }

    override fun hashCode(): Int = pubKey.contentHashCode()
}
