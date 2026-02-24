package com.bitchat.ui.onboarding

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.bitchat.data.repository.IdentityRepository
import com.bitchat.domain.model.UserIdentity
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class OnboardingViewModel @Inject constructor(
    private val identityRepository: IdentityRepository
) : ViewModel() {

    data class UiState(
        val isLoading: Boolean = true,
        val identity: UserIdentity? = null,
        val error: String? = null
    )

    private val _uiState = MutableStateFlow(UiState())
    val uiState: StateFlow<UiState> = _uiState.asStateFlow()

    init {
        viewModelScope.launch {
            identityRepository.loadOrCreateIdentity()
                .onSuccess { identity ->
                    _uiState.value = UiState(isLoading = false, identity = identity)
                }
                .onFailure { e ->
                    _uiState.value = UiState(isLoading = false, error = e.message)
                }
        }
    }
}
