package com.xipher.wrapper.security;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Base64;
import android.util.Log;

import androidx.security.crypto.EncryptedSharedPreferences;
import androidx.security.crypto.MasterKey;

import java.security.SecureRandom;

/**
 * Secure storage for database encryption keys.
 * OWASP Mobile Top 10 2024: M9 - Insecure Data Storage
 * 
 * Security: This class NEVER falls back to unencrypted storage.
 * Database encryption keys must always be stored securely.
 */
public final class DbKeyStore {
    private static final String TAG = "DbKeyStore";
    private static final String PREFS = "xipher_secure_prefs";
    private static final String KEY_MAIN_DB = "main_db_key";
    private static final int KEY_LEN_BYTES = 32;

    private DbKeyStore() {}

    public static String getOrCreateMainKey(Context context) {
        SharedPreferences prefs = getPrefs(context);
        String stored = prefs.getString(KEY_MAIN_DB, null);
        if (stored != null && !stored.isEmpty()) {
            return stored;
        }
        byte[] key = new byte[KEY_LEN_BYTES];
        new SecureRandom().nextBytes(key);
        String encoded = Base64.encodeToString(key, Base64.NO_WRAP);
        prefs.edit().putString(KEY_MAIN_DB, encoded).apply();
        return encoded;
    }

    public static String getMainKey(Context context) {
        return getPrefs(context).getString(KEY_MAIN_DB, null);
    }

    public static void clearMainKey(Context context) {
        getPrefs(context).edit().remove(KEY_MAIN_DB).apply();
    }

    private static SharedPreferences getPrefs(Context context) {
        try {
            MasterKey masterKey = new MasterKey.Builder(context)
                    .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                    .build();
            return EncryptedSharedPreferences.create(
                    context,
                    PREFS,
                    masterKey,
                    EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                    EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
            );
        } catch (Exception e) {
            Log.e(TAG, "Failed to create encrypted key storage, trying to recover", e);
            // Try to recover by clearing corrupted encrypted prefs
            try {
                // Clear the encrypted prefs file
                java.io.File prefsDir = new java.io.File(context.getApplicationInfo().dataDir, "shared_prefs");
                java.io.File encPrefsFile = new java.io.File(prefsDir, PREFS + ".xml");
                if (encPrefsFile.exists()) {
                    encPrefsFile.delete();
                }
                // Retry
                MasterKey masterKey = new MasterKey.Builder(context)
                        .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                        .build();
                return EncryptedSharedPreferences.create(
                        context,
                        PREFS,
                        masterKey,
                        EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                        EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
                );
            } catch (Exception e2) {
                Log.e(TAG, "Recovery failed - device KeyStore may be corrupted", e2);
                throw new SecurityException("Secure key storage unavailable. Try clearing app data.", e2);
            }
        }
    }
}
