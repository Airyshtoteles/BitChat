package com.bitchat.data.local.db

import androidx.room.Database
import androidx.room.RoomDatabase
import com.bitchat.data.local.entity.ChatMessageEntity
import com.bitchat.data.local.entity.ContactEntity

@Database(
    entities = [ContactEntity::class, ChatMessageEntity::class],
    version = 1,
    exportSchema = false
)
abstract class BitChatDatabase : RoomDatabase() {
    abstract fun contactDao(): ContactDao
    abstract fun chatMessageDao(): ChatMessageDao
}
