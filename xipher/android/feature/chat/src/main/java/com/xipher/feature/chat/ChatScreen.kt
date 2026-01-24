package com.xipher.feature.chat

import android.database.Cursor
import android.provider.OpenableColumns
import android.util.Base64
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.xipher.core.ui.XBanner
import com.xipher.core.ui.XMessageBubble
import com.xipher.core.ui.XPrimaryButton
import com.xipher.core.ui.XTextInput
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.ByteArrayOutputStream
import java.io.InputStream

private const val MAX_UPLOAD_BYTES = 25L * 1024 * 1024

@Composable
fun ChatScreenRoute(
    viewModel: ChatViewModel = hiltViewModel(),
    isCompact: Boolean,
    onBack: () -> Unit,
    onOpenSettings: () -> Unit
) {
    val state by viewModel.state.collectAsState()
    ChatScreen(
        state = state,
        isCompact = isCompact,
        onBack = onBack,
        onOpenSettings = onOpenSettings,
        onMessageChange = viewModel::onMessageChanged,
        onSend = viewModel::sendMessage,
        onAttachment = viewModel::sendAttachment,
        onError = viewModel::setError,
        onSelectReplyTo = viewModel::selectReplyTo,
        onCancelReply = viewModel::cancelReply
    )
}

@Composable
fun ChatScreen(
    state: ChatUiState,
    isCompact: Boolean,
    onBack: () -> Unit,
    onOpenSettings: () -> Unit,
    onMessageChange: (String) -> Unit,
    onSend: () -> Unit,
    onAttachment: (String, String, Long) -> Unit,
    onError: (String) -> Unit,
    onSelectReplyTo: (MessageItem) -> Unit = {},
    onCancelReply: () -> Unit = {}
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    val fileLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument(),
        onResult = { uri ->
            if (uri == null) return@rememberLauncherForActivityResult
            scope.launch {
                val meta = loadFileMeta(context.contentResolver.query(uri, null, null, null, null))
                val name = meta.first ?: "file"
                val size = meta.second
                if (size > MAX_UPLOAD_BYTES) {
                    onError("File too large")
                    return@launch
                }
                val bytes = readBytesWithLimit(
                    context.contentResolver.openInputStream(uri),
                    MAX_UPLOAD_BYTES.toInt()
                )
                if (bytes == null) {
                    onError("File too large")
                    return@launch
                }
                val base64 = Base64.encodeToString(bytes, Base64.NO_WRAP)
                onAttachment(name, base64, bytes.size.toLong())
            }
        }
    )

    Column(modifier = Modifier.fillMaxSize()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            if (isCompact) {
                TextButton(onClick = onBack) {
                    Text(text = "Back")
                }
            }
            Text(
                text = state.currentChat?.title ?: "Chat",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.SemiBold,
                modifier = Modifier.weight(1f)
            )
            TextButton(onClick = onOpenSettings) {
                Text(text = "Settings")
            }
        }

        if (!state.error.isNullOrBlank()) {
            XBanner(text = state.error.orEmpty())
        }

        if (state.currentChat == null) {
            Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Text(text = "Select a chat", style = MaterialTheme.typography.bodyMedium)
            }
            return
        }

        if (state.isLoading && state.messages.isEmpty()) {
            Box(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                contentAlignment = Alignment.Center
            ) {
                Text(text = "Loading...", style = MaterialTheme.typography.bodyMedium)
            }
        } else {
            LazyColumn(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .padding(horizontal = 12.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(state.messages) { message ->
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = if (message.isOutgoing) Arrangement.End else Arrangement.Start
                    ) {
                        XMessageBubble(
                            text = message.content,
                            isOutgoing = message.isOutgoing,
                            attachmentName = message.fileName,
                            status = message.status,
                            replyToSenderName = message.replyToSenderName,
                            replyToContent = message.replyToContent,
                            modifier = Modifier
                                .fillMaxWidth(0.8f)
                                .combinedClickable(
                                    onClick = {},
                                    onLongClick = { onSelectReplyTo(message) }
                                )
                        )
                    }
                }
            }
        }

        if (state.replyingTo != null) {
            Surface(
                modifier = Modifier.fillMaxWidth(),
                color = MaterialTheme.colorScheme.surfaceVariant,
                tonalElevation = 2.dp
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(12.dp),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Row(
                        modifier = Modifier.weight(1f),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .width(3.dp)
                                .height(40.dp)
                                .background(Color(0xFF9D00FF))
                        )
                        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                            Text(
                                text = if (state.replyingTo.isOutgoing) "You" else (state.currentChat?.title ?: ""),
                                style = MaterialTheme.typography.labelMedium,
                                fontWeight = FontWeight.SemiBold,
                                color = Color(0xFF9D00FF),
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                            Text(
                                text = state.replyingTo.content,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis
                            )
                        }
                    }
                    IconButton(onClick = onCancelReply) {
                        Text("✕", style = MaterialTheme.typography.titleMedium)
                    }
                }
            }
        }

        if (state.typingUsers.isNotEmpty()) {
            Text(
                text = "${state.typingUsers.joinToString()} typing...",
                style = MaterialTheme.typography.labelMedium,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 4.dp)
            )
        }

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            IconButton(onClick = { fileLauncher.launch(arrayOf("*/*")) }) {
                Box(
                    modifier = Modifier
                        .size(36.dp)
                        .background(MaterialTheme.colorScheme.surfaceVariant, MaterialTheme.shapes.small),
                    contentAlignment = Alignment.Center
                ) {
                    Text(text = "+", color = MaterialTheme.colorScheme.primary)
                }
            }
            Spacer(modifier = Modifier.size(8.dp))
            XTextInput(
                value = state.input,
                onValueChange = onMessageChange,
                placeholder = "Message",
                modifier = Modifier.weight(1f)
            )
            Spacer(modifier = Modifier.size(8.dp))
            XPrimaryButton(
                text = "Send",
                onClick = onSend,
                modifier = Modifier.height(48.dp),
                enabled = state.input.isNotBlank()
            )
        }
    }
}

private fun loadFileMeta(cursor: Cursor?): Pair<String?, Long> {
    if (cursor == null) return null to -1L
    return cursor.use {
        val nameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        val sizeIndex = cursor.getColumnIndex(OpenableColumns.SIZE)
        if (!cursor.moveToFirst()) return@use null to -1L
        val name = if (nameIndex >= 0) cursor.getString(nameIndex) else null
        val size = if (sizeIndex >= 0) cursor.getLong(sizeIndex) else -1L
        name to size
    }
}

private suspend fun readBytesWithLimit(input: InputStream?, limit: Int): ByteArray? {
    if (input == null) return null
    return withContext(Dispatchers.IO) {
        input.use { stream ->
            val output = ByteArrayOutputStream()
            val buffer = ByteArray(8 * 1024)
            var total = 0
            var read: Int
            while (stream.read(buffer).also { read = it } != -1) {
                total += read
                if (total > limit) return@withContext null
                output.write(buffer, 0, read)
            }
            output.toByteArray()
        }
    }
}
