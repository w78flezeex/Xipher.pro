package com.xipher.android

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.hilt.navigation.compose.hiltViewModel
import com.xipher.core.ui.XipherTheme
import com.xipher.feature.auth.AuthScreen
import com.xipher.feature.settings.SettingsScreenRoute
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            val viewModel: AppStateViewModel = hiltViewModel()
            val state by viewModel.state.collectAsState()
            val darkTheme = when (state.theme) {
                "light" -> false
                "dark" -> true
                else -> isSystemInDarkTheme()
            }
            XipherTheme(darkTheme = darkTheme) {
                if (state.isChecking) {
                    LoadingScreen()
                } else {
                    var showSettings by rememberSaveable { mutableStateOf(false) }
                    if (showSettings) {
                        SettingsScreenRoute(onBack = { showSettings = false })
                    } else if (!state.isAuthed) {
                        AuthScreen(onOpenSettings = { showSettings = true })
                    } else {
                        MainScreen(onOpenSettings = { showSettings = true })
                    }
                }
            }
        }
    }
}

@Composable
private fun LoadingScreen() {
    Box(modifier = Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
        CircularProgressIndicator()
    }
}
