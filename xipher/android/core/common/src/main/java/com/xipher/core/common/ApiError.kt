package com.xipher.core.common

data class ApiError(
    val httpStatus: Int = 0,
    val serverCode: String? = null,
    val userMessage: String? = null,
    val debugMessage: String? = null,
    val isTransient: Boolean = false
)
