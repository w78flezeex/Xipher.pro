package com.xipher.wrapper.security;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import androidx.security.crypto.EncryptedSharedPreferences;
import androidx.security.crypto.MasterKey;

/**
 * Secure SharedPreferences wrapper.
 * OWASP Mobile Top 10 2024: M9 - Insecure Data Storage
 * 
 * Security: This class NEVER falls back to unencrypted storage.
 * If encryption fails, it throws a SecurityException.
 */
public final class SecurePrefs {
    private static final String TAG = "SecurePrefs";
    private static final String PREFS = "xipher_auth_prefs";

    private SecurePrefs() {}

    /**
     * Get encrypted SharedPreferences.
     * 
     * @throws SecurityException if encryption is not available (device compromised or unsupported)
     */
    public static SharedPreferences get(Context context) {
        Context app = context.getApplicationContext();
        try {
            MasterKey masterKey = new MasterKey.Builder(app)
                    .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                    .build();
            return EncryptedSharedPreferences.create(
                    app,
                    PREFS,
                    masterKey,
                    EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                    EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
            );
        } catch (Exception e) {
            // Security: NEVER fall back to unencrypted storage
            // Log the error and throw SecurityException
            Log.e(TAG, "Failed to create encrypted preferences - device may be compromised", e);
            throw new SecurityException("Secure storage unavailable. Please update your device or reinstall the app.", e);
        }
    }
    
    /**
     * Check if secure storage is available without throwing.
     */
    public static boolean isAvailable(Context context) {
        try {
            get(context);
            return true;
        } catch (SecurityException e) {
            return false;
        }
    }
}
