package com.xipher.feature.chat

import com.xipher.core.network.MessageDto


data class MessageItem(
    val id: String,
    val tempId: String? = null,
    val senderId: String,
    val receiverId: String?,
    val content: String,
    val messageType: String,
    val fileName: String? = null,
    val filePath: String? = null,
    val fileSize: Long? = null,
    val time: String,
    val status: String? = null,
    val isOutgoing: Boolean,
    val replyToMessageId: String? = null,
    val replyToSenderName: String? = null,
    val replyToContent: String? = null
)

fun MessageDto.toItem(currentUserId: String): MessageItem {
    val outgoing = senderId == currentUserId
    val resolvedType = messageType ?: "text"
    val resolvedContent = if (resolvedType == "text") content.orEmpty() else ""
    return MessageItem(
        id = id,
        senderId = senderId,
        receiverId = receiverId,
        content = resolvedContent,
        messageType = resolvedType,
        fileName = fileName,
        filePath = filePath,
        fileSize = if (fileSize > 0) fileSize else null,
        time = time ?: "",
        status = status,
        isOutgoing = outgoing,
        replyToMessageId = replyToMessageId,
        replyToSenderName = replySenderName,
        replyToContent = replyContent
    )
}
