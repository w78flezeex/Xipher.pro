package com.xipher.android

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.xipher.core.common.ApiResult
import com.xipher.core.network.ApiClient
import com.xipher.core.network.NetworkConfig
import com.xipher.core.network.WsClient
import com.xipher.core.storage.SessionRepository
import com.xipher.core.storage.SessionState
import com.xipher.core.storage.SettingsStore
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class AppStateViewModel @Inject constructor(
    private val apiClient: ApiClient,
    private val sessionRepository: SessionRepository,
    private val settingsStore: SettingsStore,
    private val networkConfig: NetworkConfig,
    private val wsClient: WsClient
) : ViewModel() {
    private val _state = MutableStateFlow(AppState())
    val state: StateFlow<AppState> = _state.asStateFlow()

    private var validateJob: Job? = null
    private var lastValidatedToken: String = ""

    init {
        viewModelScope.launch {
            settingsStore.baseUrl.collect { value ->
                networkConfig.updateBaseUrl(value)
            }
        }
        viewModelScope.launch {
            settingsStore.theme.collect { value ->
                _state.update { it.copy(theme = value) }
            }
        }
        viewModelScope.launch {
            sessionRepository.session.collect { session ->
                refreshSession(session)
            }
        }
    }

    private fun refreshSession(session: SessionState) {
        validateJob?.cancel()
        if (session.token.isBlank()) {
            wsClient.disconnect()
            lastValidatedToken = ""
            _state.update { it.copy(isChecking = false, isAuthed = false, error = null) }
            return
        }
        if (session.token == lastValidatedToken && state.value.isAuthed && !state.value.isChecking) {
            return
        }
        _state.update { it.copy(isChecking = true, error = null) }
        validateJob = viewModelScope.launch {
            when (val result = apiClient.validate(session.token)) {
                is ApiResult.Success -> {
                    sessionRepository.updateSession(
                        token = session.token,
                        userId = result.data.userId,
                        username = result.data.username
                    )
                    lastValidatedToken = session.token
                    wsClient.connect(session.token)
                    _state.update { it.copy(isChecking = false, isAuthed = true) }
                }
                is ApiResult.Error -> {
                    sessionRepository.clear()
                    lastValidatedToken = ""
                    wsClient.disconnect()
                    _state.update {
                        it.copy(
                            isChecking = false,
                            isAuthed = false,
                            error = result.error.userMessage
                        )
                    }
                }
            }
        }
    }
}

data class AppState(
    val isChecking: Boolean = true,
    val isAuthed: Boolean = false,
    val theme: String = "dark",
    val error: String? = null
)
