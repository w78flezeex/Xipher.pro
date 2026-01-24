package com.xipher.feature.chats

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.xipher.core.common.ApiResult
import com.xipher.core.common.ChatItem
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
class ChatsViewModel @Inject constructor(
    private val apiClient: ApiClient,
    private val sessionRepository: SessionRepository
) : ViewModel() {
    private val _state = MutableStateFlow(ChatsState())
    val state: StateFlow<ChatsState> = _state.asStateFlow()

    private var lastToken: String = ""

    init {
        viewModelScope.launch {
            sessionRepository.session.collect { session ->
                if (session.token != lastToken) {
                    lastToken = session.token
                    if (session.token.isNotBlank()) {
                        refresh()
                    } else {
                        _state.update { it.copy(chats = emptyList(), selectedChatId = null) }
                    }
                }
            }
        }
    }

    fun updateQuery(value: String) {
        _state.update { it.copy(query = value) }
    }

    fun selectChat(chatId: String) {
        _state.update { it.copy(selectedChatId = chatId) }
    }

    fun clearSelection() {
        _state.update { it.copy(selectedChatId = null) }
    }

    fun refresh() {
        val token = sessionRepository.session.value.token
        if (token.isBlank()) return
        _state.update { it.copy(isLoading = true, error = null) }
        viewModelScope.launch {
            when (val result = apiClient.fetchChats(token)) {
                is ApiResult.Success -> {
                    _state.update {
                        it.copy(
                            chats = result.data.map { dto -> dto.toItem() },
                            isLoading = false
                        )
                    }
                }
                is ApiResult.Error -> {
                    _state.update { it.copy(isLoading = false, error = result.error.userMessage) }
                }
            }
        }
    }
}

data class ChatsState(
    val chats: List<ChatItem> = emptyList(),
    val isLoading: Boolean = false,
    val error: String? = null,
    val query: String = "",
    val selectedChatId: String? = null
)
