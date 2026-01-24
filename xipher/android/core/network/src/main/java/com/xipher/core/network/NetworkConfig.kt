package com.xipher.core.network

import okhttp3.HttpUrl.Companion.toHttpUrlOrNull

class NetworkConfig(
    baseUrl: String
) {
    var baseUrl: String = normalize(baseUrl)
        private set

    fun updateBaseUrl(value: String) {
        baseUrl = normalize(value)
    }

    fun wsUrl(): String {
        val url = baseUrl.toHttpUrlOrNull() ?: return ""
        val scheme = if (url.isHttps) "wss" else "ws"
        return url.newBuilder().scheme(scheme).encodedPath("/ws").build().toString()
    }

    private fun normalize(raw: String): String {
        var trimmed = raw.trim()
        if (trimmed.isEmpty()) return trimmed
        if (!trimmed.contains("://")) {
            trimmed = "https://$trimmed"
        }
        return trimmed.removeSuffix("/")
    }
}
