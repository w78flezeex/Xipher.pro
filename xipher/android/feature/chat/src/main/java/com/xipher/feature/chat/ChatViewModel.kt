package com.xipher.feature.chat

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.xipher.core.common.ApiResult
import com.xipher.core.common.ChatItem
import com.xipher.core.network.ApiClient
import com.xipher.core.network.SendMessageRequest
import com.xipher.core.network.WsClient
import com.xipher.core.network.WsEvent
import com.xipher.core.network.XipherJson
import com.xipher.core.storage.SessionRepository
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import java.util.UUID
import javax.inject.Inject

@HiltViewModel
class ChatViewModel @Inject constructor(
    private val apiClient: ApiClient,
    private val sessionRepository: SessionRepository,
    private val wsClient: WsClient
) : ViewModel() {
    private val _state = MutableStateFlow(ChatUiState())
    val state: StateFlow<ChatUiState> = _state.asStateFlow()

    private var typingJob: Job? = null
    private var lastTypingSentAt: Long = 0L

    init {
        viewModelScope.launch {
            wsClient.events.collect { event ->
                if (event is WsEvent.Raw) {
                    handleWsPayload(event.payload)
                }
            }
        }
        viewModelScope.launch {
            sessionRepository.session.collect { session ->
                if (session.token.isBlank()) {
                    clearChat()
                }
            }
        }
    }

    fun selectChat(chat: ChatItem) {
        _state.update { it.copy(currentChat = chat, messages = emptyList(), error = null, typingUsers = emptySet()) }
        loadMessages(chat)
    }

    fun clearChat() {
        _state.update { it.copy(currentChat = null, messages = emptyList(), input = "", error = null, typingUsers = emptySet()) }
    }

    fun onMessageChanged(value: String) {
        _state.update { it.copy(input = value, error = null) }
        val currentChat = state.value.currentChat ?: return
        val now = System.currentTimeMillis()
        if (value.isNotBlank() && now - lastTypingSentAt > 2000) {
            sendTyping(currentChat, true)
            lastTypingSentAt = now
        }
        typingJob?.cancel()
        typingJob = viewModelScope.launch {
            delay(1500)
            if (state.value.input.isBlank()) {
                sendTyping(currentChat, false)
            }
        }
    }

    fun selectReplyTo(message: MessageItem) {
        _state.update { it.copy(replyingTo = message) }
    }

    fun cancelReply() {
        _state.update { it.copy(replyingTo = null) }
    }

    fun sendMessage() {
        val currentChat = state.value.currentChat ?: return
        val content = state.value.input.trim()
        if (content.isBlank()) return
        val tempId = UUID.randomUUID().toString()
        val session = sessionRepository.session.value
        if (session.token.isBlank()) {
            _state.update { it.copy(error = "Not authenticated") }
            return
        }
        val replyingTo = state.value.replyingTo
        val tempMessage = MessageItem(
            id = tempId,
            tempId = tempId,
            senderId = session.userId,
            receiverId = currentChat.id,
            content = content,
            messageType = "text",
            time = "",
            status = "sending",
            isOutgoing = true,
            replyToMessageId = replyingTo?.id,
            replyToSenderName = if (replyingTo?.isOutgoing == true) "You" else currentChat.title,
            replyToContent = replyingTo?.content
        )
        _state.update { it.copy(messages = it.messages + tempMessage, input = "", replyingTo = null) }
        viewModelScope.launch {
            val request = SendMessageRequest(
                token = session.token,
                receiverId = currentChat.id,
                content = content,
                tempId = tempId,
                replyToMessageId = replyingTo?.id
            )
            isOutgoing = true
        )
        _state.update { it.copy(messages = it.messages + tempMessage, input = "") }
        viewModelScope.launch {
            val request = SendMessageRequest(
                token = session.token,
                receiverId = currentChat.id,
                content = content,
                tempId = tempId
            )
            when (val result = apiClient.sendMessage(request)) {
                is ApiResult.Success -> {
                    reconcileTempMessage(tempId, result.data)
                }
                is ApiResult.Error -> {
                    markTempFailed(tempId, result.error.userMessage)
                }
            }
        }
    }

    fun sendAttachment(fileName: String, base64: String, fileSize: Long) {
        val currentChat = state.value.currentChat ?: return
        val session = sessionRepository.session.value
        if (session.token.isBlank()) {
            _state.update { it.copy(error = "Not authenticated") }
            return
        }
        val tempId = UUID.randomUUID().toString()
        val tempMessage = MessageItem(
            id = tempId,
            tempId = tempId,
            senderId = session.userId,
            receiverId = currentChat.id,
            content = "",
            messageType = "file",
            fileName = fileName,
            fileSize = fileSize,
            time = "",
            status = "uploading",
            isOutgoing = true
        )
        _state.update { it.copy(messages = it.messages + tempMessage) }
        viewModelScope.launch {
            when (val upload = apiClient.uploadFile(session.token, fileName, base64)) {
                is ApiResult.Success -> {
                    val response = upload.data
                    val request = SendMessageRequest(
                        token = session.token,
                        receiverId = currentChat.id,
                        content = fileName,
                        messageType = "file",
                        filePath = response.filePath,
                        fileName = response.fileName ?: fileName,
                        fileSize = response.fileSize ?: fileSize,
                        tempId = tempId
                    )
                    when (val sendResult = apiClient.sendMessage(request)) {
                        is ApiResult.Success -> reconcileTempMessage(tempId, sendResult.data)
                        is ApiResult.Error -> markTempFailed(tempId, sendResult.error.userMessage)
                    }
                }
                is ApiResult.Error -> {
                    markTempFailed(tempId, upload.error.userMessage)
                }
            }
        }
    }

    fun setError(message: String) {
        _state.update { it.copy(error = message) }
    }

    private fun loadMessages(chat: ChatItem) {
        val token = sessionRepository.session.value.token
        if (token.isBlank()) return
        _state.update { it.copy(isLoading = true, error = null) }
        viewModelScope.launch {
            when (val result = apiClient.fetchMessages(token, chat.id)) {
                is ApiResult.Success -> {
                    val userId = sessionRepository.session.value.userId
                    _state.update {
                        it.copy(
                            messages = result.data.map { dto -> dto.toItem(userId) },
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

    private fun reconcileTempMessage(tempId: String, response: com.xipher.core.network.SendMessageResponse) {
        _state.update { state ->
            state.copy(
                messages = state.messages.map { msg ->
                    if (msg.tempId == tempId || msg.id == tempId) {
                        val resolvedType = response.messageType ?: msg.messageType
                        val resolvedContent = if (resolvedType == "text") {
                            response.content ?: msg.content
                        } else {
                            msg.content
                        }
                        msg.copy(
                            id = response.messageId ?: msg.id,
                            time = response.time ?: msg.time,
                            status = response.status ?: "sent",
                            content = resolvedContent,
                            messageType = resolvedType,
                            filePath = response.filePath ?: msg.filePath,
                            fileName = response.fileName ?: msg.fileName,
                            fileSize = response.fileSize ?: msg.fileSize
                        )
                    } else {
                        msg
                    }
                }
            )
        }
    }

    private fun markTempFailed(tempId: String, reason: String?) {
        _state.update { state ->
            state.copy(
                messages = state.messages.map { msg ->
                    if (msg.tempId == tempId || msg.id == tempId) {
                        msg.copy(status = "failed")
                    } else {
                        msg
                    }
                },
                error = reason ?: state.error
            )
        }
    }

    private fun sendTyping(chat: ChatItem, isTyping: Boolean) {
        val session = sessionRepository.session.value
        if (session.token.isBlank()) return
        val chatType = if (chat.isSavedMessages) "saved_messages" else "chat"
        wsClient.sendTyping(session.token, chatType, chat.id, isTyping)
    }

    private fun handleWsPayload(raw: String) {
        val element = try {
            XipherJson.parseToJsonElement(raw).jsonObject
        } catch (_: Exception) {
            return
        }
        val type = element.string("type")
        when (type) {
            "new_message" -> handleNewMessage(element)
            "message_delivered", "message_read" -> handleReceipt(type, element)
            "typing" -> handleTyping(element)
        }
    }

    private fun handleNewMessage(element: JsonObject) {
        val session = sessionRepository.session.value
        if (session.userId.isBlank()) return
        val message = element["message"]?.jsonObject ?: element
        val senderId = message.string("sender_id")
        val receiverId = message.string("receiver_id")
        val chatId = message.string("chat_id").ifBlank {
            if (senderId == session.userId) receiverId else senderId
        }
        val currentChat = state.value.currentChat
        if (currentChat == null || currentChat.id != chatId) return

        val outgoing = senderId == session.userId
        val resolvedType = message.string("message_type").ifBlank { "text" }
        val resolvedContent = if (resolvedType == "text") message.string("content") else ""
        val messageId = message.string("id").ifBlank { message.string("message_id") }
        val tempId = message.string("temp_id")
        if (messageId.isNotBlank() && state.value.messages.any { it.id == messageId }) return
        if (tempId.isNotBlank()) {
            reconcileTempMessage(
                tempId,
                com.xipher.core.network.SendMessageResponse(
                    success = true,
                    messageId = messageId,
                    tempId = tempId,
                    createdAt = message.string("created_at"),
                    time = message.string("time"),
                    status = message.string("status"),
                    isRead = message.string("is_read") == "true",
                    isDelivered = message.string("is_delivered") == "true",
                    content = message.string("content"),
                    messageType = resolvedType,
                    filePath = message.string("file_path"),
                    fileName = message.string("file_name"),
                    fileSize = message.long("file_size"),
                    replyToMessageId = message.string("reply_to_message_id")
                )
            )
            return
        }
        val newItem = MessageItem(
            id = messageId,
            tempId = if (tempId.isBlank()) null else tempId,
            senderId = senderId,
            receiverId = receiverId,
            content = resolvedContent,
            messageType = resolvedType,
            fileName = message.string("file_name").ifBlank { null },
            filePath = message.string("file_path").ifBlank { null },
            fileSize = message.long("file_size").takeIf { it > 0 },
            time = message.string("time"),
            status = message.string("status").ifBlank { null },
            isOutgoing = outgoing
        )

        _state.update { it.copy(messages = it.messages + newItem) }

        if (!outgoing) {
            wsClient.sendReceipt(session.token, "message_read", newItem.id)
        }
    }

    private fun handleReceipt(type: String, element: JsonObject) {
        val messageId = element.string("message_id")
        if (messageId.isBlank()) return
        val status = if (type == "message_read") "read" else "delivered"
        _state.update { state ->
            state.copy(
                messages = state.messages.map { msg ->
                    if (msg.id == messageId) msg.copy(status = status) else msg
                }
            )
        }
    }

    private fun handleTyping(element: JsonObject) {
        val chatId = element.string("chat_id")
        val currentChat = state.value.currentChat ?: return
        if (chatId != currentChat.id) return
        val fromUser = element.string("from_username").ifBlank { "typing" }
        val isTyping = element.string("is_typing") != "0"
        _state.update { state ->
            val typing = state.typingUsers.toMutableSet()
            if (isTyping) {
                typing.add(fromUser)
            } else {
                typing.remove(fromUser)
            }
            state.copy(typingUsers = typing)
        }
    }

    private fun JsonObject.string(key: String): String {
        return this[key]?.jsonPrimitive?.contentOrNull ?: ""
    }

    private fun JsonObject.long(key: String): Long {
        val primitive = this[key]?.jsonPrimitive ?: return 0L
        return primitive.longOrNull ?: primitive.contentOrNull?.toLongOrNull() ?: 0L
    }
}

data class ChatUiState(
    val currentChat: ChatItem? = null,
    val messages: List<MessageItem> = emptyList(),
    val input: String = "",
    val isLoading: Boolean = false,
    val error: String? = null,
    val typingUsers: Set<String> = emptySet(),
    val replyingTo: MessageItem? = null
)
