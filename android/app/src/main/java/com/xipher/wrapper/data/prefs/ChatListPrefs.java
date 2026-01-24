package com.xipher.wrapper.data.prefs;

import android.content.Context;
import android.content.SharedPreferences;

import com.google.gson.Gson;
import com.google.gson.reflect.TypeToken;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.model.ChatFolder;

import java.lang.reflect.Type;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

public final class ChatListPrefs {

    private static final String PREFS = "xipher_prefs";
    private static final String KEY_PINNED = "chat_pinned_ids";
    private static final String KEY_MUTED = "chat_muted_ids";
    private static final String KEY_HIDDEN = "chat_hidden_ids";
    private static final String KEY_PINNED_SYNCED = "chat_pinned_synced";
    private static final String KEY_FOLDERS = "chat_folders_json";
    private static final String KEY_ACTIVE_FOLDER = "chat_active_folder";
    private static final String KEY_FOLDERS_SYNCED = "chat_folders_synced";
    private static final String KEY_PRIVACY_BLUR = "chat_privacy_blur";
    private static final String FOLDER_ALL = "all";
    private static final Gson GSON = new Gson();
    private static final Type FOLDER_LIST_TYPE = new TypeToken<List<ChatFolder>>() {}.getType();

    private ChatListPrefs() {}

    public static String buildKey(ChatEntity chat) {
        if (chat == null || chat.id == null || chat.id.isEmpty()) return "";
        String type;
        if (chat.isChannel) {
            type = "channel";
        } else if (chat.isGroup) {
            type = "group";
        } else if (chat.isSaved) {
            type = "saved";
        } else {
            type = "chat";
        }
        return type + ":" + chat.id;
    }

    public static String buildKey(String chatId, String chatType) {
        if (chatId == null || chatId.isEmpty()) return "";
        String type = normalizeType(chatType);
        return type + ":" + chatId;
    }

    public static boolean isPinned(Context context, String key) {
        return key != null && !key.isEmpty() && getPinned(context).contains(key);
    }

    public static boolean isMuted(Context context, String key) {
        return key != null && !key.isEmpty() && getMuted(context).contains(key);
    }

    public static boolean isMuted(Context context, String chatId, String chatType) {
        String key = buildKey(chatId, chatType);
        return isMuted(context, key);
    }

    public static Set<String> getPinned(Context context) {
        return getSet(context, KEY_PINNED);
    }

    public static Set<String> getMuted(Context context) {
        return getSet(context, KEY_MUTED);
    }

    public static Set<String> getHidden(Context context) {
        return getSet(context, KEY_HIDDEN);
    }

    public static void setPinned(Context context, String key, boolean pinned) {
        setState(context, KEY_PINNED, key, pinned);
    }

    public static void setPinnedSet(Context context, Set<String> keys) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        Set<String> safe = keys == null ? new HashSet<>() : new HashSet<>(keys);
        prefs.edit().putStringSet(KEY_PINNED, safe).apply();
    }

    public static void setMuted(Context context, String key, boolean muted) {
        setState(context, KEY_MUTED, key, muted);
    }

    public static void setHidden(Context context, String key, boolean hidden) {
        setState(context, KEY_HIDDEN, key, hidden);
    }

    public static boolean isPinnedSynced(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.getBoolean(KEY_PINNED_SYNCED, false);
    }

    public static void setPinnedSynced(Context context, boolean synced) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(KEY_PINNED_SYNCED, synced).apply();
    }

    public static boolean isFoldersSynced(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.getBoolean(KEY_FOLDERS_SYNCED, false);
    }

    public static void setFoldersSynced(Context context, boolean synced) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(KEY_FOLDERS_SYNCED, synced).apply();
    }

    public static List<ChatFolder> getFolders(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String raw = prefs.getString(KEY_FOLDERS, null);
        if (raw == null || raw.isEmpty()) {
            return new ArrayList<>();
        }
        try {
            List<ChatFolder> parsed = GSON.fromJson(raw, FOLDER_LIST_TYPE);
            if (parsed == null) return new ArrayList<>();
            List<ChatFolder> cleaned = new ArrayList<>();
            for (ChatFolder folder : parsed) {
                if (folder == null || folder.id == null || folder.id.isEmpty() || folder.name == null || folder.name.isEmpty()) {
                    continue;
                }
                Set<String> unique = new HashSet<>();
                if (folder.chatKeys != null) {
                    for (String key : folder.chatKeys) {
                        if (key != null && !key.isEmpty()) {
                            unique.add(key);
                        }
                    }
                }
                folder.chatKeys = new ArrayList<>(unique);
                cleaned.add(folder);
            }
            return cleaned;
        } catch (Exception e) {
            return new ArrayList<>();
        }
    }

    public static void setFolders(Context context, List<ChatFolder> folders) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String json = GSON.toJson(folders != null ? folders : new ArrayList<>());
        prefs.edit().putString(KEY_FOLDERS, json).apply();
    }

    public static String getActiveFolder(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String stored = prefs.getString(KEY_ACTIVE_FOLDER, FOLDER_ALL);
        return stored == null || stored.isEmpty() ? FOLDER_ALL : stored;
    }

    public static void setActiveFolder(Context context, String folderId) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String safe = folderId == null || folderId.isEmpty() ? FOLDER_ALL : folderId;
        prefs.edit().putString(KEY_ACTIVE_FOLDER, safe).apply();
    }

    public static boolean hasStoredFolders(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.contains(KEY_FOLDERS);
    }

    public static boolean isChatListPrivacyEnabled(Context context) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.getBoolean(KEY_PRIVACY_BLUR, false);
    }

    public static void setChatListPrivacyEnabled(Context context, boolean enabled) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putBoolean(KEY_PRIVACY_BLUR, enabled).apply();
    }

    private static String normalizeType(String chatType) {
        if (chatType == null) return "chat";
        String type = chatType.toLowerCase(Locale.US);
        if ("direct".equals(type) || "chat".equals(type) || "message".equals(type)) {
            return "chat";
        }
        if ("saved".equals(type) || "saved_messages".equals(type)) {
            return "saved";
        }
        if ("group".equals(type)) {
            return "group";
        }
        if ("channel".equals(type)) {
            return "channel";
        }
        return type;
    }

    private static Set<String> getSet(Context context, String key) {
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        Set<String> stored = prefs.getStringSet(key, Collections.emptySet());
        return stored == null ? new HashSet<>() : new HashSet<>(stored);
    }

    private static void setState(Context context, String key, String value, boolean enabled) {
        if (value == null || value.isEmpty()) return;
        SharedPreferences prefs = context.getApplicationContext()
                .getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        Set<String> set = new HashSet<>(prefs.getStringSet(key, Collections.emptySet()));
        if (enabled) {
            set.add(value);
        } else {
            set.remove(value);
        }
        prefs.edit().putStringSet(key, set).apply();
    }
}
