package com.xipher.feature.auth

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.hilt.navigation.compose.hiltViewModel
import com.xipher.core.ui.XPrimaryButton
import com.xipher.core.ui.XTextInput

@Composable
fun AuthScreen(
    modifier: Modifier = Modifier,
    onOpenSettings: (() -> Unit)? = null,
    viewModel: AuthViewModel = hiltViewModel()
) {
    val state by viewModel.state.collectAsState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(horizontal = 24.dp, vertical = 32.dp),
        verticalArrangement = Arrangement.Center
    ) {
        Text(text = "Sign in", style = MaterialTheme.typography.headlineSmall)
        Spacer(modifier = Modifier.height(16.dp))
        XTextInput(
            value = state.username,
            onValueChange = viewModel::onUsernameChanged,
            placeholder = "Username",
            modifier = Modifier.fillMaxWidth()
        )
        Spacer(modifier = Modifier.height(12.dp))
        XTextInput(
            value = state.password,
            onValueChange = viewModel::onPasswordChanged,
            placeholder = "Password",
            modifier = Modifier.fillMaxWidth(),
            visualTransformation = PasswordVisualTransformation()
        )
        if (!state.error.isNullOrBlank()) {
            Spacer(modifier = Modifier.height(8.dp))
            Text(text = state.error.orEmpty(), color = MaterialTheme.colorScheme.error)
        }
        Spacer(modifier = Modifier.height(20.dp))
        XPrimaryButton(
            text = if (state.isLoading) "Signing in..." else "Sign in",
            onClick = viewModel::login,
            enabled = !state.isLoading,
            modifier = Modifier.fillMaxWidth()
        )
        if (onOpenSettings != null) {
            Spacer(modifier = Modifier.height(12.dp))
            XPrimaryButton(
                text = "Connection settings",
                onClick = onOpenSettings,
                modifier = Modifier.fillMaxWidth(),
                enabled = true
            )
        }
    }
}
