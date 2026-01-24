package com.xipher.feature.auth

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.xipher.core.common.ApiResult
import com.xipher.core.network.ApiClient
import com.xipher.core.storage.SessionRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class AuthViewModel @Inject constructor(
    private val apiClient: ApiClient,
    private val sessionRepository: SessionRepository
) : ViewModel() {
    private val _state = MutableStateFlow(AuthState())
    val state: StateFlow<AuthState> = _state.asStateFlow()

    fun onUsernameChanged(value: String) {
        _state.update { it.copy(username = value, error = null) }
    }

    fun onPasswordChanged(value: String) {
        _state.update { it.copy(password = value, error = null) }
    }

    fun login() {
        val username = state.value.username.trim()
        val password = state.value.password
        if (username.isEmpty() || password.isEmpty()) {
            _state.update { it.copy(error = "Missing credentials") }
            return
        }
        _state.update { it.copy(isLoading = true, error = null) }
        viewModelScope.launch {
            when (val result = apiClient.login(username, password)) {
                is ApiResult.Success -> {
                    sessionRepository.updateSession(
                        token = result.data.token,
                        userId = result.data.userId,
                        username = result.data.username
                    )
                    _state.update { it.copy(isLoading = false) }
                }
                is ApiResult.Error -> {
                    _state.update { it.copy(isLoading = false, error = result.error.userMessage ?: "Login failed") }
                }
            }
        }
    }
}

data class AuthState(
    val username: String = "",
    val password: String = "",
    val isLoading: Boolean = false,
    val error: String? = null
)
