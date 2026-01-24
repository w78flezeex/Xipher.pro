package com.xipher.core.network

import kotlinx.serialization.json.Json

val XipherJson = Json {
    ignoreUnknownKeys = true
    isLenient = true
    coerceInputValues = true
    explicitNulls = false
}
