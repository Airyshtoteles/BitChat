package com.bitchat.ui.profile

import androidx.lifecycle.ViewModel
import com.bitchat.data.repository.IdentityRepository
import com.bitchat.domain.model.UserIdentity
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.StateFlow
import javax.inject.Inject

@HiltViewModel
class ProfileViewModel @Inject constructor(
    identityRepository: IdentityRepository
) : ViewModel() {
    val identity: StateFlow<UserIdentity?> = identityRepository.identity
}
