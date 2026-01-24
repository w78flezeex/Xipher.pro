package com.xipher.wrapper.notifications;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.BasicResponse;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.di.AppServices;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import ru.rustore.sdk.pushclient.RuStorePushClient;

public final class RuStorePushTokenManager {
    private static final String TAG = "RuStorePushToken";
    private static final String PREFS = "xipher_prefs";
    private static final String KEY_RUSTORE_TOKEN = "rustore_push_token";
    private static final String KEY_RUSTORE_TOKEN_SENT = "rustore_push_token_sent";

    private static final ExecutorService IO = Executors.newSingleThreadExecutor();

    private RuStorePushTokenManager() {}

    public static void onNewToken(Context context, String token) {
        try {
            if (token == null || token.isEmpty()) return;
            Log.i(TAG, "RuStore push token: " + token);
            if (!PushTokenManager.isPushEnabled(context)) {
                saveToken(context, token, false);
                return;
            }
            saveToken(context, token, false);
            syncIfPossible(context);
        } catch (Throwable t) {
            Log.w(TAG, "RuStore token handling failed", t);
        }
    }

    public static void syncIfPossible(Context context) {
        try {
            if (!PushTokenManager.isPushEnabled(context)) return;
            if (!RuStorePushClient.INSTANCE.isInitialized()) return;

            AuthRepository authRepo = AppServices.authRepository(context);
            String authToken = authRepo.getToken();
            String cachedToken = getPrefs(context).getString(KEY_RUSTORE_TOKEN, null);
            String sentToken = getPrefs(context).getString(KEY_RUSTORE_TOKEN_SENT, null);

            RuStorePushClient.INSTANCE.getToken()
                    .addOnSuccessListener(token -> {
                        String resolved = token != null && !token.isEmpty() ? token : cachedToken;
                        if (resolved == null || resolved.isEmpty()) return;
                        boolean alreadySent = resolved.equals(sentToken);
                        saveToken(context, resolved, alreadySent);
                        if (authToken == null || authToken.isEmpty() || alreadySent) return;
                        sendToken(context, authToken, resolved);
                    })
                    .addOnFailureListener(e -> {
                        Log.w(TAG, "Failed to get RuStore token", e);
                        if (cachedToken != null && !cachedToken.isEmpty()
                                && !cachedToken.equals(sentToken)
                                && authToken != null
                                && !authToken.isEmpty()) {
                            sendToken(context, authToken, cachedToken);
                        }
                    });
        } catch (Throwable t) {
            Log.w(TAG, "RuStore token sync failed", t);
        }
    }

    public static void onPushEnabledChanged(Context context, boolean enabled) {
        try {
            if (enabled) {
                syncIfPossible(context);
            } else {
                deleteToken(context);
            }
        } catch (Throwable t) {
            Log.w(TAG, "RuStore push toggle failed", t);
        }
    }

    private static void sendToken(Context context, String authToken, String token) {
        IO.execute(() -> {
            try {
                ApiClient client = new ApiClient(authToken);
                BasicResponse res = client.registerPushToken(authToken, token, "rustore");
                if (res != null && res.success) {
                    saveToken(context, token, true);
                }
            } catch (Exception e) {
                Log.w(TAG, "Failed to send RuStore token", e);
            }
        });
    }

    private static void deleteToken(Context context) {
        AuthRepository authRepo = AppServices.authRepository(context);
        String authToken = authRepo.getToken();
        if (authToken == null || authToken.isEmpty()) return;
        String stored = getPrefs(context).getString(KEY_RUSTORE_TOKEN, null);
        if (stored == null || stored.isEmpty()) return;
        IO.execute(() -> {
            try {
                ApiClient client = new ApiClient(authToken);
                BasicResponse res = client.deletePushToken(authToken, stored);
                if (res != null && res.success) {
                    saveToken(context, stored, false);
                }
            } catch (Exception e) {
                Log.w(TAG, "Failed to delete RuStore token", e);
            }
        });
    }

    private static void saveToken(Context context, String token, boolean sent) {
        SharedPreferences prefs = getPrefs(context);
        prefs.edit()
                .putString(KEY_RUSTORE_TOKEN, token)
                .putString(KEY_RUSTORE_TOKEN_SENT, sent ? token : null)
                .apply();
    }

    public static String getCachedToken(Context context) {
        return getPrefs(context).getString(KEY_RUSTORE_TOKEN, null);
    }

    private static SharedPreferences getPrefs(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
