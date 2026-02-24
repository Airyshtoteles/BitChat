package com.bitchat.data.repository

import com.bitchat.data.local.db.ContactDao
import com.bitchat.data.local.entity.ContactEntity
import com.bitchat.domain.model.Contact
import com.bitchat.domain.util.PubKeyUtils
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class ContactRepository @Inject constructor(
    private val contactDao: ContactDao
) {
    fun getAllContacts(): Flow<List<Contact>> =
        contactDao.getAllContacts().map { entities ->
            entities.map { it.toDomain() }
        }

    suspend fun addContact(pubKey: ByteArray, alias: String? = null) {
        contactDao.upsert(
            ContactEntity(
                pubKey = pubKey,
                displayName = alias,
                addedAt = System.currentTimeMillis()
            )
        )
    }

    suspend fun getContact(pubKey: ByteArray): Contact? =
        contactDao.getByPubKey(pubKey)?.toDomain()

    suspend fun deleteContact(pubKey: ByteArray) {
        contactDao.getByPubKey(pubKey)?.let { contactDao.delete(it) }
    }

    suspend fun updateLastMessageTime(pubKey: ByteArray, timestamp: Long) {
        contactDao.updateLastMessageTime(pubKey, timestamp)
    }

    private fun ContactEntity.toDomain() = Contact(
        pubKey = pubKey,
        displayName = displayName ?: PubKeyUtils.generateDisplayName(pubKey),
        avatarColor = PubKeyUtils.generateAvatarColor(pubKey),
        addedAt = addedAt,
        lastMessageAt = lastMessageAt
    )
}
