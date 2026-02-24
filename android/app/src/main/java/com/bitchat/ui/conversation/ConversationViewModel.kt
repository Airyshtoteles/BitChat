package com.bitchat.ui.conversation

import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.bitchat.data.repository.ContactRepository
import com.bitchat.data.repository.MessageRepository
import com.bitchat.domain.model.ChatMessage
import com.bitchat.domain.model.Contact
import com.bitchat.domain.util.PubKeyUtils
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class ConversationViewModel @Inject constructor(
    savedStateHandle: SavedStateHandle,
    private val messageRepository: MessageRepository,
    private val contactRepository: ContactRepository
) : ViewModel() {

    private val pubKeyHex: String = savedStateHandle["pubKeyHex"] ?: ""
    val peerPubKey: ByteArray = PubKeyUtils.fromHex(pubKeyHex)

    val messages: StateFlow<List<ChatMessage>> =
        messageRepository.getMessagesForPeer(peerPubKey)
            .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    val contact: StateFlow<Contact?> = flow {
        emit(contactRepository.getContact(peerPubKey))
    }.stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), null)

    private val _sendError = MutableSharedFlow<String>()
    val sendError: SharedFlow<String> = _sendError.asSharedFlow()

    fun sendMessage(content: String) {
        if (content.isBlank()) return
        viewModelScope.launch {
            messageRepository.sendMessage(peerPubKey, content.trim())
                .onFailure { _sendError.emit(it.message ?: "Send failed") }
        }
    }
}
