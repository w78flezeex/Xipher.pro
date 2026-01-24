package com.xipher.wrapper.notifications;

import androidx.annotation.NonNull;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.SecurityContext;

import java.util.Map;

public class XipherFirebaseMessagingService extends FirebaseMessagingService {

    @Override
    public void onNewToken(@NonNull String token) {
        super.onNewToken(token);
        PushTokenManager.onNewToken(getApplicationContext(), token);
    }

    @Override
    public void onMessageReceived(@NonNull RemoteMessage message) {
        super.onMessageReceived(message);

        if (SecurityContext.get().isPanicMode() || PanicModePrefs.isEnabled(this)) {
            return;
        }
        if (!PushTokenManager.isPushEnabled(this)) {
            return;
        }
        NotificationsService.startIfPossible(this);

        Map<String, String> data = message.getData();
        String type = data != null ? data.get("type") : null;
        String title = data != null ? data.get("title") : null;
        String body = data != null ? data.get("body") : null;
        if (message.getNotification() != null) {
            if (title == null) title = message.getNotification().getTitle();
            if (body == null) body = message.getNotification().getBody();
        }

        if ("call".equals(type)) {
            String callId = data != null ? data.get("call_id") : null;
            String callerId = data != null ? data.get("caller_id") : null;
            String callerName = data != null ? data.get("caller_name") : null;
            NotificationHelper.showCallNotification(this, callerName, callId, callerId);
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
            return;
        }
        if (body == null || body.isEmpty() || chatId == null || chatId.isEmpty()) {
            PushSyncHelper.syncAndNotify(this, chatId, chatType, title, body, userName, senderName);
            return;
        }
        NotificationHelper.showMessageNotification(this, title, body, chatId, chatTitle, chatType, userName, senderName);
    }
}
