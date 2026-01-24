package com.xipher.wrapper.notifications;

import android.util.Log;

import androidx.annotation.NonNull;

import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.SecurityContext;

import java.util.Map;

import ru.rustore.sdk.pushclient.messaging.model.Notification;
import ru.rustore.sdk.pushclient.messaging.model.RemoteMessage;
import ru.rustore.sdk.pushclient.messaging.service.RuStoreMessagingService;

/**
 * RuStore push messaging service for handling push notifications in background.
 * This service is invoked by RuStore SDK when push is received, even when app is killed.
 */
public class MyRuStoreService extends RuStoreMessagingService {
    private static final String TAG = "MyRuStoreService";

    @Override
    public void onNewToken(@NonNull String token) {
        super.onNewToken(token);
        Log.d(TAG, "onNewToken: received new RuStore token");
        RuStorePushTokenManager.onNewToken(getApplicationContext(), token);
    }

    @Override
    public void onDeletedMessages() {
        super.onDeletedMessages();
        Log.d(TAG, "onDeletedMessages: pending messages expired on server");
        // Sync messages to ensure we have the latest state
        PushSyncHelper.syncAll(this);
    }

    @Override
    public void onMessageReceived(@NonNull RemoteMessage message) {
        // Note: We call super AFTER our handling for data-only messages
        // For notification+data messages, SDK will show notification automatically
        Log.d(TAG, "onMessageReceived: processing push message");

        if (SecurityContext.get().isPanicMode() || PanicModePrefs.isEnabled(this)) {
            Log.d(TAG, "onMessageReceived: ignored due to panic mode");
            super.onMessageReceived(message);
            return;
        }
        if (!PushTokenManager.isPushEnabled(this)) {
            Log.d(TAG, "onMessageReceived: ignored because push is disabled");
            super.onMessageReceived(message);
            return;
        }
        NotificationsService.startIfPossible(this);

        Map<String, String> data = message.getData();
        String type = data != null ? data.get("type") : null;
        String title = data != null ? data.get("title") : null;
        String body = data != null ? data.get("body") : null;
        Notification notification = message.getNotification();
        if (notification != null) {
            if (title == null) title = notification.getTitle();
            if (body == null) body = notification.getBody();
        }

        if ("call".equals(type)) {
            String callId = data != null ? data.get("call_id") : null;
            String callerId = data != null ? data.get("caller_id") : null;
            String callerName = data != null ? data.get("caller_name") : null;
            Log.d(TAG, "onMessageReceived: showing call notification for callId=" + callId);
            NotificationHelper.showCallNotification(this, callerName, callId, callerId);
            super.onMessageReceived(message);
            return;
        }

        String chatId = data != null ? data.get("chat_id") : null;
        String chatTitle = data != null ? data.get("chat_title") : null;
        String chatType = data != null ? data.get("chat_type") : null;
        String userName = data != null ? data.get("user_name") : null;
        String senderName = data != null ? data.get("sender_name") : null;
        if (senderName != null && body != null) {
            String prefix = senderName + ": ";
            if (body.startsWith(prefix)) {
                body = body.substring(prefix.length());
            }
        }
        if (ChatListPrefs.isMuted(this, chatId, chatType)) {
            Log.d(TAG, "onMessageReceived: chat is muted, skipping notification");
            super.onMessageReceived(message);
            return;
        }
        if (body == null || body.isEmpty() || chatId == null || chatId.isEmpty()) {
            Log.d(TAG, "onMessageReceived: syncing due to missing body or chatId");
            PushSyncHelper.syncAndNotify(this, chatId, chatType, title, body, userName, senderName);
            super.onMessageReceived(message);
            return;
        }
        
        // Show our custom notification for data-only messages
        Log.d(TAG, "onMessageReceived: showing message notification for chatId=" + chatId);
        NotificationHelper.showMessageNotification(this, title, body, chatId, chatTitle, chatType, userName, senderName);
        
        // Call super last - SDK may show notification if 'notification' field exists
        super.onMessageReceived(message);
    }
}
