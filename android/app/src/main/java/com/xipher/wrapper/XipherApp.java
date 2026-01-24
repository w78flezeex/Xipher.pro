package com.xipher.wrapper;

import android.app.Application;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.os.Build;
import android.util.Log;

import androidx.emoji2.bundled.BundledEmojiCompatConfig;
import androidx.emoji2.text.EmojiCompat;

import com.google.firebase.FirebaseApp;
import com.google.firebase.messaging.FirebaseMessaging;
import com.yandex.mapkit.MapKitFactory;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.notifications.NotificationSyncWorker;
import com.xipher.wrapper.notifications.RuStoreLogger;

import ru.rustore.sdk.pushclient.RuStorePushClient;

public class XipherApp extends Application {
    private static final String TAG = "XipherApp";

    @Override
    public void onCreate() {
        super.onCreate();
        Thread.setDefaultUncaughtExceptionHandler(new CrashHandler(this));
        
        try {
            com.xipher.wrapper.theme.ThemeManager.applySaved(this);
        } catch (Throwable t) {
            Log.e(TAG, "ThemeManager init failed", t);
        }
        
        try {
            EmojiCompat.init(new BundledEmojiCompatConfig(this));
        } catch (Throwable t) {
            Log.e(TAG, "EmojiCompat init failed", t);
        }
        
        try {
            FirebaseApp.initializeApp(this);
            FirebaseMessaging.getInstance().setAutoInitEnabled(true);
        } catch (Throwable t) {
            Log.w(TAG, "Firebase init failed", t);
        }
        
        try {
            initRuStorePush();
        } catch (Throwable t) {
            Log.e(TAG, "RuStore init failed", t);
        }
        
        try {
            PushTokenManager.syncIfPossible(this);
        } catch (Throwable t) {
            Log.w(TAG, "Push token sync failed", t);
        }
        
        try {
            if (BuildConfig.MAPKIT_API_KEY != null && !BuildConfig.MAPKIT_API_KEY.isEmpty()) {
                MapKitFactory.setApiKey(BuildConfig.MAPKIT_API_KEY);
                MapKitFactory.initialize(this);
            }
        } catch (Throwable t) {
            Log.e(TAG, "MapKit init failed", t);
        }
        
        try {
            createChannel("channel_messages", getString(R.string.notification_channel_messages), NotificationManager.IMPORTANCE_HIGH);
            createChannel("messages", getString(R.string.notification_channel_messages), NotificationManager.IMPORTANCE_DEFAULT);
            createChannel("calls", getString(R.string.notification_channel_calls), NotificationManager.IMPORTANCE_HIGH);
            createChannel("updates", getString(R.string.notification_channel_updates), NotificationManager.IMPORTANCE_LOW);
        } catch (Throwable t) {
            Log.e(TAG, "Channel creation failed", t);
        }
        
        try {
            NotificationSyncWorker.schedule(this);
        } catch (Throwable t) {
            Log.e(TAG, "NotificationSyncWorker schedule failed", t);
        }
    }

    private void initRuStorePush() {
        try {
            String projectId = BuildConfig.RUSTORE_PROJECT_ID;
            if (projectId == null || projectId.isEmpty()) {
                Log.w(TAG, "RuStore push disabled: missing project id");
                return;
            }
            if (!RuStorePushClient.INSTANCE.isInitialized()) {
                RuStorePushClient.INSTANCE.init(this, projectId, new RuStoreLogger());
                Log.i(TAG, "RuStore push initialized with project: " + projectId);
            }
            // Check push availability and get token
            RuStorePushClient.INSTANCE.checkPushAvailability()
                    .addOnSuccessListener(result -> {
                        Log.i(TAG, "RuStore push availability: " + result);
                        // Request token if available
                        RuStorePushClient.INSTANCE.getToken()
                                .addOnSuccessListener(token -> {
                                    if (token != null && !token.isEmpty()) {
                                        Log.i(TAG, "RuStore push token obtained");
                                        com.xipher.wrapper.notifications.RuStorePushTokenManager.onNewToken(this, token);
                                    }
                                })
                                .addOnFailureListener(e -> Log.w(TAG, "Failed to get RuStore token", e));
                    })
                    .addOnFailureListener(e -> Log.w(TAG, "RuStore push not available", e));
        } catch (Throwable t) {
            Log.w(TAG, "RuStore push init failed", t);
        }
    }

    private void createChannel(String id, String name, int importance) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) {
                NotificationChannel channel = new NotificationChannel(id, name, importance);
                nm.createNotificationChannel(channel);
            }
        }
    }
}
