package com.xipher.feature.chats

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import com.xipher.core.common.ChatItem
import com.xipher.core.ui.XBanner
import com.xipher.core.ui.XChatRow
import com.xipher.core.ui.XTextInput

@Composable
fun ChatsPanel(
    state: ChatsState,
    onQueryChange: (String) -> Unit,
    onSelectChat: (ChatItem) -> Unit,
    modifier: Modifier = Modifier
) {
    val filtered = if (state.query.isBlank()) {
        state.chats
    } else {
        state.chats.filter { it.title.contains(state.query, ignoreCase = true) }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(12.dp)
    ) {
        XTextInput(
            value = state.query,
            onValueChange = onQueryChange,
            placeholder = "Search",
            modifier = Modifier.fillMaxWidth()
        )
        Spacer(modifier = Modifier.height(12.dp))
        if (!state.error.isNullOrBlank()) {
            XBanner(text = state.error.orEmpty())
            Spacer(modifier = Modifier.height(8.dp))
        }
        if (state.isLoading) {
            Text(text = "Loading...", style = MaterialTheme.typography.bodyMedium)
        } else if (filtered.isEmpty()) {
            Text(text = "No chats", style = MaterialTheme.typography.bodyMedium)
        } else {
            LazyColumn(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                items(filtered, key = { it.id }) { chat ->
                    val selected = chat.id == state.selectedChatId
                    val background = if (selected) {
                        MaterialTheme.colorScheme.surfaceVariant
                    } else {
                        Color.Transparent
                    }
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .background(background, shape = MaterialTheme.shapes.medium)
                            .clickable { onSelectChat(chat) }
                    ) {
                        XChatRow(
                            title = chat.title,
                            preview = chat.preview,
                            time = chat.time,
                            unread = chat.unread,
                            avatarText = chat.avatarText,
                            modifier = Modifier.fillMaxWidth()
                        )
                    }
                }
            }
        }
    }
}
