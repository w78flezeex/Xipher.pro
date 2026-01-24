package com.xipher.core.storage

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import javax.inject.Inject
import javax.inject.Singleton

@Singleton
class SessionRepository @Inject constructor(
    private val tokenStore: SecureTokenStore
) {
    private val _session = MutableStateFlow(
        SessionState(
            token = tokenStore.getToken(),
            userId = tokenStore.getUserId(),
            username = tokenStore.getUsername()
        )
    )
    val session: StateFlow<SessionState> = _session.asStateFlow()

    fun updateSession(token: String, userId: String, username: String) {
        tokenStore.setSession(token, userId, username)
        _session.value = SessionState(token = token, userId = userId, username = username)
    }

    fun clear() {
        tokenStore.clear()
        _session.value = SessionState()
    }
}

data class SessionState(
    val token: String = "",
    val userId: String = "",
    val username: String = ""
)
