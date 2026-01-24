package com.xipher.core.common

data class ChatItem(
    val id: String,
    val title: String,
    val preview: String,
    val time: String,
    val unread: Int,
    val avatarText: String,
    val avatarUrl: String? = null,
    val isSavedMessages: Boolean = false,
    val isBot: Boolean = false,
    val isPremium: Boolean = false
)
