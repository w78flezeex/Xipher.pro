package com.xipher.wrapper.notifications;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

import com.google.firebase.messaging.FirebaseMessaging;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.BasicResponse;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.security.SecurityContext;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class PushTokenManager {
    private static final String TAG = "PushTokenManager";
    private static final String PREFS = "xipher_prefs";
    private static final String KEY_FCM_TOKEN = "fcm_token";
    private static final String KEY_FCM_TOKEN_SENT = "fcm_token_sent";
    private static final String KEY_PUSH_ENABLED = "push_enabled";

    private static final ExecutorService IO = Executors.newSingleThreadExecutor();

    private PushTokenManager() {}

    public static void onNewToken(Context context, String token) {
        if (token == null || token.isEmpty()) return;
        if (!isPushEnabled(context)) {
            saveToken(context, token, false);
            return;
        }
        saveToken(context, token, false);
        syncIfPossible(context);
    }

    public static void syncIfPossible(Context context) {
        if (SecurityContext.get().isPanicMode()) return;
        if (!isPushEnabled(context)) return;
        NotificationsService.startIfPossible(context);
        AuthRepository authRepo = AppServices.authRepository(context);
        String authToken = authRepo.getToken();
        String cachedToken = getPrefs(context).getString(KEY_FCM_TOKEN, null);
        String sentToken = getPrefs(context).getString(KEY_FCM_TOKEN_SENT, null);

        try {
            RuStorePushTokenManager.syncIfPossible(context);
        } catch (Throwable t) {
            Log.w(TAG, "RuStore token sync failed", t);
        }

        try {
            FirebaseMessaging.getInstance().getToken()
                    .addOnSuccessListener(token -> {
                        String resolved = token != null && !token.isEmpty() ? token : cachedToken;
                        if (resolved == null || resolved.isEmpty()) return;
                        boolean alreadySent = resolved.equals(sentToken);
                        saveToken(context, resolved, alreadySent);
                        if (authToken == null || authToken.isEmpty()) return;
                        NotificationsService.startIfPossible(context);
                        if (alreadySent) return;
                        sendToken(context, authToken, resolved);
                    })
                    .addOnFailureListener(e -> {
                        Log.w(TAG, "FCM token fetch failed", e);
                        if (cachedToken != null && !cachedToken.isEmpty()
                                && !cachedToken.equals(sentToken)
                                && authToken != null
                                && !authToken.isEmpty()) {
                            NotificationsService.startIfPossible(context);
                            sendToken(context, authToken, cachedToken);
                        }
                    });
        } catch (Throwable t) {
            Log.w(TAG, "FCM token fetch failed", t);
            if (cachedToken != null && !cachedToken.isEmpty()
                    && !cachedToken.equals(sentToken)
                    && authToken != null
                    && !authToken.isEmpty()) {
                NotificationsService.startIfPossible(context);
                sendToken(context, authToken, cachedToken);
            }
        }
    }

    public static boolean isPushEnabled(Context context) {
        return getPrefs(context).getBoolean(KEY_PUSH_ENABLED, true);
    }

    public static void setPushEnabled(Context context, boolean enabled) {
        SharedPreferences prefs = getPrefs(context);
        prefs.edit().putBoolean(KEY_PUSH_ENABLED, enabled).apply();
        if (enabled) {
            syncIfPossible(context);
        } else {
            deleteToken(context);
            NotificationsService.stop(context);
            try {
                RuStorePushTokenManager.onPushEnabledChanged(context, false);
            } catch (Throwable t) {
                Log.w(TAG, "RuStore token delete failed", t);
            }
        }
    }

    private static void sendToken(Context context, String authToken, String fcmToken) {
        IO.execute(() -> {
            try {
                ApiClient client = new ApiClient(authToken);
                BasicResponse res = client.registerPushToken(authToken, fcmToken, "android");
                if (res != null && res.success) {
                    saveToken(context, fcmToken, true);
                }
            } catch (Exception ignored) {
                // keep token marked as unsent for a later retry
            }
        });
    }

    private static void deleteToken(Context context) {
        AuthRepository authRepo = AppServices.authRepository(context);
        String authToken = authRepo.getToken();
        if (authToken == null || authToken.isEmpty()) return;
        String fcmToken = getPrefs(context).getString(KEY_FCM_TOKEN, null);
        if (fcmToken == null || fcmToken.isEmpty()) return;
        IO.execute(() -> {
            try {
                ApiClient client = new ApiClient(authToken);
                BasicResponse res = client.deletePushToken(authToken, fcmToken);
                if (res != null && res.success) {
                    saveToken(context, fcmToken, false);
                }
            } catch (Exception ignored) {
            }
        });
    }

    private static void saveToken(Context context, String token, boolean sent) {
        SharedPreferences prefs = getPrefs(context);
        prefs.edit()
                .putString(KEY_FCM_TOKEN, token)
                .putString(KEY_FCM_TOKEN_SENT, sent ? token : null)
                .apply();
    }

    public static String getCachedToken(Context context) {
        return getPrefs(context).getString(KEY_FCM_TOKEN, null);
    }

    public static CachedToken getCachedTokenInfo(Context context) {
        String ru = RuStorePushTokenManager.getCachedToken(context);
        if (ru != null && !ru.isEmpty()) {
            return new CachedToken(ru, "rustore");
        }
        String fcm = getCachedToken(context);
        if (fcm != null && !fcm.isEmpty()) {
            return new CachedToken(fcm, "android");
        }
        return null;
    }

    public static final class CachedToken {
        public final String token;
        public final String platform;

        public CachedToken(String token, String platform) {
            this.token = token;
            this.platform = platform;
        }
    }

    private static SharedPreferences getPrefs(Context context) {
        return context.getApplicationContext().getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }
}
