package com.xipher.wrapper.ui.vm;

import android.app.Application;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Base64;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.model.ChecklistItem;
import com.xipher.wrapper.data.model.ChecklistItemUpdate;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.data.model.ChecklistUpdatePayload;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.ui.CallActivity;
import com.xipher.wrapper.util.ChecklistCodec;
import com.xipher.wrapper.util.SpoilerCodec;
import com.xipher.wrapper.ws.SocketManager;
import com.xipher.wrapper.security.SecurityContext;

import java.io.File;
import java.io.FileInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.IOException;
import java.net.URLConnection;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ConcurrentHashMap;

import okhttp3.Call;

public class ChatViewModel extends AndroidViewModel implements SocketManager.Listener {

    private static final String TAG = "ChatViewModel";
    private static final long SEND_RECOVERY_WINDOW_MS = 30_000L;
    private static final long SEND_RECOVERY_PAST_GRACE_MS = 5_000L;
    private static final long RECONNECT_MIN_MS = 5000L;
    private static final long RECONNECT_MAX_MS = 60000L;
    private static final long TYPING_SEND_THROTTLE_MS = 2000L;
    private static final long TYPING_IDLE_TIMEOUT_MS = 2500L;
    private static final long TYPING_DISPLAY_TIMEOUT_MS = 4500L;
    public static final int TRANSFER_IDLE = 0;
    public static final int TRANSFER_LOADING = 1;
    public static final int TRANSFER_DONE = 2;
    private final MutableLiveData<List<MessageEntity>> messages = new MutableLiveData<>(new ArrayList<>());
    private final MutableLiveData<String> errors = new MutableLiveData<>();
    private final MutableLiveData<TransferUpdate> transferUpdates = new MutableLiveData<>();
    private final MutableLiveData<TypingInfo> typingInfo = new MutableLiveData<>(new TypingInfo(0, new ArrayList<>()));
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Map<String, Call> uploadCalls = new ConcurrentHashMap<>();

    private ChatRepository repo;
    private AuthRepository authRepo;
    private AppDatabase db;
    private SocketManager socket;
    private String currentToken;
    private long reconnectDelayMs = RECONNECT_MIN_MS;
    private Runnable reconnectRunnable;
    private String chatId;
    private String selfId;
    private String selfName;
    private boolean isGroup;
    private boolean isChannel;
    private boolean isSaved;
    private final Set<String> deliveredSent = new HashSet<>();
    private final Set<String> readSent = new HashSet<>();
    private final Map<String, String> typingUsers = new HashMap<>();
    private final Map<String, Runnable> typingTimeouts = new HashMap<>();
    private boolean typingActive = false;
    private long typingLastSentAt = 0L;
    private Runnable typingIdleRunnable;

    public ChatViewModel(@NonNull Application app) {
        super(app);
        authRepo = AppServices.authRepository(app);
        try {
            db = AppDatabase.get(app);
        } catch (IllegalStateException e) {
            // Database not initialized yet - will be null
            db = null;
        }
    }

    public void init(String chatId) {
        init(chatId, false, false, false);
    }

    public void init(String chatId, boolean isGroup, boolean isChannel) {
        init(chatId, isGroup, isChannel, false);
    }

    public void init(String chatId, boolean isGroup, boolean isChannel, boolean isSaved) {
        this.chatId = chatId;
        this.selfId = authRepo != null ? authRepo.getUserId() : null;
        this.selfName = authRepo != null ? authRepo.getUsername() : null;
        this.isGroup = isGroup;
        this.isChannel = isChannel;
        this.isSaved = isSaved;
        String token = authRepo != null ? authRepo.getToken() : null;
        this.repo = new ChatRepository(getApplication(), token);
        clearTypingState();
        // начальная загрузка из кеша
        if (db != null) {
            io.execute(() -> {
                List<MessageEntity> cached = db.messageDao().getByChat(chatId);
                List<MessageEntity> sorted = sortMessages(cached);
                List<MessageEntity> processed = processChecklistMessages(sorted);
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
            });
        }
    }

    public LiveData<List<MessageEntity>> getMessages() { return messages; }
    public LiveData<String> getErrors() { return errors; }
    public LiveData<TransferUpdate> getTransferUpdates() { return transferUpdates; }
    public LiveData<TypingInfo> getTypingInfo() { return typingInfo; }
    public String getSelfId() { return selfId; }

    private void loadCachedMessages() {
        if (chatId == null || db == null) return;
        io.execute(() -> {
            List<MessageEntity> cached = db.messageDao().getByChat(chatId);
            List<MessageEntity> sorted = sortMessages(cached);
            List<MessageEntity> processed = processChecklistMessages(sorted);
            List<MessageEntity> pruned = filterExpiredMessages(processed);
            messages.postValue(pruned);
            if (isGroup || isChannel) {
                updatePreviewFromHistory(pruned);
            }
        });
    }

    public void refreshHistory() {
        if (isPanicMode()) {
            loadCachedMessages();
            return;
        }
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null || repo == null) return;
        io.execute(() -> {
            try {
                List<MessageEntity> list;
                if (isGroup) {
                    list = repo.syncGroupMessages(token, chatId);
                } else if (isChannel) {
                    list = repo.syncChannelMessages(token, chatId);
                } else {
                    list = repo.syncMessages(token, chatId);
                }
                if (list == null) list = new ArrayList<>();
                List<MessageEntity> sorted = sortMessages(list);
                List<MessageEntity> processed = processChecklistMessages(sorted);
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
                sendReceiptsForMessages(pruned);
                if (isGroup || isChannel) {
                    updatePreviewFromHistory(pruned);
                }
            } catch (Exception e) {
                errors.postValue(getApplication().getString(R.string.history_load_failed));
            }
        });
    }

    public void pollNewMessages() {
        if (isPanicMode()) {
            loadCachedMessages();
            return;
        }
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null) return;
        io.execute(() -> {
            try {
                List<MessageEntity> list;
                if (isGroup) {
                    list = repo.syncGroupMessages(token, chatId);
                } else if (isChannel) {
                    list = repo.syncChannelMessages(token, chatId);
                } else {
                    list = repo.syncMessages(token, chatId);
                }
                List<MessageEntity> sorted = sortMessages(list);
                List<MessageEntity> processed = processChecklistMessages(sorted);
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                List<MessageEntity> current = messages.getValue() != null ? messages.getValue() : new ArrayList<>();
                if (shouldUpdateMessages(current, pruned)) {
                    messages.postValue(pruned);
                    sendReceiptsForMessages(pruned);
                    if (isGroup || isChannel) {
                        updatePreviewFromHistory(pruned);
                    }
                }
            } catch (Exception ignored) {
            }
        });
    }

    public void deleteMessages(List<MessageEntity> selected) {
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null || selected == null || selected.isEmpty()) return;
        io.execute(() -> {
            try {
                List<String> deletedIds = new ArrayList<>();
                boolean hadError = false;
                for (MessageEntity m : selected) {
                    if (m == null || m.id == null) continue;
                    try {
                        if (isGroup) {
                            repo.deleteGroupMessage(token, chatId, m.id);
                        } else if (isChannel) {
                            repo.deleteChannelMessage(token, chatId, m.id);
                        } else {
                            repo.deleteMessage(token, m.id);
                        }
                        deletedIds.add(m.id);
                    } catch (Exception e) {
                        hadError = true;
                    }
                }
                if (hadError) {
                    errors.postValue(getApplication().getString(R.string.delete_failed));
                }
                if (deletedIds.isEmpty()) return;
                List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
                Set<String> ids = new HashSet<>();
                ids.addAll(deletedIds);
                current.removeIf(m -> m != null && m.id != null && ids.contains(m.id));
                List<MessageEntity> sorted = sortMessages(current);
                messages.postValue(sorted);
                updatePreviewFromHistory(sorted);
            } catch (Exception e) {
                errors.postValue(getApplication().getString(R.string.delete_failed));
            }
        });
    }

    public void forwardMessages(List<MessageEntity> selected, com.xipher.wrapper.data.db.entity.ChatEntity target) {
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || selected == null || selected.isEmpty() || target == null) return;
        io.execute(() -> {
            try {
                List<MessageEntity> ordered = new ArrayList<>(selected);
                ordered.sort((a, b) -> Long.compare(resolveTimestamp(a), resolveTimestamp(b)));
                boolean hadError = false;
                for (MessageEntity m : ordered) {
                    if (m == null) continue;
                    String type = m.messageType != null ? m.messageType : "text";
                    String content = m.content != null ? m.content : "";
                    if (isTextMessage(type, content)) {
                        content = buildForwardText(m);
                    }
                    try {
                        if (target.isGroup) {
                            repo.sendGroupMessage(token, target.id, content, type, null, selfId, selfName, m.filePath, m.fileName, m.fileSize);
                        } else if (target.isChannel) {
                            repo.sendChannelMessage(token, target.id, content, type, null, selfId, selfName, m.filePath, m.fileName, m.fileSize);
                        } else {
                            repo.sendMessage(token, target.id, content, type, null, selfId, m.filePath, m.fileName, m.fileSize);
                        }
                    } catch (Exception e) {
                        hadError = true;
                    }
                }
                if (hadError) {
                    errors.postValue(getApplication().getString(R.string.forward_failed));
                }
            } catch (Exception e) {
                errors.postValue(getApplication().getString(R.string.forward_failed));
            }
        });
    }

    public void sendMessage(String content) {
        sendMessage(content, false, 0L, null);
    }

    public void sendMessage(String content, boolean spoiler) {
        sendMessage(content, spoiler, 0L, null);
    }

    public void sendMessage(String content, boolean spoiler, long ttlSeconds) {
        sendMessage(content, spoiler, ttlSeconds, null);
    }

    public void sendMessage(String content, boolean spoiler, long ttlSeconds, String replyToId) {
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null) return;
        io.execute(() -> {
            String payload = content != null ? content : "";
            if (spoiler && !SpoilerCodec.isSpoilerContent(payload)) {
                payload = SpoilerCodec.encode(payload);
            }
            long attemptAtMillis = System.currentTimeMillis();
            try {
                MessageEntity m;
                if (isGroup) {
                    m = repo.sendGroupMessage(token, chatId, payload, "text", replyToId, selfId, selfName, null, null, null, ttlSeconds);
                } else if (isChannel) {
                    m = repo.sendChannelMessage(token, chatId, payload, "text", replyToId, selfId, selfName, null, null, null, ttlSeconds);
                } else {
                    m = repo.sendMessage(token, chatId, payload, "text", replyToId, selfId, null, null, null, ttlSeconds);
                }
                List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
                current.add(m);
                List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
                updatePreviewFromHistory(pruned);
            } catch (Exception e) {
                scheduleSendFailureCheck(payload, "text", attemptAtMillis);
            }
        });
    }

    public void sendChecklist(ChecklistPayload payload) {
        sendChecklist(payload, 0L);
    }

    public void sendChecklist(ChecklistPayload payload, long ttlSeconds) {
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null || payload == null) return;
        io.execute(() -> {
            String content = ChecklistCodec.encodeChecklist(payload);
            if (content == null || content.isEmpty()) return;
            long attemptAtMillis = System.currentTimeMillis();
            try {
                MessageEntity m;
                if (isGroup) {
                    m = repo.sendGroupMessage(token, chatId, content, "text", null, selfId, selfName, null, null, null, ttlSeconds);
                } else if (isChannel) {
                    m = repo.sendChannelMessage(token, chatId, content, "text", null, selfId, selfName, null, null, null, ttlSeconds);
                } else {
                    m = repo.sendMessage(token, chatId, content, "text", null, selfId, null, null, null, ttlSeconds);
                }
                List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
                current.add(m);
                List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
                updatePreviewFromHistory(pruned);
            } catch (Exception e) {
                scheduleSendFailureCheck(content, "text", attemptAtMillis);
            }
        });
    }

    public void sendFile(Uri uri, String fileName, long fileSize) {
        sendFile(uri, fileName, fileSize, false, 0L);
    }

    public void sendFile(Uri uri, String fileName, long fileSize, boolean spoiler) {
        sendFile(uri, fileName, fileSize, spoiler, 0L);
    }

    public void sendFile(Uri uri, String fileName, long fileSize, boolean spoiler, long ttlSeconds) {
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null || uri == null) return;
        io.execute(() -> {
            String tempId = "temp-" + UUID.randomUUID();
            String mimeType = null;
            try {
                if ("content".equals(uri.getScheme())) {
                    mimeType = getApplication().getContentResolver().getType(uri);
                }
            } catch (Exception e) {
                Log.w(TAG, "sendFile: mimeType lookup failed", e);
            }
            if (mimeType == null) {
                String nameForType = fileName != null ? fileName : uri.getLastPathSegment();
                if (nameForType != null) {
                    mimeType = URLConnection.guessContentTypeFromName(nameForType);
                }
            }
            boolean isImage = isImageFile(fileName) || (mimeType != null && mimeType.startsWith("image/"));
            boolean useSpoiler = spoiler && isImage;
            String messageType = isImage ? "image" : "file";
            String fallbackName = isImage
                    ? getApplication().getString(R.string.chat_preview_photo)
                    : getApplication().getString(R.string.chat_preview_file);
            String content = fileName != null ? fileName : fallbackName;
            if (useSpoiler && !SpoilerCodec.isSpoilerContent(content)) {
                content = SpoilerCodec.encode(content);
            }
            MessageEntity pending = new MessageEntity();
            pending.id = tempId;
            pending.tempId = tempId;
            pending.chatId = chatId;
            pending.senderId = selfId;
            pending.senderName = selfName;
            pending.content = content;
            pending.messageType = messageType;
            boolean panic = isPanicMode();
            pending.filePath = panic ? uri.toString() : (isImage ? uri.toString() : null);
            pending.fileName = fileName;
            pending.fileSize = fileSize > 0 ? fileSize : null;
            pending.status = panic ? "sent" : "pending";
            pending.createdAtMillis = System.currentTimeMillis();
            pending.createdAt = nowIso();

            List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
            current.add(pending);
            List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
            List<MessageEntity> pruned = filterExpiredMessages(processed);
            messages.postValue(pruned);
            transferUpdates.postValue(new TransferUpdate(tempId, TRANSFER_LOADING, 0, false));

            if (panic) {
                if (db != null) db.messageDao().upsert(pending);
                String preview = useSpoiler
                        ? getApplication().getString(R.string.chat_preview_spoiler)
                        : (messageType.equals("image")
                        ? getApplication().getString(R.string.chat_preview_photo)
                        : getApplication().getString(R.string.chat_preview_file));
                updateChatPreview(preview);
                transferUpdates.postValue(new TransferUpdate(tempId, TRANSFER_DONE, 100, true));
                return;
            }

            try {
                ReadBytesResult rr = readBytes(getApplication().getContentResolver(), uri, 25 * 1024 * 1024L);
                if (rr.tooLarge) {
                    removeMessageByKey(tempId);
                    errors.postValue(getApplication().getString(R.string.file_too_large));
                    return;
                }
                if (rr.bytes == null) {
                    removeMessageByKey(tempId);
                    errors.postValue(getApplication().getString(R.string.file_read_failed));
                    return;
                }
                byte[] data = rr.bytes;
                if (data == null) {
                    removeMessageByKey(tempId);
                    errors.postValue(getApplication().getString(R.string.file_read_failed));
                    return;
                }
                String base64 = Base64.encodeToString(data, Base64.NO_WRAP);
                JsonObject upload;
                try {
                    ApiClient.ProgressListener progressListener = (written, total) -> {
                        if (total <= 0L) return;
                        int progress = Math.min(100, Math.round((written * 100f) / total));
                        transferUpdates.postValue(new TransferUpdate(tempId, TRANSFER_LOADING, progress, false));
                    };
                    Call call = repo.uploadFileCall(token, fileName, base64, progressListener);
                    uploadCalls.put(tempId, call);
                    try (okhttp3.Response response = call.execute()) {
                        if (!response.isSuccessful()) {
                            throw new IOException("HTTP " + response.code());
                        }
                        String body = response.body() != null ? response.body().string() : "";
                        upload = new com.google.gson.Gson().fromJson(body, JsonObject.class);
                    }
                } catch (Exception e) {
                    Log.w(TAG, "sendFile: upload failed", e);
                    if (e instanceof IOException) {
                        String msg = e.getMessage();
                        if (msg != null && msg.toLowerCase().contains("canceled")) {
                            removeMessageByKey(tempId);
                            transferUpdates.postValue(new TransferUpdate(tempId, TRANSFER_DONE, 0, true));
                            return;
                        }
                    }
                    String msg = e.getMessage();
                    if (msg != null && msg.contains("413")) {
                        removeMessageByKey(tempId);
                        errors.postValue(getApplication().getString(R.string.file_too_large));
                    } else {
                        removeMessageByKey(tempId);
                        errors.postValue(getApplication().getString(R.string.upload_failed));
                    }
                    return;
                } finally {
                    uploadCalls.remove(tempId);
                }
                if (upload == null || !upload.has("success") || !upload.get("success").getAsBoolean()) {
                    String msg = upload != null && upload.has("message") ? upload.get("message").getAsString() : null;
                    removeMessageByKey(tempId);
                    errors.postValue(msg != null && !msg.isEmpty()
                            ? msg
                            : getApplication().getString(R.string.upload_failed));
                    return;
                }
                String filePath = upload.has("file_path") ? upload.get("file_path").getAsString() : null;
                String serverName = upload.has("file_name") ? upload.get("file_name").getAsString() : fileName;
                Long serverSize = upload.has("file_size") ? upload.get("file_size").getAsLong() : (fileSize > 0 ? fileSize : null);
                MessageEntity m;
                try {
                    if (isGroup) {
                        m = repo.sendGroupMessage(token, chatId, content, messageType, null, selfId, selfName, filePath, serverName, serverSize, ttlSeconds);
                    } else if (isChannel) {
                        m = repo.sendChannelMessage(token, chatId, content, messageType, null, selfId, selfName, filePath, serverName, serverSize, ttlSeconds);
                    } else {
                        m = repo.sendMessage(token, chatId, content, messageType, null, selfId, filePath, serverName, serverSize, ttlSeconds);
                    }
                } catch (Exception e) {
                    Log.w(TAG, "sendFile: send message failed", e);
                    String msg = e.getMessage();
                    if (msg != null && !msg.isEmpty() && !msg.startsWith("HTTP")) {
                        removeMessageByKey(tempId);
                        errors.postValue(msg);
                    } else {
                        removeMessageByKey(tempId);
                        errors.postValue(getApplication().getString(R.string.file_send_failed));
                    }
                    return;
                }
                replaceMessageByKey(tempId, m);
                String preview = useSpoiler
                        ? getApplication().getString(R.string.chat_preview_spoiler)
                        : (messageType.equals("image")
                        ? getApplication().getString(R.string.chat_preview_photo)
                        : getApplication().getString(R.string.chat_preview_file));
                updateChatPreview(preview);
                transferUpdates.postValue(new TransferUpdate(tempId, TRANSFER_DONE, 100, true));
            } catch (Exception e) {
                Log.w(TAG, "sendFile: unexpected error", e);
                removeMessageByKey(tempId);
                errors.postValue(getApplication().getString(R.string.file_send_failed));
            }
        });
    }

    public void sendVoice(Uri uri, String mimeType) {
        sendVoice(uri, mimeType, 0L);
    }

    public void sendVoice(Uri uri, String mimeType, long ttlSeconds) {
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null || uri == null) return;
        io.execute(() -> {
            if (isPanicMode()) {
                MessageEntity m = new MessageEntity();
                m.id = "decoy-" + UUID.randomUUID().toString();
                m.tempId = m.id;
                m.chatId = chatId;
                m.senderId = selfId;
                m.senderName = selfName;
                m.content = getApplication().getString(R.string.chat_preview_voice);
                m.messageType = "voice";
                m.filePath = uri.toString();
                m.fileName = "voice";
                m.createdAtMillis = System.currentTimeMillis();
                m.createdAt = nowIso();
                m.status = "sent";
                m.ttlSeconds = Math.max(0L, ttlSeconds);
                if (db != null) db.messageDao().upsert(m);
                List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
                current.add(m);
                List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
                updateChatPreview(getApplication().getString(R.string.chat_preview_voice));
                return;
            }
            try {
                ReadBytesResult rr = readBytes(getApplication().getContentResolver(), uri, 15 * 1024 * 1024L);
                if (rr.tooLarge) {
                    errors.postValue(getApplication().getString(R.string.voice_too_large));
                    return;
                }
                if (rr.bytes == null) {
                    errors.postValue(getApplication().getString(R.string.voice_read_failed));
                    return;
                }
                byte[] data = rr.bytes;
                if (data == null) {
                    errors.postValue(getApplication().getString(R.string.voice_read_failed));
                    return;
                }
                String base64 = Base64.encodeToString(data, Base64.NO_WRAP);
                JsonObject upload = repo.uploadVoice(token, base64, mimeType);
                if (!upload.has("success") || !upload.get("success").getAsBoolean()) {
                    errors.postValue(getApplication().getString(R.string.voice_upload_failed));
                    return;
                }
                String filePath = upload.has("file_path") ? upload.get("file_path").getAsString() : null;
                String serverName = upload.has("file_name") ? upload.get("file_name").getAsString() : "voice";
                Long serverSize = upload.has("file_size") ? upload.get("file_size").getAsLong() : null;
                MessageEntity m;
                if (isGroup) {
                    m = repo.sendGroupMessage(token, chatId, getApplication().getString(R.string.chat_preview_voice), "voice", null, selfId, selfName, filePath, serverName, serverSize, ttlSeconds);
                } else if (isChannel) {
                    m = repo.sendChannelMessage(token, chatId, getApplication().getString(R.string.chat_preview_voice), "voice", null, selfId, selfName, filePath, serverName, serverSize, ttlSeconds);
                } else {
                    m = repo.sendMessage(token, chatId, getApplication().getString(R.string.chat_preview_voice), "voice", null, selfId, filePath, serverName, serverSize, ttlSeconds);
                }
                List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
                current.add(m);
                List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
                updateChatPreview(getApplication().getString(R.string.chat_preview_voice));
            } catch (Exception e) {
                errors.postValue(getApplication().getString(R.string.voice_send_failed));
            }
        });
    }

    public void startSocket() {
        if (isPanicMode()) return;
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || token.isEmpty()) return;
        ensureSocket();
    }

    public void cancelUpload(String messageKey) {
        if (messageKey == null || messageKey.isEmpty()) return;
        Call call = uploadCalls.remove(messageKey);
        if (call != null) {
            call.cancel();
        }
        removeMessageByKey(messageKey);
        transferUpdates.postValue(new TransferUpdate(messageKey, TRANSFER_DONE, 0, true));
    }

    private void removeMessageByKey(String key) {
        List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
        boolean changed = false;
        for (int i = current.size() - 1; i >= 0; i--) {
            MessageEntity m = current.get(i);
            String match = getMessageKey(m);
            if (key.equals(match)) {
                current.remove(i);
                changed = true;
            }
        }
        if (changed) {
            messages.postValue(current);
        }
    }

    private void replaceMessageByKey(String key, MessageEntity replacement) {
        if (replacement == null) return;
        List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
        boolean replaced = false;
        for (int i = 0; i < current.size(); i++) {
            MessageEntity m = current.get(i);
            String match = getMessageKey(m);
            if (key.equals(match)) {
                current.set(i, replacement);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            current.add(replacement);
        }
        messages.postValue(current);
    }

    private static String getMessageKey(MessageEntity m) {
        if (m == null) return null;
        if (m.id != null && !m.id.isEmpty()) return m.id;
        if (m.tempId != null && !m.tempId.isEmpty()) return m.tempId;
        return null;
    }

    private static String nowIso() {
        java.text.SimpleDateFormat fmt = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", java.util.Locale.US);
        return fmt.format(new java.util.Date());
    }

    private void ensureSocket() {
        if (isPanicMode()) return;
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || token.isEmpty()) return;
        if (socket != null && token.equals(currentToken)) return;
        closeSocket();
        currentToken = token;
        PushTokenManager.CachedToken cached = PushTokenManager.isPushEnabled(getApplication())
                ? PushTokenManager.getCachedTokenInfo(getApplication())
                : null;
        String pushToken = cached != null ? cached.token : null;
        String platform = cached != null ? cached.platform : null;
        socket = new SocketManager(token, pushToken, platform);
        socket.setListener(this);
        socket.connect();
    }

    private void closeSocket() {
        if (socket != null) {
            socket.close();
            socket = null;
        }
    }

    private void scheduleReconnect() {
        if (reconnectRunnable != null) return;
        reconnectRunnable = () -> {
            reconnectRunnable = null;
            ensureSocket();
        };
        handler.postDelayed(reconnectRunnable, reconnectDelayMs);
        reconnectDelayMs = Math.min(reconnectDelayMs * 2L, RECONNECT_MAX_MS);
    }

    private void cancelReconnect() {
        if (reconnectRunnable != null) {
            handler.removeCallbacks(reconnectRunnable);
            reconnectRunnable = null;
        }
    }

    public static class TransferUpdate {
        public final String messageKey;
        public final int state;
        public final int progress;
        public final boolean clearWhenDone;

        public TransferUpdate(String messageKey, int state, int progress, boolean clearWhenDone) {
            this.messageKey = messageKey;
            this.state = state;
            this.progress = progress;
            this.clearWhenDone = clearWhenDone;
        }
    }

    public static class TypingInfo {
        public final int count;
        public final List<String> names;

        public TypingInfo(int count, List<String> names) {
            this.count = count;
            this.names = names != null ? names : new ArrayList<>();
        }
    }

    public void stopSocket() {
        cancelReconnect();
        closeSocket();
        clearTypingState();
    }

    @Override
    public void onAuthed() {
        reconnectDelayMs = RECONNECT_MIN_MS;
        cancelReconnect();
    }

    @Override
    public void onMessage(JsonObject obj) {
        if (obj == null) return;
        String type = obj.has("type") ? obj.get("type").getAsString() : "";
        if ("call_offer".equals(type)) {
            handleIncomingCall(obj);
            return;
        }
        if ("message_delivered".equals(type) || "message_read".equals(type)) {
            String messageId = obj.has("message_id") ? obj.get("message_id").getAsString() : null;
            String chatHint = obj.has("chat_id") ? obj.get("chat_id").getAsString() : null;
            if (messageId != null && !messageId.isEmpty()) {
                if (chatHint == null || chatId == null || chatId.equals(chatHint)) {
                    updateMessageStatus(messageId, "message_read".equals(type) ? "read" : "delivered");
                }
            }
            return;
        }
        if ("typing".equals(type)) {
            handleTypingEvent(obj);
            return;
        }
        if ("channel_new_message".equals(type)) {
            String channelId = obj.has("chat_id") ? obj.get("chat_id").getAsString() : null;
            if (isChannel && chatId != null && chatId.equals(channelId)) {
                refreshHistory();
            }
            touchChat(channelId, getApplication().getString(R.string.chat_preview_new_messages));
            return;
        }
        if ("group_message".equals(type)) {
            String groupId = obj.has("chat_id") ? obj.get("chat_id").getAsString() : null;
            if (isGroup && chatId != null && chatId.equals(groupId)) {
                refreshHistory();
            }
            touchChat(groupId, getApplication().getString(R.string.chat_preview_new_messages));
            return;
        }
        if ("new_message".equals(type)) {
            JsonObject msg = obj.has("message") ? obj.get("message").getAsJsonObject() : obj;
            String receiverId = msg.has("receiver_id") ? msg.get("receiver_id").getAsString() : null;
            String senderId = msg.has("sender_id") ? msg.get("sender_id").getAsString() : null;
            if (chatId == null || receiverId == null) return;
            // фильтр по текущему чату (учитываем, что это диалог 1:1)
            if (!receiverId.equals(chatId) && !senderId.equals(chatId)) return;
            MessageEntity m = new MessageEntity();
            m.id = msg.has("id") ? msg.get("id").getAsString() : msg.has("message_id") ? msg.get("message_id").getAsString() : "msg-" + System.currentTimeMillis();
            m.chatId = chatId;
            m.senderId = senderId;
            m.senderName = msg.has("sender_username") ? msg.get("sender_username").getAsString() : null;
            m.content = msg.has("content") ? msg.get("content").getAsString() : "";
            m.messageType = msg.has("message_type") ? msg.get("message_type").getAsString() : "text";
            m.filePath = msg.has("file_path") ? msg.get("file_path").getAsString() : null;
            m.fileName = msg.has("file_name") ? msg.get("file_name").getAsString() : null;
            m.fileSize = msg.has("file_size") ? msg.get("file_size").getAsLong() : null;
            if (msg.has("ttl_seconds")) {
                try {
                    m.ttlSeconds = msg.get("ttl_seconds").getAsLong();
                } catch (Exception ignored) {
                    m.ttlSeconds = 0L;
                }
            }
            if (msg.has("read_timestamp")) {
                try {
                    m.readTimestamp = parseEpoch(msg.get("read_timestamp").getAsString());
                } catch (Exception ignored) {
                    m.readTimestamp = 0L;
                }
            }
            String createdAt = msg.has("created_at") ? msg.get("created_at").getAsString() : null;
            if (createdAt == null || createdAt.isEmpty()) {
                createdAt = nowIso();
            }
            m.createdAt = createdAt;
            m.createdAtMillis = parseEpoch(createdAt);
            if (m.createdAtMillis <= 0L) {
                m.createdAtMillis = System.currentTimeMillis();
            }
            m.status = "sent";
            boolean isIncoming = senderId != null && !senderId.equals(selfId);
            if (isIncoming && !isGroup && !isChannel) {
                sendDeliveryReceipt(m.id);
                sendReadReceipt(m.id);
                if (m.ttlSeconds > 0L && m.readTimestamp <= 0L) {
                    m.readTimestamp = System.currentTimeMillis();
                    m.status = "read";
                }
            }
            io.execute(() -> {
                if (db != null) db.messageDao().upsert(m);
                List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
                current.add(m);
                List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
                List<MessageEntity> pruned = filterExpiredMessages(processed);
                messages.postValue(pruned);
                updatePreviewFromHistory(pruned);
            });
        }
    }

    @Override
    public void onClosed() {
        scheduleReconnect();
    }

    @Override
    public void onFailure(Throwable t) {
        errors.postValue(getApplication().getString(R.string.ws_error));
        scheduleReconnect();
    }

    private void sendReceiptsForMessages(List<MessageEntity> list) {
        if (list == null || isGroup || isChannel) return;
        for (MessageEntity m : list) {
            if (m == null || m.id == null) continue;
            if (m.senderId != null && m.senderId.equals(selfId)) continue;
            sendDeliveryReceipt(m.id);
            sendReadReceipt(m.id);
            if (m.ttlSeconds > 0L && m.readTimestamp <= 0L) {
                markMessageReadLocal(m);
            }
        }
    }

    public void onTypingInput(String value) {
        if (isPanicMode() || isSaved || chatId == null || chatId.isEmpty()) return;
        String trimmed = value != null ? value.trim() : "";
        if (trimmed.isEmpty()) {
            stopTyping();
            return;
        }
        long now = System.currentTimeMillis();
        if (!typingActive || now - typingLastSentAt > TYPING_SEND_THROTTLE_MS) {
            sendTypingSignal(true);
            typingActive = true;
            typingLastSentAt = now;
        }
        if (typingIdleRunnable != null) {
            handler.removeCallbacks(typingIdleRunnable);
        }
        typingIdleRunnable = () -> {
            sendTypingSignal(false);
            typingActive = false;
            typingIdleRunnable = null;
        };
        handler.postDelayed(typingIdleRunnable, TYPING_IDLE_TIMEOUT_MS);
    }

    public void stopTyping() {
        if (typingIdleRunnable != null) {
            handler.removeCallbacks(typingIdleRunnable);
            typingIdleRunnable = null;
        }
        if (!typingActive) {
            typingLastSentAt = 0L;
            return;
        }
        sendTypingSignal(false);
        typingActive = false;
        typingLastSentAt = 0L;
    }

    private void sendTypingSignal(boolean isTyping) {
        if (isPanicMode() || isSaved || chatId == null || chatId.isEmpty()) return;
        if (socket == null || !socket.isAuthed()) return;
        JsonObject payload = new JsonObject();
        payload.addProperty("type", "typing");
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token != null && !token.isEmpty()) {
            payload.addProperty("token", token);
        }
        payload.addProperty("chat_type", expectedChatType());
        payload.addProperty("chat_id", chatId);
        payload.addProperty("is_typing", isTyping ? "1" : "0");
        socket.send(payload);
    }

    private void handleTypingEvent(JsonObject obj) {
        if (obj == null || isSaved || chatId == null || chatId.isEmpty()) return;
        String eventChatType = normalizeChatType(optString(obj, "chat_type"));
        if (eventChatType.isEmpty()) {
            eventChatType = normalizeChatType(optString(obj, "chatType"));
        }
        if (eventChatType.isEmpty()) {
            eventChatType = "chat";
        }
        String eventChatId = optString(obj, "chat_id");
        if (eventChatId.isEmpty()) {
            eventChatId = optString(obj, "chatId");
        }
        if (eventChatId.isEmpty() || !eventChatId.equals(chatId)) return;
        if (!expectedChatType().equals(eventChatType)) return;
        String fromId = optString(obj, "from_user_id");
        if (fromId.isEmpty()) {
            fromId = optString(obj, "user_id");
        }
        if (!fromId.isEmpty() && fromId.equals(selfId)) return;
        boolean isTyping = !obj.has("is_typing") || parseTypingFlag(obj.get("is_typing"));
        String name = optString(obj, "from_username");
        if (name.isEmpty()) {
            name = optString(obj, "username");
        }
        String key = !fromId.isEmpty() ? fromId : name;
        if (key == null || key.isEmpty()) {
            key = "unknown";
        }
        final String finalKey = key;
        final String finalName = name != null ? name : "";
        handler.post(() -> applyTypingEvent(finalKey, finalName, isTyping));
    }

    private void applyTypingEvent(String key, String name, boolean isTyping) {
        if (key == null || key.isEmpty()) return;
        if (!isTyping) {
            removeTypingUser(key);
            return;
        }
        typingUsers.put(key, name != null ? name : "");
        Runnable existing = typingTimeouts.remove(key);
        if (existing != null) {
            handler.removeCallbacks(existing);
        }
        Runnable timeout = () -> {
            typingUsers.remove(key);
            typingTimeouts.remove(key);
            updateTypingInfo();
        };
        typingTimeouts.put(key, timeout);
        handler.postDelayed(timeout, TYPING_DISPLAY_TIMEOUT_MS);
        updateTypingInfo();
    }

    private void removeTypingUser(String key) {
        Runnable existing = typingTimeouts.remove(key);
        if (existing != null) {
            handler.removeCallbacks(existing);
        }
        typingUsers.remove(key);
        updateTypingInfo();
    }

    private void updateTypingInfo() {
        List<String> names = new ArrayList<>();
        for (String name : typingUsers.values()) {
            if (name != null && !name.trim().isEmpty()) {
                names.add(name);
            }
        }
        typingInfo.setValue(new TypingInfo(typingUsers.size(), names));
    }

    private void clearTypingState() {
        if (typingIdleRunnable != null) {
            handler.removeCallbacks(typingIdleRunnable);
            typingIdleRunnable = null;
        }
        for (Runnable timeout : typingTimeouts.values()) {
            handler.removeCallbacks(timeout);
        }
        typingTimeouts.clear();
        typingUsers.clear();
        typingActive = false;
        typingLastSentAt = 0L;
        typingInfo.setValue(new TypingInfo(0, new ArrayList<>()));
    }

    private String expectedChatType() {
        if (isGroup) return "group";
        if (isChannel) return "channel";
        return "chat";
    }

    private String normalizeChatType(String value) {
        if (value == null || value.isEmpty()) return "chat";
        if ("direct".equalsIgnoreCase(value) || "dm".equalsIgnoreCase(value)) return "chat";
        return value;
    }

    private boolean parseTypingFlag(JsonElement value) {
        if (value == null || value.isJsonNull()) return false;
        try {
            String raw = value.getAsString();
            if (raw == null) return false;
            String normalized = raw.trim().toLowerCase();
            return "1".equals(normalized) || "true".equals(normalized) || "yes".equals(normalized);
        } catch (Exception ignored) {
            return false;
        }
    }

    private String optString(JsonObject obj, String key) {
        if (obj == null || key == null || !obj.has(key) || obj.get(key).isJsonNull()) return "";
        try {
            return obj.get(key).getAsString();
        } catch (Exception ignored) {
            return "";
        }
    }

    private void sendDeliveryReceipt(String messageId) {
        if (messageId == null || messageId.isEmpty()) return;
        if (deliveredSent.contains(messageId)) return;
        deliveredSent.add(messageId);
        sendReceipt("message_delivered", messageId);
    }

    private void sendReadReceipt(String messageId) {
        SharedPreferences prefs = getApplication().getSharedPreferences("xipher_prefs", Context.MODE_PRIVATE);
        if (prefs.getBoolean("pref_incognito_read", false)) {
            return;
        }
        if (messageId == null || messageId.isEmpty()) return;
        if (readSent.contains(messageId)) return;
        readSent.add(messageId);
        sendReceipt("message_read", messageId);
    }

    private void sendReceipt(String type, String messageId) {
        if (socket == null || !socket.isAuthed()) return;
        JsonObject payload = new JsonObject();
        payload.addProperty("type", type);
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token != null && !token.isEmpty()) {
            payload.addProperty("token", token);
        }
        payload.addProperty("message_id", messageId);
        socket.send(payload);
    }

    private void updateMessageStatus(String messageId, String status) {
        if (messageId == null || status == null || status.isEmpty()) return;
        io.execute(() -> {
            List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
            boolean updated = false;
            long readTimestamp = 0L;
            for (MessageEntity m : current) {
                if (m == null || m.id == null) continue;
                if (!messageId.equals(m.id)) continue;
                if ("read".equals(status) && m.ttlSeconds > 0L && m.readTimestamp <= 0L) {
                    readTimestamp = System.currentTimeMillis();
                    m.readTimestamp = readTimestamp;
                    updated = true;
                }
                if (statusRank(status) > statusRank(m.status)) {
                    m.status = status;
                    updated = true;
                }
                break;
            }
            if (readTimestamp > 0L) {
                if (db != null) db.messageDao().updateStatusAndReadTimestamp(messageId, status, readTimestamp);
            } else {
                if (db != null) db.messageDao().updateStatus(messageId, status);
            }
            if (updated) {
                List<MessageEntity> pruned = filterExpiredMessages(current);
                messages.postValue(pruned);
                updatePreviewFromHistory(pruned);
            }
        });
    }

    public void pruneExpiredMessages() {
        io.execute(() -> {
            List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
            if (current.isEmpty()) return;
            List<MessageEntity> pruned = filterExpiredMessages(current);
            if (pruned.size() != current.size()) {
                messages.postValue(pruned);
                updatePreviewFromHistory(pruned);
            }
        });
    }

    public void expireMessage(MessageEntity message) {
        if (message == null) return;
        String id = message.id;
        String tempId = message.tempId;
        io.execute(() -> {
            if (id != null && db != null) {
                db.messageDao().deleteById(id);
            }
            List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
            boolean removed = current.removeIf(m -> m != null && ((id != null && id.equals(m.id)) || (id == null && tempId != null && tempId.equals(m.tempId))));
            if (removed) {
                messages.postValue(current);
                updatePreviewFromHistory(current);
            }
        });
    }

    private void markMessageReadLocal(MessageEntity message) {
        if (message == null || message.id == null) return;
        if (message.ttlSeconds <= 0L || message.readTimestamp > 0L) return;
        long now = System.currentTimeMillis();
        message.readTimestamp = now;
        if (statusRank(message.status) < statusRank("read")) {
            message.status = "read";
        }
        if (db != null) db.messageDao().updateStatusAndReadTimestamp(message.id, message.status, now);
        List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
        boolean updated = false;
        for (MessageEntity m : current) {
            if (m == null || m.id == null) continue;
            if (!message.id.equals(m.id)) continue;
            m.readTimestamp = now;
            if (statusRank(m.status) < statusRank("read")) {
                m.status = "read";
            }
            updated = true;
            break;
        }
        if (updated) {
            List<MessageEntity> pruned = filterExpiredMessages(current);
            messages.postValue(pruned);
            updatePreviewFromHistory(pruned);
        }
    }

    private List<MessageEntity> filterExpiredMessages(List<MessageEntity> list) {
        if (list == null) return new ArrayList<>();
        if (list.isEmpty()) return list;
        long now = System.currentTimeMillis();
        List<MessageEntity> result = new ArrayList<>(list);
        List<String> expiredIds = new ArrayList<>();
        result.removeIf(m -> {
            if (m == null) return true; // Remove null entries
            if (!isExpired(m, now)) return false;
            if (m.id != null) {
                expiredIds.add(m.id);
            }
            return true;
        });
        if (!expiredIds.isEmpty() && db != null) {
            try {
                if (db.messageDao() != null) {
                    db.messageDao().deleteByIds(expiredIds);
                }
            } catch (Exception ignored) {}
        }
        return result;
    }

    private boolean isExpired(MessageEntity message, long now) {
        if (message == null) return false;
        if (message.ttlSeconds <= 0L || message.readTimestamp <= 0L) return false;
        long expiresAt = message.readTimestamp + (message.ttlSeconds * 1000L);
        return expiresAt <= now;
    }

    private static int statusRank(String status) {
        if ("read".equals(status)) return 3;
        if ("delivered".equals(status)) return 2;
        if ("sent".equals(status)) return 1;
        return 0;
    }

    private static long parseEpoch(String value) {
        if (value == null || value.isEmpty()) return 0L;
        String v = value.trim();
        if (v.matches("^\\d+$")) {
            try {
                long epoch = Long.parseLong(v);
                if (epoch < 100000000000L) {
                    epoch *= 1000L;
                }
                return epoch;
            } catch (Exception ignored) {
            }
        }
        String[] patterns = new String[] {
                "yyyy-MM-dd'T'HH:mm:ss",
                "yyyy-MM-dd HH:mm:ss",
                "yyyy-MM-dd'T'HH:mm:ss'Z'",
                "yyyy-MM-dd'T'HH:mm:ss.SSS'Z'",
                "yyyy-MM-dd'T'HH:mm:ssX",
                "yyyy-MM-dd'T'HH:mm:ss.SSSX"
        };
        for (String pattern : patterns) {
            try {
                java.text.SimpleDateFormat fmt = new java.text.SimpleDateFormat(pattern, java.util.Locale.US);
                if (pattern.endsWith("'Z'")) {
                    fmt.setTimeZone(java.util.TimeZone.getTimeZone("UTC"));
                }
                java.util.Date date = fmt.parse(v);
                if (date != null) return date.getTime();
            } catch (Exception ignored) {
            }
        }
        return 0L;
    }

    private void handleIncomingCall(JsonObject obj) {
        String fromId = obj.has("from_user_id") ? obj.get("from_user_id").getAsString() : null;
        if (fromId == null || fromId.isEmpty()) return;
        String fromName = obj.has("from_username") ? obj.get("from_username").getAsString() : null;
        CallActivity.startIncoming(getApplication(), fromId, fromName);
    }

    private void updateChatPreview(String preview) {
        touchChat(chatId, resolvePreview(preview));
    }

    private void touchChat(String id, String preview) {
        if (id == null || db == null) return;
        String safePreview = preview != null ? preview : "";
        long now = System.currentTimeMillis();
        io.execute(() -> db.chatDao().updatePreview(id, safePreview, now));
    }

    private void updatePreviewFromHistory(List<MessageEntity> list) {
        if (list == null || list.isEmpty()) return;
        MessageEntity last = list.get(list.size() - 1);
        if (last == null) return;
        String preview = buildPreviewForMessage(last);
        updateChatPreview(preview);
    }

    private void scheduleSendFailureCheck(String content, String messageType, long attemptAtMillis) {
        io.execute(() -> {
            if (recoverSendFailure(content, messageType, attemptAtMillis)) {
                return;
            }
            errors.postValue(getApplication().getString(R.string.send_failed));
        });
    }

    private boolean recoverSendFailure(String content, String messageType, long attemptAtMillis) {
        if (chatId == null || db == null) return false;
        long attemptAt = attemptAtMillis > 0L ? attemptAtMillis : System.currentTimeMillis();
        List<MessageEntity> cached = db.messageDao().getByChat(chatId);
        List<MessageEntity> cachedPrepared = prepareMessagesForRecovery(cached);
        if (findLatestSelfMessage(cachedPrepared, content, messageType, attemptAt) != null) {
            messages.postValue(cachedPrepared);
            updatePreviewFromHistory(cachedPrepared);
            return true;
        }
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null) return false;
        try {
            List<MessageEntity> list;
            if (isGroup) {
                list = repo.syncGroupMessages(token, chatId);
            } else if (isChannel) {
                list = repo.syncChannelMessages(token, chatId);
            } else {
                list = repo.syncMessages(token, chatId);
            }
            List<MessageEntity> prepared = prepareMessagesForRecovery(list);
            if (findLatestSelfMessage(prepared, content, messageType, attemptAt) == null) {
                return false;
            }
            messages.postValue(prepared);
            updatePreviewFromHistory(prepared);
            return true;
        } catch (Exception ignored) {
            return false;
        }
    }

    private List<MessageEntity> prepareMessagesForRecovery(List<MessageEntity> list) {
        List<MessageEntity> sorted = sortMessages(list);
        List<MessageEntity> processed = processChecklistMessages(sorted);
        return filterExpiredMessages(processed);
    }

    private MessageEntity findLatestSelfMessage(List<MessageEntity> list, String content, String messageType, long attemptAtMillis) {
        if (list == null || list.isEmpty()) return null;
        String expectedType = normalizeMessageType(messageType);
        long windowStart = attemptAtMillis - SEND_RECOVERY_PAST_GRACE_MS;
        long windowEnd = attemptAtMillis + SEND_RECOVERY_WINDOW_MS;
        for (int i = list.size() - 1; i >= 0; i--) {
            MessageEntity m = list.get(i);
            if (m == null) continue;
            if (selfId != null && m.senderId != null && !selfId.equals(m.senderId)) continue;
            if (!contentMatchesForRecovery(content, m.content)) continue;
            String actualType = normalizeMessageType(m.messageType);
            if (!expectedType.equals(actualType)) continue;
            if (attemptAtMillis > 0L) {
                long createdAt = m.createdAtMillis;
                if (createdAt <= 0L || createdAt < windowStart || createdAt > windowEnd) continue;
            }
            return m;
        }
        return null;
    }

    private boolean contentMatchesForRecovery(String sentContent, String storedContent) {
        if (sentContent == null) return true;
        String safeStored = storedContent != null ? storedContent : "";
        if (sentContent.equals(safeStored)) return true;
        return ChecklistCodec.isChecklistContent(sentContent) && ChecklistCodec.isChecklistContent(safeStored);
    }

    private String normalizeMessageType(String type) {
        return type == null || type.isEmpty() ? "text" : type;
    }

    private boolean shouldUpdateMessages(List<MessageEntity> current, List<MessageEntity> remote) {
        if (remote == null) return false;
        if (current == null || current.isEmpty()) return !remote.isEmpty();
        if (remote.size() != current.size()) return true;
        Set<String> currentIds = new HashSet<>();
        for (MessageEntity m : current) {
            if (m != null && m.id != null) currentIds.add(m.id);
        }
        for (MessageEntity m : remote) {
            if (m != null && m.id != null && !currentIds.contains(m.id)) return true;
        }
        MessageEntity lastRemote = remote.get(remote.size() - 1);
        MessageEntity lastCurrent = current.get(current.size() - 1);
        if (lastRemote == null || lastRemote.id == null) return false;
        String lastCurrentId = lastCurrent != null ? lastCurrent.id : null;
        return !lastRemote.id.equals(lastCurrentId);
    }

    private List<MessageEntity> sortMessages(List<MessageEntity> list) {
        if (list == null) return new ArrayList<>();
        // Remove null elements to prevent NPE in sort
        list.removeIf(m -> m == null);
        boolean updated = false;
        long fallbackBase = System.currentTimeMillis();
        long fallbackOffset = 0L;
        for (MessageEntity m : list) {
            if (m == null) continue;
            if (m.createdAtMillis <= 0L) {
                long parsed = parseEpoch(m.createdAt);
                if (parsed > 0L) {
                    m.createdAtMillis = parsed;
                } else {
                    m.createdAtMillis = fallbackBase + fallbackOffset;
                    fallbackOffset += 1L;
                }
                updated = true;
            }
        }
        if (updated && db != null) {
            final List<MessageEntity> toSave = new ArrayList<>(list);
            io.execute(() -> {
                try {
                    if (db != null && db.messageDao() != null) {
                        db.messageDao().upsertAll(toSave);
                    }
                } catch (Exception ignored) {}
            });
        }
        try {
            list.sort((a, b) -> Long.compare(resolveTimestamp(a), resolveTimestamp(b)));
        } catch (Exception ignored) {}
        return list;
    }

    private long resolveTimestamp(MessageEntity m) {
        if (m == null) return 0L;
        if (m.createdAtMillis > 0L) return m.createdAtMillis;
        long parsed = parseEpoch(m.createdAt);
        return parsed > 0L ? parsed : 0L;
    }

    private boolean isTextMessage(String type, String content) {
        String safeContent = content != null ? content : "";
        if ("voice".equals(type) || "file".equals(type) || "image".equals(type)
                || "location".equals(type) || "live_location".equals(type)) {
            return false;
        }
        if (SpoilerCodec.isSpoilerContent(safeContent)) return true;
        if (ChecklistCodec.parseChecklist(safeContent) != null) return false;
        return true;
    }

    private String buildForwardText(MessageEntity message) {
        String from = message != null ? message.senderName : null;
        if (from == null || from.isEmpty()) {
            if (message != null && message.senderId != null && message.senderId.equals(selfId) && selfName != null) {
                from = selfName;
            } else if (message != null && message.senderId != null) {
                from = message.senderId;
            } else {
                from = getApplication().getString(R.string.forward_unknown);
            }
        }
        String content = SpoilerCodec.strip(message != null ? message.content : "");
        return getApplication().getString(R.string.forwarded_prefix, from, content);
    }

    private String buildPreviewForMessage(MessageEntity message) {
        if (message == null) return "";
        String content = message.content != null ? message.content : "";
        if (SpoilerCodec.isSpoilerContent(content)) {
            return getApplication().getString(R.string.chat_preview_spoiler);
        }
        String displayContent = SpoilerCodec.strip(content);
        if (ChecklistCodec.isChecklistContent(displayContent)) {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(displayContent);
            if (payload != null && payload.title != null && !payload.title.isEmpty()) {
                return getApplication().getString(R.string.chat_preview_checklist) + ": " + payload.title;
            }
            return getApplication().getString(R.string.chat_preview_checklist);
        }
        if ("voice".equals(message.messageType)) {
            return getApplication().getString(R.string.chat_preview_voice);
        }
        if ("image".equals(message.messageType)) {
            return getApplication().getString(R.string.chat_preview_photo);
        }
        if ("file".equals(message.messageType)) {
            return getApplication().getString(R.string.chat_preview_file);
        }
        if ("location".equals(message.messageType)) {
            return getApplication().getString(R.string.chat_preview_location);
        }
        if ("live_location".equals(message.messageType)) {
            return getApplication().getString(R.string.chat_preview_live_location);
        }
        return displayContent;
    }

    private String resolvePreview(String raw) {
        if (raw == null) return "";
        if (SpoilerCodec.isSpoilerContent(raw)) {
            return getApplication().getString(R.string.chat_preview_spoiler);
        }
        String display = SpoilerCodec.strip(raw);
        if (ChecklistCodec.isChecklistContent(display)) {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(display);
            if (payload != null && payload.title != null && !payload.title.isEmpty()) {
                return getApplication().getString(R.string.chat_preview_checklist) + ": " + payload.title;
            }
            return getApplication().getString(R.string.chat_preview_checklist);
        }
        return display;
    }

    private List<MessageEntity> processChecklistMessages(List<MessageEntity> list) {
        if (list == null) return new ArrayList<>();
        Map<String, MessageEntity> checklistById = new HashMap<>();
        Map<String, ChecklistPayload> payloadById = new HashMap<>();
        Map<String, List<ChecklistUpdatePayload>> pending = new HashMap<>();
        List<MessageEntity> result = new ArrayList<>();
        List<MessageEntity> toUpsert = new ArrayList<>();

        for (MessageEntity m : list) {
            if (m == null) continue;
            String content = m.content != null ? m.content : "";
            if (SpoilerCodec.isSpoilerContent(content)) {
                result.add(m);
                continue;
            }
            if (ChecklistCodec.isChecklistContent(content)) {
                ChecklistPayload payload = ChecklistCodec.parseChecklist(content);
                if (payload != null) {
                    if (payload.id == null || payload.id.isEmpty()) {
                        payload.id = m.id != null ? m.id : UUID.randomUUID().toString();
                        m.content = ChecklistCodec.encodeChecklist(payload);
                        toUpsert.add(m);
                    }
                    checklistById.put(payload.id, m);
                    payloadById.put(payload.id, payload);
                    List<ChecklistUpdatePayload> updates = pending.remove(payload.id);
                    if (updates != null) {
                        boolean changed = false;
                        for (ChecklistUpdatePayload update : updates) {
                            changed |= applyChecklistUpdate(payload, update);
                        }
                        if (changed) {
                            m.content = ChecklistCodec.encodeChecklist(payload);
                            toUpsert.add(m);
                        }
                    }
                }
                result.add(m);
                continue;
            }
            if (ChecklistCodec.isChecklistUpdateContent(content)) {
                ChecklistUpdatePayload update = ChecklistCodec.parseUpdate(content);
                if (update == null || update.checklistId == null || update.checklistId.isEmpty()) {
                    result.add(m);
                    continue;
                }
                ChecklistPayload payload = payloadById.get(update.checklistId);
                MessageEntity base = checklistById.get(update.checklistId);
                if (payload != null && base != null) {
                    if (applyChecklistUpdate(payload, update)) {
                        base.content = ChecklistCodec.encodeChecklist(payload);
                        toUpsert.add(base);
                    }
                } else {
                    pending.computeIfAbsent(update.checklistId, k -> new ArrayList<>()).add(update);
                }
                continue;
            }
            result.add(m);
        }

        if (!toUpsert.isEmpty() && db != null) {
            io.execute(() -> db.messageDao().upsertAll(toUpsert));
        }
        return result;
    }

    private boolean applyChecklistUpdate(ChecklistPayload payload, ChecklistUpdatePayload update) {
        if (payload == null || update == null) return false;
        boolean changed = false;
        if (payload.items == null) {
            payload.items = new ArrayList<>();
        }
        if (update.updates != null) {
            for (ChecklistItemUpdate itemUpdate : update.updates) {
                if (itemUpdate == null || itemUpdate.id == null) continue;
                for (ChecklistItem item : payload.items) {
                    if (item == null || item.id == null) continue;
                    if (item.id.equals(itemUpdate.id) && item.done != itemUpdate.done) {
                        item.done = itemUpdate.done;
                        changed = true;
                    }
                }
            }
        }
        if (update.added != null) {
            for (ChecklistItem added : update.added) {
                if (added == null || added.id == null) continue;
                boolean exists = false;
                for (ChecklistItem item : payload.items) {
                    if (item != null && added.id.equals(item.id)) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    payload.items.add(added);
                    changed = true;
                }
            }
        }
        return changed;
    }

    public void toggleChecklistItem(MessageEntity message, String checklistId, String itemId, boolean done) {
        if (message == null || checklistId == null || itemId == null) return;
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null) return;
        io.execute(() -> {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(message.content);
            if (payload == null || payload.items == null) return;
            boolean canToggle = payload.othersCanMark || (message.senderId != null && message.senderId.equals(selfId));
            if (!canToggle) return;
            boolean changed = false;
            for (ChecklistItem item : payload.items) {
                if (item == null || item.id == null) continue;
                if (item.id.equals(itemId) && item.done != done) {
                    item.done = done;
                    changed = true;
                    break;
                }
            }
            if (!changed) return;
            String updatedContent = ChecklistCodec.encodeChecklist(payload);
            if (updatedContent == null) return;
            message.content = updatedContent;
            if (db != null) db.messageDao().upsert(message);
            List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
            updateMessageContent(current, message);
            List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
            messages.postValue(processed);
            updatePreviewFromHistory(processed);
            sendChecklistUpdate(token, checklistId, itemId, done, null);
        });
    }

    public void addChecklistItem(MessageEntity message, String checklistId, String text) {
        if (message == null || checklistId == null || text == null) return;
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || chatId == null) return;
        io.execute(() -> {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(message.content);
            if (payload == null) return;
            boolean canAdd = payload.othersCanAdd || (message.senderId != null && message.senderId.equals(selfId));
            if (!canAdd) return;
            if (payload.items == null) {
                payload.items = new ArrayList<>();
            }
            ChecklistItem item = new ChecklistItem(UUID.randomUUID().toString(), text, false);
            payload.items.add(item);
            String updatedContent = ChecklistCodec.encodeChecklist(payload);
            if (updatedContent == null) return;
            message.content = updatedContent;
            if (db != null) db.messageDao().upsert(message);
            List<MessageEntity> current = new ArrayList<>(messages.getValue() != null ? messages.getValue() : new ArrayList<>());
            updateMessageContent(current, message);
            List<MessageEntity> processed = processChecklistMessages(sortMessages(current));
            messages.postValue(processed);
            updatePreviewFromHistory(processed);
            sendChecklistAddUpdate(token, checklistId, item);
        });
    }

    private void updateMessageContent(List<MessageEntity> list, MessageEntity message) {
        if (list == null || message == null || message.id == null) return;
        for (MessageEntity m : list) {
            if (m != null && message.id.equals(m.id)) {
                m.content = message.content;
                return;
            }
        }
    }

    private void sendChecklistUpdate(String token, String checklistId, String itemId, boolean done, ChecklistItem added) {
        if (isPanicMode()) return;
        ChecklistUpdatePayload update = new ChecklistUpdatePayload();
        update.checklistId = checklistId;
        if (itemId != null) {
            List<ChecklistItemUpdate> updates = new ArrayList<>();
            updates.add(new ChecklistItemUpdate(itemId, done));
            update.updates = updates;
        }
        if (added != null) {
            List<ChecklistItem> addedItems = new ArrayList<>();
            addedItems.add(added);
            update.added = addedItems;
        }
        String content = ChecklistCodec.encodeUpdate(update);
        if (content == null) return;
        try {
            if (isGroup) {
                repo.sendGroupMessage(token, chatId, content, "text", null, selfId, selfName, null, null, null);
            } else if (isChannel) {
                repo.sendChannelMessage(token, chatId, content, "text", null, selfId, selfName, null, null, null);
            } else {
                repo.sendMessage(token, chatId, content, "text", null, selfId);
            }
        } catch (Exception ignored) {
        }
    }

    private void sendChecklistAddUpdate(String token, String checklistId, ChecklistItem item) {
        sendChecklistUpdate(token, checklistId, null, false, item);
    }

    private static final class ReadBytesResult {
        final byte[] bytes;
        final boolean tooLarge;

        ReadBytesResult(byte[] bytes, boolean tooLarge) {
            this.bytes = bytes;
            this.tooLarge = tooLarge;
        }
    }

    private ReadBytesResult readBytes(ContentResolver resolver, Uri uri, long maxBytes) {
        InputStream in = null;
        try {
            String scheme = uri.getScheme();
            if (scheme == null || "file".equals(scheme)) {
                String path = uri.getPath();
                if (path == null) return new ReadBytesResult(null, false);
                File f = new File(path);
                if (!f.exists()) return new ReadBytesResult(null, false);
                if (f.length() > maxBytes) return new ReadBytesResult(null, true);
                in = new FileInputStream(f);
            } else {
                in = resolver.openInputStream(uri);
            }
            if (in == null) return new ReadBytesResult(null, false);
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buffer = new byte[8192];
            int read;
            long total = 0;
            while ((read = in.read(buffer)) != -1) {
                total += read;
                if (total > maxBytes) {
                    return new ReadBytesResult(null, true);
                }
                out.write(buffer, 0, read);
            }
            return new ReadBytesResult(out.toByteArray(), false);
        } catch (Exception e) {
            return new ReadBytesResult(null, false);
        } finally {
            try {
                if (in != null) in.close();
            } catch (Exception ignored) {
            }
        }
    }

    private boolean isImageFile(String name) {
        if (name == null) return false;
        String lower = name.toLowerCase();
        return lower.endsWith(".png") || lower.endsWith(".jpg") || lower.endsWith(".jpeg")
                || lower.endsWith(".gif") || lower.endsWith(".webp");
    }

    private boolean isPanicMode() {
        return SecurityContext.get().isPanicMode();
    }
}
