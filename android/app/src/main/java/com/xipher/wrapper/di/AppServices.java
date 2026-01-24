package com.xipher.wrapper.di;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.security.SecurePrefs;

public class AppServices {
    private static final String TAG = "AppServices";
    private static AuthRepository authRepo;

    public static AuthRepository authRepository(Context ctx) {
        if (authRepo == null) {
            try {
                Context app = ctx.getApplicationContext();
                SharedPreferences secure = SecurePrefs.get(app);
                SharedPreferences legacy = app.getSharedPreferences("xipher_prefs", Context.MODE_PRIVATE);
                authRepo = new AuthRepository(secure, legacy);
            } catch (Throwable t) {
                Log.e(TAG, "Failed to create AuthRepository", t);
                // Fallback to plain prefs
                Context app = ctx.getApplicationContext();
                SharedPreferences plain = app.getSharedPreferences("xipher_auth_prefs", Context.MODE_PRIVATE);
                SharedPreferences legacy = app.getSharedPreferences("xipher_prefs", Context.MODE_PRIVATE);
                authRepo = new AuthRepository(plain, legacy);
            }
        }
        return authRepo;
    }
}
