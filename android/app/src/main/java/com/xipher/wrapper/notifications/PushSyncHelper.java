package com.xipher.wrapper.notifications;

import android.content.Context;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.db.DatabaseManager;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.PinPrefs;
import com.xipher.wrapper.security.SecurityContext;
import com.xipher.wrapper.util.ChecklistCodec;
import com.xipher.wrapper.util.SpoilerCodec;

import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class PushSyncHelper {
    private static final ExecutorService IO = Executors.newSingleThreadExecutor();

    private PushSyncHelper() {}

    public static void syncAndNotify(Context context,
                                    String chatId,
                                    String chatType,
                                    String titleHint,
                                    String bodyHint,
                                    String userNameHint,
                                    String senderNameHint) {
        if (context == null) return;
        Context app = context.getApplicationContext();
        IO.execute(() -> {
            if (SecurityContext.get().isPanicMode() || PanicModePrefs.isEnabled(app)) {
                return;
            }
            AuthRepository authRepo = AppServices.authRepository(app);
            String token = authRepo.getToken();
            if (token == null || token.isEmpty()) return;
            if (!ensureDatabaseReady(app)) {
                if (chatId != null && !chatId.isEmpty()
                        && ChatListPrefs.isMuted(app, chatId, chatType)) {
                    return;
                }
                String fallbackTitle = titleHint != null && !titleHint.isEmpty()
                        ? titleHint
                        : app.getString(R.string.app_name);
                String fallbackBody = bodyHint != null && !bodyHint.isEmpty()
                        ? bodyHint
                        : app.getString(R.string.chat_preview_new_messages);
                NotificationHelper.showMessageNotification(
                        app,
                        fallbackTitle,
                        fallbackBody,
                        chatId,
                        titleHint,
                        chatType,
                        userNameHint,
                        senderNameHint
                );
                return;
            }
            if (chatId == null || chatId.isEmpty()) {
                syncChatsAndNotify(app, token);
                return;
            }
            ChatRepository repo = new ChatRepository(app, token);
            List<MessageEntity> list = fetchMessages(repo, token, chatId, chatType);
            MessageEntity latest = pickLatestIncoming(list, authRepo.getUserId());
            if (latest == null) return;
            ChatEntity chat = null;
            try {
                AppDatabase db = AppDatabase.get(app);
                chat = db.chatDao().getById(chatId);
            } catch (Exception ignored) {
            }
            String resolvedChatType = resolveChatType(chatType, chat);
            String chatTitle = chat != null ? chat.title : null;
            String userName = userNameHint != null && !userNameHint.isEmpty()
                    ? userNameHint
                    : (chat != null ? chat.username : null);
            String senderName = latest.senderName != null && !latest.senderName.isEmpty()
                    ? latest.senderName
                    : senderNameHint;
            String title = chatTitle != null && !chatTitle.isEmpty()
                    ? chatTitle
                    : (userName != null && !userName.isEmpty() ? userName : titleHint);
            String body = buildPreview(app, latest, bodyHint);
            NotificationHelper.showMessageNotification(app, title, body, chatId, chatTitle, resolvedChatType, userName, senderName);
        });
    }

    private static List<MessageEntity> fetchMessages(ChatRepository repo, String token, String chatId, String chatType) {
        String type = chatType != null ? chatType.toLowerCase(Locale.US) : "";
        try {
            if ("group".equals(type)) {
                return repo.syncGroupMessages(token, chatId);
            }
            if ("channel".equals(type)) {
                return repo.syncChannelMessages(token, chatId);
            }
            return repo.syncMessages(token, chatId);
        } catch (Exception ignored) {
            return null;
        }
    }

    private static MessageEntity pickLatestIncoming(List<MessageEntity> list, String selfId) {
        if (list == null || list.isEmpty()) return null;
        for (int i = list.size() - 1; i >= 0; i--) {
            MessageEntity m = list.get(i);
            if (m == null) continue;
            if (selfId != null && m.senderId != null && selfId.equals(m.senderId)) continue;
            return m;
        }
        return null;
    }

    private static String buildPreview(Context context, MessageEntity message, String bodyHint) {
        if (message == null) return bodyHint != null ? bodyHint : "";
        String content = message.content != null ? message.content : "";
        if (SpoilerCodec.isSpoilerContent(content)) {
            return context.getString(R.string.chat_preview_spoiler);
        }
        if (ChecklistCodec.isChecklistContent(content)) {
            return context.getString(R.string.chat_preview_checklist);
        }
        if (ChecklistCodec.isChecklistUpdateContent(content)) {
            return context.getString(R.string.chat_preview_checklist);
        }
        String messageType = message.messageType != null ? message.messageType : "text";
        switch (messageType) {
            case "voice":
                return context.getString(R.string.chat_preview_voice);
            case "image":
                return context.getString(R.string.chat_preview_photo);
            case "file":
                return context.getString(R.string.chat_preview_file);
            case "location":
                return context.getString(R.string.chat_preview_location);
            case "live_location":
                return context.getString(R.string.chat_preview_live_location);
            default:
                return content;
        }
    }

    private static void syncChatsAndNotify(Context context, String token) {
        if (!ensureDatabaseReady(context)) return;
        AppDatabase db = AppDatabase.get(context);
        List<ChatEntity> cached = db.chatDao().getAll();
        Map<String, ChatEntity> cachedById = new HashMap<>();
        for (ChatEntity chat : cached) {
            if (chat != null && chat.id != null) {
                cachedById.put(chat.id, chat);
            }
        }
        ChatRepository repo = new ChatRepository(context, token);
        List<ChatEntity> updated;
        try {
            updated = repo.syncChats(token);
        } catch (Exception ignored) {
            return;
        }
        if (updated == null || updated.isEmpty()) return;
        for (ChatEntity chat : updated) {
            if (chat == null || chat.id == null || chat.id.isEmpty()) continue;
            int oldUnread = 0;
            ChatEntity old = cachedById.get(chat.id);
            if (old != null) oldUnread = old.unread;
            if (chat.unread <= oldUnread) continue;
            if (chat.lastMessage == null || chat.lastMessage.isEmpty()) continue;
            String chatType = resolveChatType(null, chat);
            NotificationHelper.showMessageNotification(
                    context,
                    chat.title,
                    chat.lastMessage,
                    chat.id,
                    chat.title,
                    chatType,
                    chat.username,
                    null
            );
        }
    }

    private static String resolveChatType(String hint, ChatEntity chat) {
        if (hint != null && !hint.isEmpty()) {
            return hint.toLowerCase(Locale.US);
        }
        if (chat != null) {
            if (chat.isChannel) return "channel";
            if (chat.isGroup) return "group";
        }
        return "chat";
    }

    /**
     * Sync all chats and show notification for any unread messages.
     * Called when onDeletedMessages is received (messages expired on server).
     */
    public static void syncAll(Context context) {
        if (context == null) return;
        Context app = context.getApplicationContext();
        IO.execute(() -> {
            if (SecurityContext.get().isPanicMode() || PanicModePrefs.isEnabled(app)) {
                return;
            }
            AuthRepository authRepo = AppServices.authRepository(app);
            String token = authRepo.getToken();
            if (token == null || token.isEmpty()) return;
            if (!ensureDatabaseReady(app)) return;
            syncChatsAndNotify(app, token);
        });
    }

    private static boolean ensureDatabaseReady(Context context) {
        if (context == null) return false;
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
