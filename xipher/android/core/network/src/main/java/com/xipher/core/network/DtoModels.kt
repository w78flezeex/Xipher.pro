package com.xipher.core.network

import kotlinx.serialization.KSerializer
import kotlinx.serialization.SerialName
import kotlinx.serialization.Serializable
import kotlinx.serialization.descriptors.PrimitiveKind
import kotlinx.serialization.descriptors.PrimitiveSerialDescriptor
import kotlinx.serialization.descriptors.SerialDescriptor
import kotlinx.serialization.encoding.Decoder
import kotlinx.serialization.encoding.Encoder

object FlexibleBooleanSerializer : KSerializer<Boolean> {
    override val descriptor: SerialDescriptor = PrimitiveSerialDescriptor("FlexibleBoolean", PrimitiveKind.STRING)

    override fun deserialize(decoder: Decoder): Boolean {
        val value = decoder.decodeString()
        return value.equals("true", true) || value == "1" || value.equals("yes", true)
    }

    override fun serialize(encoder: Encoder, value: Boolean) {
        encoder.encodeBoolean(value)
    }
}

object FlexibleLongSerializer : KSerializer<Long> {
    override val descriptor: SerialDescriptor = PrimitiveSerialDescriptor("FlexibleLong", PrimitiveKind.STRING)

    override fun deserialize(decoder: Decoder): Long {
        return decoder.decodeString().toLongOrNull() ?: 0L
    }

    override fun serialize(encoder: Encoder, value: Long) {
        encoder.encodeLong(value)
    }
}

@Serializable
 data class ApiEnvelope<T>(
    val success: Boolean,
    val message: String? = null,
    val data: T? = null
)

@Serializable
 data class LoginRequest(
    val username: String,
    val password: String
)

@Serializable
data class SessionRequest(
    val token: String
)

@Serializable
data class RevokeSessionRequest(
    val token: String,
    @SerialName("session_id") val sessionId: String
)

@Serializable
 data class LoginData(
    @SerialName("user_id") val userId: String,
    val token: String = "",
    val username: String,
    @Serializable(with = FlexibleBooleanSerializer::class)
    @SerialName("is_premium") val isPremium: Boolean = false,
    @SerialName("premium_plan") val premiumPlan: String? = null,
    @SerialName("premium_expires_at") val premiumExpiresAt: String? = null
)

@Serializable
data class SessionInfoDto(
    @SerialName("session_id") val sessionId: String,
    @SerialName("created_at") val createdAt: String = "",
    @SerialName("last_seen") val lastSeen: String = "",
    @SerialName("user_agent") val userAgent: String = ""
)

@Serializable
data class SessionsResponse(
    val success: Boolean,
    val message: String? = null,
    val sessions: List<SessionInfoDto> = emptyList()
)

@Serializable
 data class ChatListResponse(
    val success: Boolean,
    val message: String? = null,
    val chats: List<ChatDto> = emptyList()
)

@Serializable
 data class ChatDto(
    val id: String,
    val name: String = "",
    @SerialName("display_name") val displayName: String? = null,
    val avatar: String? = null,
    @SerialName("avatar_url") val avatarUrl: String? = null,
    @SerialName("lastMessage") val lastMessage: String? = null,
    val time: String? = null,
    val unread: Int = 0,
    @Serializable(with = FlexibleBooleanSerializer::class)
    val online: Boolean = false,
    @SerialName("last_activity") val lastActivity: String? = null,
    @SerialName("is_saved_messages") @Serializable(with = FlexibleBooleanSerializer::class) val isSavedMessages: Boolean = false,
    @SerialName("is_bot") @Serializable(with = FlexibleBooleanSerializer::class) val isBot: Boolean = false,
    @SerialName("is_premium") @Serializable(with = FlexibleBooleanSerializer::class) val isPremium: Boolean = false
)

@Serializable
 data class MessagesResponse(
    val success: Boolean,
    val message: String? = null,
    val messages: List<MessageDto> = emptyList()
)

@Serializable
 data class MessageDto(
    val id: String = "",
    @SerialName("sender_id") val senderId: String = "",
    @SerialName("receiver_id") val receiverId: String? = null,
    @Serializable(with = FlexibleBooleanSerializer::class)
    val sent: Boolean = false,
    val status: String? = null,
    @SerialName("is_read") @Serializable(with = FlexibleBooleanSerializer::class) val isRead: Boolean = false,
    @SerialName("is_delivered") @Serializable(with = FlexibleBooleanSerializer::class) val isDelivered: Boolean = false,
    val content: String? = null,
    @SerialName("message_type") val messageType: String? = "text",
    @SerialName("file_path") val filePath: String? = null,
    @SerialName("file_name") val fileName: String? = null,
    @SerialName("file_size") @Serializable(with = FlexibleLongSerializer::class) val fileSize: Long = 0L,
    @SerialName("reply_to_message_id") val replyToMessageId: String? = null,
    @SerialName("reply_content") val replyContent: String? = null,
    @SerialName("reply_sender_name") val replySenderName: String? = null,
    val time: String? = null
)

@Serializable
 data class SendMessageRequest(
    val token: String,
    @SerialName("receiver_id") val receiverId: String,
    val content: String,
    @SerialName("message_type") val messageType: String = "text",
    @SerialName("file_path") val filePath: String? = null,
    @SerialName("file_name") val fileName: String? = null,
    @SerialName("file_size") val fileSize: Long? = null,
    @SerialName("reply_to_message_id") val replyToMessageId: String? = null,
    @SerialName("temp_id") val tempId: String? = null
)

@Serializable
 data class SendMessageResponse(
    val success: Boolean,
    val message: String? = null,
    @SerialName("message_id") val messageId: String? = null,
    @SerialName("temp_id") val tempId: String? = null,
    @SerialName("created_at") val createdAt: String? = null,
    val time: String? = null,
    val status: String? = null,
    @SerialName("is_read") @Serializable(with = FlexibleBooleanSerializer::class) val isRead: Boolean = false,
    @SerialName("is_delivered") @Serializable(with = FlexibleBooleanSerializer::class) val isDelivered: Boolean = false,
    val content: String? = null,
    @SerialName("message_type") val messageType: String? = null,
    @SerialName("file_path") val filePath: String? = null,
    @SerialName("file_name") val fileName: String? = null,
    @SerialName("file_size") val fileSize: Long? = null,
    @SerialName("reply_to_message_id") val replyToMessageId: String? = null
)

@Serializable
 data class UploadFileRequest(
    val token: String,
    @SerialName("file_data") val fileData: String,
    @SerialName("file_name") val fileName: String
)

@Serializable
 data class UploadFileResponse(
    val success: Boolean,
    val message: String? = null,
    @SerialName("file_path") val filePath: String? = null,
    @SerialName("file_name") val fileName: String? = null,
    @SerialName("file_size") val fileSize: Long? = null
)
