package com.xipher.feature.settings

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.xipher.core.common.ApiResult
import com.xipher.core.network.ApiClient
import com.xipher.core.network.SessionInfoDto
import com.xipher.core.network.NetworkConfig
import com.xipher.core.storage.SessionRepository
import com.xipher.core.storage.SettingsStore
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class SettingsViewModel @Inject constructor(
    private val settingsStore: SettingsStore,
    private val networkConfig: NetworkConfig,
    private val sessionRepository: SessionRepository,
    private val apiClient: ApiClient
) : ViewModel() {
    private val _state = MutableStateFlow(SettingsState())
    val state: StateFlow<SettingsState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            settingsStore.baseUrl.collect { value ->
                _state.update { it.copy(baseUrl = value) }
                networkConfig.updateBaseUrl(value)
            }
        }
        viewModelScope.launch {
            settingsStore.theme.collect { value ->
                _state.update { it.copy(theme = value) }
            }
        }
        viewModelScope.launch {
            settingsStore.language.collect { value ->
                _state.update { it.copy(language = value) }
            }
        }
    }

    fun updateBaseUrl(value: String) {
        val trimmed = value.trim()
        _state.update { it.copy(baseUrl = trimmed) }
        viewModelScope.launch {
            settingsStore.setBaseUrl(trimmed)
            networkConfig.updateBaseUrl(trimmed)
        }
    }

    fun updateTheme(value: String) {
        _state.update { it.copy(theme = value) }
        viewModelScope.launch {
            settingsStore.setTheme(value)
        }
    }

    fun updateLanguage(value: String) {
        _state.update { it.copy(language = value) }
        viewModelScope.launch {
            settingsStore.setLanguage(value)
        }
    }

    fun refreshSessions() {
        val token = sessionRepository.session.value.token
        if (token.isBlank()) {
            _state.update {
                it.copy(
                    sessions = emptyList(),
                    sessionsLoading = false,
                    sessionsError = "Not authorized"
                )
            }
            return
        }
        viewModelScope.launch {
            _state.update { it.copy(sessionsLoading = true, sessionsError = null) }
            when (val result = apiClient.fetchSessions(token)) {
                is ApiResult.Success -> {
                    _state.update {
                        it.copy(
                            sessions = result.data.map { dto -> dto.toSessionItem() },
                            sessionsLoading = false,
                            sessionsError = null
                        )
                    }
                }
                is ApiResult.Error -> {
                    _state.update {
                        it.copy(
                            sessionsLoading = false,
                            sessionsError = result.error.userMessage ?: "Failed to load sessions"
                        )
                    }
                }
            }
        }
    }

    fun revokeSession(sessionId: String) {
        val token = sessionRepository.session.value.token
        if (token.isBlank()) {
            _state.update { it.copy(sessionsError = "Not authorized") }
            return
        }
        _state.update { it.copy(revokingSessionIds = it.revokingSessionIds + sessionId) }
        viewModelScope.launch {
            when (val result = apiClient.revokeSession(token, sessionId)) {
                is ApiResult.Success -> refreshSessions()
                is ApiResult.Error -> _state.update {
                    it.copy(sessionsError = result.error.userMessage ?: "Failed to revoke session")
                }
            }
            _state.update { it.copy(revokingSessionIds = it.revokingSessionIds - sessionId) }
        }
    }

    fun revokeOtherSessions() {
        val token = sessionRepository.session.value.token
        if (token.isBlank()) {
            _state.update { it.copy(sessionsError = "Not authorized") }
            return
        }
        _state.update { it.copy(revokingAll = true) }
        viewModelScope.launch {
            when (val result = apiClient.revokeOtherSessions(token)) {
                is ApiResult.Success -> refreshSessions()
                is ApiResult.Error -> _state.update {
                    it.copy(sessionsError = result.error.userMessage ?: "Failed to revoke sessions")
                }
            }
            _state.update { it.copy(revokingAll = false) }
        }
    }

    fun logout() {
        sessionRepository.clear()
    }
}

data class SettingsState(
    val baseUrl: String = "",
    val theme: String = "dark",
    val language: String = "en",
    val sessions: List<SessionItem> = emptyList(),
    val sessionsLoading: Boolean = false,
    val sessionsError: String? = null,
    val revokingAll: Boolean = false,
    val revokingSessionIds: Set<String> = emptySet()
)

data class SessionItem(
    val sessionId: String,
    val createdAt: String,
    val lastSeen: String,
    val userAgent: String
)

private fun SessionInfoDto.toSessionItem(): SessionItem = SessionItem(
    sessionId = sessionId,
    createdAt = createdAt,
    lastSeen = lastSeen,
    userAgent = userAgent
)
