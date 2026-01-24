package com.xipher.feature.settings

import android.os.Build
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.xipher.core.ui.XPrimaryButton
import com.xipher.core.ui.XTextInput

@Composable
fun SettingsScreenRoute(
    onBack: () -> Unit,
    viewModel: SettingsViewModel = hiltViewModel()
) {
    val state by viewModel.state.collectAsState()
    LaunchedEffect(Unit) {
        viewModel.refreshSessions()
    }
    SettingsScreen(
        state = state,
        onBack = onBack,
        onBaseUrlChange = viewModel::updateBaseUrl,
        onThemeChange = viewModel::updateTheme,
        onLanguageChange = viewModel::updateLanguage,
        onRefreshSessions = viewModel::refreshSessions,
        onRevokeSession = viewModel::revokeSession,
        onRevokeOtherSessions = viewModel::revokeOtherSessions,
        onLogout = viewModel::logout
    )
}

@Composable
fun SettingsScreen(
    state: SettingsState,
    onBack: () -> Unit,
    onBaseUrlChange: (String) -> Unit,
    onThemeChange: (String) -> Unit,
    onLanguageChange: (String) -> Unit,
    onRefreshSessions: () -> Unit,
    onRevokeSession: (String) -> Unit,
    onRevokeOtherSessions: () -> Unit,
    onLogout: () -> Unit
) {
    val deviceLabel = remember {
        val manufacturer = Build.MANUFACTURER.orEmpty().trim()
        val model = Build.MODEL.orEmpty().trim()
        listOf(manufacturer, model).filter { it.isNotBlank() }.joinToString(" ")
            .ifBlank { "Android" }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(text = "Settings", style = MaterialTheme.typography.titleMedium)
            TextButton(onClick = onBack) { Text(text = "Close") }
        }

        Text(text = "Base URL", style = MaterialTheme.typography.labelMedium)
        XTextInput(
            value = state.baseUrl,
            onValueChange = onBaseUrlChange,
            placeholder = "https://xipher.pro",
            modifier = Modifier.fillMaxWidth()
        )

        Text(text = "Theme", style = MaterialTheme.typography.labelMedium)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(onClick = { onThemeChange("dark") }) { Text(text = "Dark") }
            TextButton(onClick = { onThemeChange("light") }) { Text(text = "Light") }
            TextButton(onClick = { onThemeChange("system") }) { Text(text = "System") }
        }

        Text(text = "Language", style = MaterialTheme.typography.labelMedium)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TextButton(onClick = { onLanguageChange("ru") }) { Text(text = "RU") }
            TextButton(onClick = { onLanguageChange("en") }) { Text(text = "EN") }
        }

        Spacer(modifier = Modifier.height(8.dp))

        Text(text = "Active sessions", style = MaterialTheme.typography.labelMedium)
        Text(text = "Current session (this device)", style = MaterialTheme.typography.bodySmall)
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(text = "Device", style = MaterialTheme.typography.bodySmall)
            Text(text = deviceLabel, style = MaterialTheme.typography.bodySmall)
        }
        Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
            Text(text = "Last activity", style = MaterialTheme.typography.bodySmall)
            Text(text = "now", style = MaterialTheme.typography.bodySmall)
        }

        Spacer(modifier = Modifier.height(6.dp))
        Text(text = "Other devices", style = MaterialTheme.typography.labelMedium)
        if (state.sessionsLoading) {
            Text(text = "Loading...", style = MaterialTheme.typography.bodySmall)
        } else if (state.sessionsError != null) {
            Text(
                text = state.sessionsError ?: "Failed to load sessions",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.error
            )
            TextButton(onClick = onRefreshSessions) { Text(text = "Retry") }
        } else if (state.sessions.isEmpty()) {
            Text(text = "No other sessions", style = MaterialTheme.typography.bodySmall)
        } else {
            state.sessions.forEach { session ->
                SessionRow(
                    session = session,
                    isRevoking = state.revokingSessionIds.contains(session.sessionId),
                    onRevoke = onRevokeSession
                )
            }
        }

        XPrimaryButton(
            text = "Terminate all others",
            onClick = onRevokeOtherSessions,
            modifier = Modifier.fillMaxWidth(),
            enabled = !state.revokingAll && state.sessions.isNotEmpty()
        )

        Spacer(modifier = Modifier.height(12.dp))
        XPrimaryButton(text = "Sign out", onClick = onLogout, modifier = Modifier.fillMaxWidth())
    }
}

@Composable
private fun SessionRow(
    session: SessionItem,
    isRevoking: Boolean,
    onRevoke: (String) -> Unit
) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Column(modifier = Modifier.weight(1f)) {
            Text(text = sessionTitle(session), style = MaterialTheme.typography.bodyMedium)
            Text(text = sessionSubtitle(session), style = MaterialTheme.typography.bodySmall)
        }
        TextButton(onClick = { onRevoke(session.sessionId) }, enabled = !isRevoking) {
            Text(text = "Terminate")
        }
    }
}

private fun sessionTitle(session: SessionItem): String {
    val ua = session.userAgent.lowercase()
    val browser = when {
        ua.contains("edg/") -> "Edge"
        ua.contains("chrome") && !ua.contains("edg/") -> "Chrome"
        ua.contains("safari") && !ua.contains("chrome") -> "Safari"
        ua.contains("firefox") -> "Firefox"
        ua.contains("opr/") || ua.contains("opera") -> "Opera"
        else -> ""
    }
    val os = when {
        ua.contains("android") || ua.contains("okhttp") -> "Android"
        ua.contains("windows") -> "Windows"
        ua.contains("mac os") || ua.contains("macintosh") -> "macOS"
        ua.contains("iphone") || ua.contains("ipad") || ua.contains("ipod") -> "iOS"
        ua.contains("linux") -> "Linux"
        else -> ""
    }
    val parts = listOf(browser, os).filter { it.isNotBlank() }
    if (parts.isNotEmpty()) {
        return parts.joinToString(" - ")
    }
    val shortId = session.sessionId.take(6)
    return if (shortId.isNotBlank()) "Session $shortId" else "Session"
}

private fun sessionSubtitle(session: SessionItem): String {
    val value = session.lastSeen.ifBlank { session.createdAt }
    val label = if (value.isNotBlank()) value else "unknown"
    return "Last activity: $label"
}
