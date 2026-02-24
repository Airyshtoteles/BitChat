package com.bitchat.data.repository

import com.bitchat.core.BitChatCore
import com.bitchat.data.local.db.ChatMessageDao
import com.bitchat.data.local.entity.ChatMessageEntity
import com.bitchat.domain.model.ChatMessage
import com.bitchat.domain.model.MessageStatus
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.withContext
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class MessageRepository @Inject constructor(
    private val chatMessageDao: ChatMessageDao,
    private val contactRepository: ContactRepository
) {
    fun getMessagesForPeer(peerPubKey: ByteArray): Flow<List<ChatMessage>> =
        chatMessageDao.getMessagesForPeer(peerPubKey).map { entities ->
            entities.map { it.toDomain() }
        }

    fun getChatPreviews(): Flow<List<ChatMessage>> =
        chatMessageDao.getLatestMessagePerPeer().map { entities ->
            entities.map { it.toDomain() }
        }

    suspend fun sendMessage(peerPubKey: ByteArray, content: String): Result<Unit> =
        withContext(Dispatchers.IO) {
            val now = System.currentTimeMillis()

            chatMessageDao.insert(
                ChatMessageEntity(
                    peerPubKey = peerPubKey,
                    content = content,
                    isOutgoing = true,
                    timestamp = now,
                    status = MessageStatus.SENT.ordinal
                )
            )

            val rc = BitChatCore.sendMessage(peerPubKey, content.toByteArray(Charsets.UTF_8))
            if (rc != 0) {
                return@withContext Result.failure(RuntimeException("sendMessage failed: $rc"))
            }

            contactRepository.updateLastMessageTime(peerPubKey, now)
            Result.success(Unit)
        }

    suspend fun receiveMessage(senderPubKey: ByteArray, plaintext: ByteArray) {
        val now = System.currentTimeMillis()
        val content = String(plaintext, Charsets.UTF_8)

        chatMessageDao.insert(
            ChatMessageEntity(
                peerPubKey = senderPubKey,
                content = content,
                isOutgoing = false,
                timestamp = now,
                status = MessageStatus.SENT.ordinal
            )
        )

        if (contactRepository.getContact(senderPubKey) == null) {
            contactRepository.addContact(senderPubKey)
        }

        contactRepository.updateLastMessageTime(senderPubKey, now)
    }

    private fun ChatMessageEntity.toDomain() = ChatMessage(
        id = id,
        peerPubKey = peerPubKey,
        content = content,
        isOutgoing = isOutgoing,
        timestamp = timestamp,
        status = MessageStatus.entries[status]
    )
}
