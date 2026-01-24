package com.xipher.wrapper.notifications;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.work.Constraints;
import androidx.work.ExistingPeriodicWorkPolicy;
import androidx.work.NetworkType;
import androidx.work.PeriodicWorkRequest;
import androidx.work.WorkManager;
import androidx.work.Worker;
import androidx.work.WorkerParameters;

import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.DatabaseManager;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.PinPrefs;
import com.xipher.wrapper.security.SecurityContext;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeUnit;

public class NotificationSyncWorker extends Worker {
    private static final String UNIQUE_NAME = "xipher_notification_sync";

    public NotificationSyncWorker(@NonNull Context context, @NonNull WorkerParameters params) {
        super(context, params);
    }

    public static void schedule(Context context) {
        try {
            Constraints constraints = new Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build();
            PeriodicWorkRequest request = new PeriodicWorkRequest.Builder(
                    NotificationSyncWorker.class,
                    15,
                    TimeUnit.MINUTES
            )
                    .setConstraints(constraints)
                    .build();
            WorkManager.getInstance(context.getApplicationContext())
                    .enqueueUniquePeriodicWork(UNIQUE_NAME, ExistingPeriodicWorkPolicy.UPDATE, request);
        } catch (Throwable t) {
            android.util.Log.e("NotificationSyncWorker", "Failed to schedule", t);
        }
    }

    @NonNull
    @Override
    public Result doWork() {
        Context context = getApplicationContext();
        if (SecurityContext.get().isPanicMode()) return Result.success();
        if (!PushTokenManager.isPushEnabled(context)) return Result.success();
        PushTokenManager.syncIfPossible(context);

        AuthRepository authRepo = AppServices.authRepository(context);
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return Result.success();

        if (!ensureDatabaseReady(context)) {
            return Result.success();
        }
        AppDatabase db = AppDatabase.get(context);
        List<ChatEntity> cached = db.chatDao().getAll();
        Map<String, Integer> unreadById = new HashMap<>();
        for (ChatEntity chat : cached) {
            if (chat != null && chat.id != null) {
                unreadById.put(chat.id, chat.unread);
            }
        }

        ChatRepository repo = new ChatRepository(context, token);
        List<ChatEntity> updated;
        try {
            updated = repo.syncChats(token);
        } catch (Exception e) {
            return Result.retry();
        }
        if (updated == null || updated.isEmpty()) return Result.success();
        for (ChatEntity chat : updated) {
            if (chat == null || chat.id == null || chat.id.isEmpty()) continue;
            int oldUnread = unreadById.getOrDefault(chat.id, 0);
            if (chat.unread <= oldUnread) continue;
            String chatType = chat.isChannel ? "channel" : (chat.isGroup ? "group" : "chat");
            if (ChatListPrefs.isMuted(context, chat.id, chatType)) continue;
            String body = chat.lastMessage != null ? chat.lastMessage : "";
            if (body.isEmpty()) continue;
            NotificationHelper.showMessageNotification(
                    context,
                    chat.title,
                    body,
                    chat.id,
                    chat.title,
                    chatType,
                    chat.username,
                    null
            );
        }
        return Result.success();
    }

    private boolean ensureDatabaseReady(Context context) {
        try {
            AppDatabase.get(context);
            return true;
        } catch (Exception ignored) {
        }
        if (PanicModePrefs.isEnabled(context)) return false;
        if (PinPrefs.isMainPinSet(context)) return false;
        String key = DbKeyStore.getMainKey(context);
        if (key == null || key.isEmpty()) return false;
        try {
            DatabaseManager.getInstance().init(context, key);
            return true;
        } catch (Exception ignored) {
            return false;
        }
    }
}
