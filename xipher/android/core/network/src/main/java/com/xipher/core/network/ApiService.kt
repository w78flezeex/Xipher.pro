package com.xipher.core.network

import retrofit2.http.Body
import retrofit2.http.POST

interface ApiService {
    @POST("/api/login")
    suspend fun login(@Body request: LoginRequest): ApiEnvelope<LoginData>

    @POST("/api/validate-token")
    suspend fun validateToken(@Body request: SessionRequest): ApiEnvelope<LoginData>

    @POST("/api/get-sessions")
    suspend fun getSessions(@Body request: SessionRequest): SessionsResponse

    @POST("/api/revoke-session")
    suspend fun revokeSession(@Body request: RevokeSessionRequest): ApiEnvelope<Map<String, String>>

    @POST("/api/revoke-other-sessions")
    suspend fun revokeOtherSessions(@Body request: SessionRequest): ApiEnvelope<Map<String, String>>

    @POST("/api/chats")
    suspend fun fetchChats(@Body request: SessionRequest): ChatListResponse

    @POST("/api/messages")
    suspend fun fetchMessages(@Body request: Map<String, String>): MessagesResponse

    @POST("/api/send-message")
    suspend fun sendMessage(@Body request: SendMessageRequest): SendMessageResponse

    @POST("/api/upload-file")
    suspend fun uploadFile(@Body request: UploadFileRequest): UploadFileResponse
}
