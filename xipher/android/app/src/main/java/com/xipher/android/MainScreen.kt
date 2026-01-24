package com.xipher.android

import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.xipher.core.common.ChatItem
import com.xipher.feature.chat.ChatScreenRoute
import com.xipher.feature.chat.ChatViewModel
import com.xipher.feature.chats.ChatsPanel
import com.xipher.feature.chats.ChatsViewModel

@Composable
fun MainScreen(
    onOpenSettings: () -> Unit,
    modifier: Modifier = Modifier
) {
    val chatsViewModel: ChatsViewModel = hiltViewModel()
    val chatViewModel: ChatViewModel = hiltViewModel()
    val chatsState by chatsViewModel.state.collectAsState()
    val chatState by chatViewModel.state.collectAsState()

    BoxWithConstraints(modifier = modifier.fillMaxSize()) {
        val isCompact = maxWidth < 600.dp
        val selectChat: (ChatItem) -> Unit = { chat ->
            chatsViewModel.selectChat(chat.id)
            chatViewModel.selectChat(chat)
        }
        if (isCompact) {
            if (chatState.currentChat == null) {
                ChatsPanel(
                    state = chatsState,
                    onQueryChange = chatsViewModel::updateQuery,
                    onSelectChat = selectChat,
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                ChatScreenRoute(
                    viewModel = chatViewModel,
                    isCompact = true,
                    onBack = {
                        chatViewModel.clearChat()
                        chatsViewModel.clearSelection()
                    },
                    onOpenSettings = onOpenSettings
                )
            }
        } else {
            Row(modifier = Modifier.fillMaxSize()) {
                ChatsPanel(
                    state = chatsState,
                    onQueryChange = chatsViewModel::updateQuery,
                    onSelectChat = selectChat,
                    modifier = Modifier.width(320.dp)
                )
                ChatScreenRoute(
                    viewModel = chatViewModel,
                    isCompact = false,
                    onBack = {},
                    onOpenSettings = onOpenSettings
                )
            }
        }
    }
}
