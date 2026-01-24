package com.xipher.core.storage

import android.content.Context
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey

class SecureTokenStore(context: Context) {
    private val prefs = EncryptedSharedPreferences.create(
        context,
        "secure_tokens",
        MasterKey.Builder(context).setKeyScheme(MasterKey.KeyScheme.AES256_GCM).build(),
        EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
        EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
    )

    fun setSession(token: String, userId: String, username: String) {
        prefs.edit()
            .putString("session_token", token)
            .putString("session_user_id", userId)
            .putString("session_username", username)
            .apply()
    }

    fun getToken(): String = prefs.getString("session_token", "") ?: ""

    fun getUserId(): String = prefs.getString("session_user_id", "") ?: ""

    fun getUsername(): String = prefs.getString("session_username", "") ?: ""

    fun clear() {
        prefs.edit()
            .remove("session_token")
            .remove("session_user_id")
            .remove("session_username")
            .apply()
    }
}
