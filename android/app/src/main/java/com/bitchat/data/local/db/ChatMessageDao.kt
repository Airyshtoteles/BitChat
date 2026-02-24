package com.bitchat.data.local.db

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import com.bitchat.data.local.entity.ChatMessageEntity
import kotlinx.coroutines.flow.Flow

@Dao
interface ChatMessageDao {
    @Query(
        """
        SELECT * FROM chat_messages
        WHERE peer_pub_key = :peerPubKey
        ORDER BY timestamp ASC
        """
    )
    fun getMessagesForPeer(peerPubKey: ByteArray): Flow<List<ChatMessageEntity>>

    @Insert
    suspend fun insert(message: ChatMessageEntity): Long

    @Query(
        """
        SELECT * FROM chat_messages
        WHERE peer_pub_key = :peerPubKey
        ORDER BY timestamp DESC
        LIMIT 1
        """
    )
    suspend fun getLatestMessageForPeer(peerPubKey: ByteArray): ChatMessageEntity?

    @Query(
        """
        SELECT cm.* FROM chat_messages cm
        INNER JOIN (
            SELECT peer_pub_key, MAX(timestamp) as max_ts
            FROM chat_messages
            GROUP BY peer_pub_key
        ) latest ON cm.peer_pub_key = latest.peer_pub_key
                 AND cm.timestamp = latest.max_ts
        ORDER BY cm.timestamp DESC
        """
    )
    fun getLatestMessagePerPeer(): Flow<List<ChatMessageEntity>>
}
