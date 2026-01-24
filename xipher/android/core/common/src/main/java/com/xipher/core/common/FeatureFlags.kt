package com.xipher.core.common

data class FeatureFlags(
    val callsEnabled: Boolean = false,
    val botsEnabled: Boolean = false,
    val adminEnabled: Boolean = false,
    val marketplaceEnabled: Boolean = false
)
