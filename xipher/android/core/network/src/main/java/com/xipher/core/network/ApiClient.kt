package com.xipher.core.network

import com.xipher.core.common.ApiError
import com.xipher.core.common.ApiResult
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.withContext
import retrofit2.HttpException
import java.io.IOException

class ApiClient(
    private val service: ApiService,
    private val ioDispatcher: CoroutineDispatcher
) {
    suspend fun login(username: String, password: String): ApiResult<LoginData> =
        safeCall { service.login(LoginRequest(username, password)) }

    suspend fun validate(token: String): ApiResult<LoginData> =
        safeCall { service.validateToken(SessionRequest(token)) }

    suspend fun fetchSessions(token: String): ApiResult<List<SessionInfoDto>> =
        safeCallSessions { service.getSessions(SessionRequest(token)) }

    suspend fun revokeSession(token: String, sessionId: String): ApiResult<Unit> =
        safeCallStatus { service.revokeSession(RevokeSessionRequest(token, sessionId)) }

    suspend fun revokeOtherSessions(token: String): ApiResult<Unit> =
        safeCallStatus { service.revokeOtherSessions(SessionRequest(token)) }

    suspend fun fetchChats(token: String): ApiResult<List<ChatDto>> =
        safeCallChats { service.fetchChats(SessionRequest(token)) }

    suspend fun fetchMessages(token: String, friendId: String): ApiResult<List<MessageDto>> =
        safeCallMessages { service.fetchMessages(mapOf("token" to token, "friend_id" to friendId)) }

    suspend fun sendMessage(request: SendMessageRequest): ApiResult<SendMessageResponse> =
        safeCallSend { service.sendMessage(request) }

    suspend fun uploadFile(token: String, fileName: String, base64: String): ApiResult<UploadFileResponse> =
        safeCallUpload { service.uploadFile(UploadFileRequest(token, base64, fileName)) }

    private suspend fun <T> safeCall(block: suspend () -> ApiEnvelope<T>): ApiResult<T> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success && response.data != null) {
                    ApiResult.Success(response.data)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message ?: "Request failed"))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun safeCallChats(block: suspend () -> ChatListResponse): ApiResult<List<ChatDto>> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success) {
                    ApiResult.Success(response.chats)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun safeCallMessages(block: suspend () -> MessagesResponse): ApiResult<List<MessageDto>> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success) {
                    ApiResult.Success(response.messages)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun safeCallSessions(block: suspend () -> SessionsResponse): ApiResult<List<SessionInfoDto>> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success) {
                    ApiResult.Success(response.sessions)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun safeCallStatus(block: suspend () -> ApiEnvelope<Map<String, String>>): ApiResult<Unit> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success) {
                    ApiResult.Success(Unit)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message ?: "Request failed"))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun <T> safeCallRaw(block: suspend () -> T): ApiResult<T> {
        return withContext(ioDispatcher) {
            try {
                ApiResult.Success(block())
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun safeCallSend(block: suspend () -> SendMessageResponse): ApiResult<SendMessageResponse> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success) {
                    ApiResult.Success(response)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message ?: "Send failed"))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private suspend fun safeCallUpload(block: suspend () -> UploadFileResponse): ApiResult<UploadFileResponse> {
        return withContext(ioDispatcher) {
            try {
                val response = block()
                if (response.success) {
                    ApiResult.Success(response)
                } else {
                    ApiResult.Error(ApiError(userMessage = response.message ?: "Upload failed"))
                }
            } catch (e: Exception) {
                ApiResult.Error(buildError(e))
            }
        }
    }

    private fun buildError(e: Exception): ApiError {
        return when (e) {
            is HttpException -> ApiError(
                httpStatus = e.code(),
                userMessage = "Network error",
                debugMessage = e.message(),
                isTransient = e.code() in setOf(408, 429, 502, 503, 504)
            )
            is IOException -> ApiError(userMessage = "Network error", debugMessage = e.message, isTransient = true)
            else -> ApiError(userMessage = "Unknown error", debugMessage = e.message)
        }
    }
}
