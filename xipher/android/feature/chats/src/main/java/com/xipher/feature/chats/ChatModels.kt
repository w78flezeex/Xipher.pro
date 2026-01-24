package com.xipher.feature.chats

import com.xipher.core.common.ChatItem
import com.xipher.core.network.ChatDto

fun ChatDto.toItem(): ChatItem {
    val display = displayName?.takeIf { it.isNotBlank() } ?: name
    val avatarValue = avatar?.takeIf { it.isNotBlank() } ?: display.take(1).uppercase()
    return ChatItem(
        id = id,
        title = display,
        preview = lastMessage ?: "",
        time = time ?: "",
        unread = unread,
        avatarText = avatarValue,
        avatarUrl = avatarUrl?.takeIf { it.isNotBlank() },
        isSavedMessages = isSavedMessages,
        isBot = isBot,
        isPremium = isPremium
    )
}
