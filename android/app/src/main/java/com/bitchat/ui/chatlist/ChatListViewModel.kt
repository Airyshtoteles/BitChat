package com.bitchat.ui.chatlist

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.bitchat.data.repository.ContactRepository
import com.bitchat.data.repository.MessageRepository
import com.bitchat.domain.model.Contact
import com.bitchat.domain.util.PubKeyUtils
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import javax.inject.Inject

data class ChatPreview(
    val contact: Contact,
    val lastMessage: String?,
    val lastMessageAt: Long?
)

@HiltViewModel
class ChatListViewModel @Inject constructor(
    contactRepository: ContactRepository,
    messageRepository: MessageRepository
) : ViewModel() {

    val chatPreviews: StateFlow<List<ChatPreview>> = combine(
        contactRepository.getAllContacts(),
        messageRepository.getChatPreviews()
    ) { contacts, latestMessages ->
        val messageMap = latestMessages.associateBy { PubKeyUtils.toHex(it.peerPubKey) }
        contacts.map { contact ->
            val latest = messageMap[PubKeyUtils.toHex(contact.pubKey)]
            ChatPreview(
                contact = contact,
                lastMessage = latest?.content,
                lastMessageAt = latest?.timestamp
            )
        }.sortedByDescending { it.lastMessageAt ?: it.contact.addedAt }
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())
}
