package com.xipher.wrapper.notifications;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.util.Log;

import ru.rustore.sdk.pushclient.RuStorePushClient;

public class AppStartReceiver extends BroadcastReceiver {
    public static final String ACTION_APP_START = "com.xipher.wrapper.action.START";
    private static final String TAG = "AppStartReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (context == null || intent == null) return;
        String action = intent.getAction();
        if (Intent.ACTION_BOOT_COMPLETED.equals(action)
                || Intent.ACTION_MY_PACKAGE_REPLACED.equals(action)
                || ACTION_APP_START.equals(action)) {
            Log.d(TAG, "onReceive: " + action);
            
            // Sync FCM and RuStore tokens
            PushTokenManager.syncIfPossible(context);
            
            // Schedule periodic notification sync
            NotificationSyncWorker.schedule(context);
            
            // Start notification service for WebSocket connection
            NotificationsService.startIfPossible(context);
            
            // Ensure RuStore SDK is initialized for background push
            try {
                if (RuStorePushClient.INSTANCE.isInitialized()) {
                    RuStorePushClient.INSTANCE.getToken()
                            .addOnSuccessListener(token -> {
                                if (token != null && !token.isEmpty()) {
                                    Log.d(TAG, "RuStore token refreshed on boot");
                                    RuStorePushTokenManager.onNewToken(context, token);
                                }
                            })
                            .addOnFailureListener(e -> Log.w(TAG, "Failed to refresh RuStore token", e));
                }
            } catch (Throwable t) {
                Log.w(TAG, "RuStore token refresh failed", t);
            }
        }
    }
}
