package com.xipher.core.storage

import android.content.Context
import androidx.datastore.preferences.core.Preferences
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore(name = "settings")

class SettingsStore(private val context: Context) {
    companion object {
        private val KEY_BASE_URL = stringPreferencesKey("base_url")
        private val KEY_THEME = stringPreferencesKey("theme")
        private val KEY_LANGUAGE = stringPreferencesKey("language")
    }

    val baseUrl: Flow<String> = context.dataStore.data.map { prefs ->
        prefs[KEY_BASE_URL] ?: "https://xipher.pro"
    }

    val theme: Flow<String> = context.dataStore.data.map { prefs ->
        prefs[KEY_THEME] ?: "dark"
    }

    val language: Flow<String> = context.dataStore.data.map { prefs ->
        prefs[KEY_LANGUAGE] ?: "en"
    }

    suspend fun setBaseUrl(value: String) {
        context.dataStore.edit { it[KEY_BASE_URL] = value }
    }

    suspend fun setTheme(value: String) {
        context.dataStore.edit { it[KEY_THEME] = value }
    }

    suspend fun setLanguage(value: String) {
        context.dataStore.edit { it[KEY_LANGUAGE] = value }
    }
}
