package com.xipher.core.ui

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color

private val DarkColors = darkColorScheme(
    primary = XipherColors.DarkAccent,
    onPrimary = XipherColors.DarkTextPrimary,
    secondary = XipherColors.DarkAccentStrong,
    tertiary = XipherColors.DarkAccentSoft,
    background = XipherColors.DarkBgPrimary,
    surface = XipherColors.DarkBgSecondary,
    surfaceVariant = XipherColors.DarkBgTertiary,
    outline = XipherColors.DarkGlassBorder,
    error = XipherColors.DarkError
)

private val LightColors = lightColorScheme(
    primary = XipherColors.LightAccent,
    onPrimary = XipherColors.LightTextPrimary,
    secondary = XipherColors.LightAccentStrong,
    tertiary = XipherColors.LightAccentSoft,
    background = XipherColors.LightBgPrimary,
    surface = XipherColors.LightBgSecondary,
    surfaceVariant = XipherColors.LightBgTertiary,
    outline = XipherColors.LightGlassBorder,
    error = XipherColors.LightError
)

val LocalXipherGradients = staticCompositionLocalOf {
    XipherGradients(
        primary = Brush.linearGradient(listOf(Color(0xFF9D00FF), Color(0xFF7B25FF)))
    )
}

@Composable
fun XipherTheme(
    darkTheme: Boolean,
    content: @Composable () -> Unit
) {
    val colors = if (darkTheme) DarkColors else LightColors
    MaterialTheme(
        colorScheme = colors,
        typography = XipherTypography,
        shapes = XipherShapes,
        content = content
    )
}

data class XipherGradients(
    val primary: Brush
)
