package com.xipher.core.network

import com.xipher.core.common.Logger
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.serialization.json.jsonObject
import kotlinx.serialization.json.jsonPrimitive
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import java.util.concurrent.TimeUnit
import javax.inject.Inject
import javax.inject.Singleton

sealed class WsEvent {
    data class AuthSuccess(val userId: String) : WsEvent()
    data class AuthError(val error: String) : WsEvent()
    data class Raw(val payload: String) : WsEvent()
}

@Singleton
class WsClient @Inject constructor(
    private val okHttpClient: OkHttpClient,
    private val config: NetworkConfig
) {
    private var socket: WebSocket? = null

    private val _events = MutableSharedFlow<WsEvent>(
        extraBufferCapacity = 64,
        onBufferOverflow = BufferOverflow.DROP_OLDEST
    )
    val events: SharedFlow<WsEvent> = _events

    fun connect(token: String, pushToken: String? = null, platform: String = "android") {
        val wsUrl = config.wsUrl()
        if (wsUrl.isBlank()) return
        socket?.close(1000, "reconnect")
        val request = Request.Builder().url(wsUrl).build()
        socket = okHttpClient.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                sendAuth(token, pushToken, platform)
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                val type = parseType(text)
                when (type) {
                    "auth_success" -> _events.tryEmit(WsEvent.AuthSuccess(parseUserId(text)))
                    "auth_error" -> _events.tryEmit(WsEvent.AuthError(parseError(text)))
                    else -> _events.tryEmit(WsEvent.Raw(text))
                }
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                Logger.warn("WS failure: ${t.message}")
            }
        })
    }

    fun disconnect() {
        socket?.close(1000, "bye")
        socket = null
    }

    fun sendTyping(token: String, chatType: String, chatId: String, isTyping: Boolean) {
        val payload = """{"type":"typing","token":"$token","chat_type":"$chatType","chat_id":"$chatId","is_typing":"${if (isTyping) "1" else "0"}"}"""
        socket?.send(payload)
    }

    fun sendReceipt(token: String, type: String, messageId: String) {
        val payload = """{"type":"$type","token":"$token","message_id":"$messageId"}"""
        socket?.send(payload)
    }

    private fun sendAuth(token: String, pushToken: String?, platform: String) {
        val push = if (!pushToken.isNullOrBlank()) "," + "\"push_token\":\"$pushToken\",\"platform\":\"$platform\"" else ""
        val payload = """{"type":"auth","token":"$token"$push}"""
        socket?.send(payload)
    }

    private fun parseType(raw: String): String {
        return try {
            XipherJson.parseToJsonElement(raw).jsonObject["type"]?.jsonPrimitive?.content ?: ""
        } catch (_: Exception) {
            ""
        }
    }

    private fun parseUserId(raw: String): String {
        return try {
            XipherJson.parseToJsonElement(raw).jsonObject["user_id"]?.jsonPrimitive?.content ?: ""
        } catch (_: Exception) {
            ""
        }
    }

    private fun parseError(raw: String): String {
        return try {
            XipherJson.parseToJsonElement(raw).jsonObject["error"]?.jsonPrimitive?.content ?: ""
        } catch (_: Exception) {
            ""
        }
    }
}
