package com.xipher.wrapper.notifications;

import android.Manifest;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

import androidx.core.app.NotificationCompat;
import androidx.core.app.NotificationManagerCompat;
import androidx.core.content.ContextCompat;

import com.xipher.wrapper.R;
import com.xipher.wrapper.ui.CallActivity;
import com.xipher.wrapper.ui.ChatActivity;

public final class NotificationHelper {
    public static final String CHANNEL_MESSAGES = "channel_messages";
    public static final String CHANNEL_MESSAGES_LEGACY = "messages";
    public static final String CHANNEL_CALLS = "calls";

    public static final String EXTRA_CHAT_ID = "chat_id";
    public static final String EXTRA_CHAT_TITLE = "chat_title";
    public static final String EXTRA_CHAT_TYPE = "chat_type";
    public static final String EXTRA_USER_NAME = "user_name";
    public static final String EXTRA_CALL_ID = "call_id";
    public static final String EXTRA_CALLER_ID = "caller_id";
    public static final String EXTRA_CALLER_NAME = "caller_name";

    private NotificationHelper() {}

    public static void showMessageNotification(Context context,
                                               String title,
                                               String body,
                                               String chatId,
                                               String chatTitle,
                                               String chatType,
                                               String userName,
                                               String senderName) {
        String safeTitle = title != null ? title : context.getString(R.string.app_name);
        String conversationTitle = chatTitle != null && !chatTitle.isEmpty()
                ? chatTitle
                : (userName != null && !userName.isEmpty() ? userName : safeTitle);
        String displayName = senderName != null && !senderName.isEmpty()
                ? senderName
                : (userName != null && !userName.isEmpty() ? userName : conversationTitle);
        String safeBody = body != null ? body : "";

        String effectiveUserName = userName != null && !userName.isEmpty() ? userName : chatTitle;
        Intent intent = new Intent(context, ChatActivity.class);
        intent.putExtra(EXTRA_CHAT_ID, chatId);
        intent.putExtra(EXTRA_CHAT_TITLE, chatTitle);
        intent.putExtra(EXTRA_CHAT_TYPE, chatType);
        intent.putExtra(EXTRA_USER_NAME, effectiveUserName);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);

        PendingIntent pendingIntent = PendingIntent.getActivity(
                context,
                safeRequestCode(chatId),
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        androidx.core.app.Person sender = new androidx.core.app.Person.Builder()
                .setName(displayName != null ? displayName : context.getString(R.string.app_name))
                .build();
        NotificationCompat.MessagingStyle style = new NotificationCompat.MessagingStyle(sender)
                .setConversationTitle(conversationTitle);
        style.addMessage(safeBody, System.currentTimeMillis(), sender);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(context, CHANNEL_MESSAGES)
                .setSmallIcon(android.R.drawable.stat_notify_chat)
                .setContentTitle(conversationTitle)
                .setContentText(safeBody)
                .setStyle(style)
                .setAutoCancel(true)
                .setContentIntent(pendingIntent)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setCategory(NotificationCompat.CATEGORY_MESSAGE);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        NotificationManagerCompat.from(context).notify(safeRequestCode(chatId), builder.build());
    }

    public static void showCallNotification(Context context,
                                            String callerName,
                                            String callId,
                                            String callerId) {
        Intent intent = new Intent(context, CallActivity.class);
        intent.putExtra(CallActivity.EXTRA_CALL_ID, callId);
        intent.putExtra(CallActivity.EXTRA_PEER_ID, callerId);
        intent.putExtra(CallActivity.EXTRA_PEER_NAME, callerName);
        intent.putExtra(CallActivity.EXTRA_INCOMING, true);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);

        PendingIntent pendingIntent = PendingIntent.getActivity(
                context,
                safeRequestCode(callId),
                intent,
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE
        );

        String title = context.getString(R.string.call_notification_title);
        String body = callerName != null ? callerName : context.getString(R.string.unknown_user);

        NotificationCompat.Builder builder = new NotificationCompat.Builder(context, CHANNEL_CALLS)
                .setSmallIcon(android.R.drawable.stat_sys_phone_call)
                .setContentTitle(title)
                .setContentText(body)
                .setAutoCancel(true)
                .setContentIntent(pendingIntent)
                .setFullScreenIntent(pendingIntent, true)
                .setCategory(NotificationCompat.CATEGORY_CALL)
                .setPriority(NotificationCompat.PRIORITY_HIGH);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                && ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED) {
            return;
        }
        NotificationManagerCompat.from(context).notify(safeRequestCode(callId), builder.build());
    }

    private static int safeRequestCode(String seed) {
        return seed == null ? 1000 : Math.abs(seed.hashCode());
    }
}
