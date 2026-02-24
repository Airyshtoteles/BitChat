package com.bitchat.ui.contacts

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.bitchat.data.repository.ContactRepository
import com.bitchat.domain.model.Contact
import com.bitchat.domain.util.PubKeyUtils
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class ContactsViewModel @Inject constructor(
    private val contactRepository: ContactRepository
) : ViewModel() {

    val contacts: StateFlow<List<Contact>> =
        contactRepository.getAllContacts()
            .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    fun addContact(pubKeyHex: String, alias: String? = null): Boolean {
        if (pubKeyHex.length != 64) return false
        val pubKey = try {
            PubKeyUtils.fromHex(pubKeyHex)
        } catch (e: Exception) {
            return false
        }
        viewModelScope.launch {
            contactRepository.addContact(pubKey, alias?.takeIf { it.isNotBlank() })
        }
        return true
    }

    fun deleteContact(pubKey: ByteArray) {
        viewModelScope.launch {
            contactRepository.deleteContact(pubKey)
        }
    }
}
