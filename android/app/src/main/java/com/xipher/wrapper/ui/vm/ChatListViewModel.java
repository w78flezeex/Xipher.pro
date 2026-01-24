package com.xipher.wrapper.ui.vm;

import android.app.Application;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.NonNull;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;

import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.google.gson.JsonObject;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.data.repo.ChatRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.security.SecurityContext;
import com.xipher.wrapper.ws.SocketManager;
import com.xipher.wrapper.R;
import com.xipher.wrapper.ui.CallActivity;
import com.xipher.wrapper.util.ChecklistCodec;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.util.SpoilerCodec;

import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ChatListViewModel extends AndroidViewModel implements SocketManager.Listener {

    private final MutableLiveData<List<ChatEntity>> chats = new MutableLiveData<>();
    private final MutableLiveData<Boolean> loading = new MutableLiveData<>(false);
    private final ChatRepository repo;
    private final AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final AppDatabase db;
    private final Handler handler = new Handler(Looper.getMainLooper());
    private SocketManager socket;
    private String currentToken;
    private long reconnectDelayMs = RECONNECT_MIN_MS;
    private Runnable reconnectRunnable;

    private static final long RECONNECT_MIN_MS = 5000L;
    private static final long RECONNECT_MAX_MS = 60000L;

    public ChatListViewModel(@NonNull Application app) {
        super(app);
        authRepo = AppServices.authRepository(app);
        AppDatabase tempDb = null;
        try {
            tempDb = AppDatabase.get(app);
        } catch (IllegalStateException e) {
            // Database not initialized yet
        }
        db = tempDb;
        repo = new ChatRepository(app, authRepo.getToken());
        // начальная загрузка из кеша
        if (db != null) {
            io.execute(() -> chats.postValue(db.chatDao().getAll()));
        }
    }

    public LiveData<List<ChatEntity>> getChats() {
        return chats;
    }

    public LiveData<Boolean> getLoading() {
        return loading;
    }

    public void refresh() {
        String token = authRepo.getToken();
        if (token == null) return;
        loading.postValue(true);
        io.execute(() -> {
            try {
                List<ChatEntity> list = repo.syncChats(token);
                chats.postValue(list);
            } catch (Exception e) {
                // ignore; оставим старые данные
            } finally {
                loading.postValue(false);
            }
        });
    }

    public void startSocket() {
        if (SecurityContext.get().isPanicMode()) return;
        ensureSocket();
    }

    public void stopSocket() {
        cancelReconnect();
        closeSocket();
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
        if ("channel_new_message".equals(type)) {
            String channelId = obj.has("chat_id") ? obj.get("chat_id").getAsString() : null;
            touchChat(channelId, getApplication().getString(R.string.chat_preview_new_messages));
            return;
        }
        if ("group_message".equals(type)) {
            String groupId = obj.has("chat_id") ? obj.get("chat_id").getAsString() : null;
            touchChat(groupId, getApplication().getString(R.string.chat_preview_new_messages));
            return;
        }
        if ("new_message".equals(type)) {
            JsonObject msg = obj.has("message") ? obj.getAsJsonObject("message") : obj;
            String senderId = msg.has("sender_id") ? msg.get("sender_id").getAsString() : null;
            String receiverId = msg.has("receiver_id") ? msg.get("receiver_id").getAsString() : null;
            String selfId = authRepo.getUserId();
            String chatId = senderId;
            if (selfId != null && selfId.equals(senderId)) {
                chatId = receiverId;
            }
            String content = msg.has("content") ? msg.get("content").getAsString() : "";
            if (ChecklistCodec.isChecklistUpdateContent(content)) {
                return;
            }
            String preview = buildPreview(msg);
            touchChat(chatId, preview);
        }
    }

    @Override
    public void onClosed() {
        scheduleReconnect();
    }

    @Override
    public void onFailure(Throwable t) {
        scheduleReconnect();
    }

    @Override
    protected void onCleared() {
        stopSocket();
        io.shutdownNow();
        super.onCleared();
    }

    private void ensureSocket() {
        if (SecurityContext.get().isPanicMode()) return;
        String token = authRepo.getToken();
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

    private String buildPreview(JsonObject msg) {
        String content = msg.has("content") ? msg.get("content").getAsString() : "";
        if (SpoilerCodec.isSpoilerContent(content)) {
            return getApplication().getString(R.string.chat_preview_spoiler);
        }
        if (ChecklistCodec.isChecklistContent(content)) {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(content);
            if (payload != null && payload.title != null && !payload.title.isEmpty()) {
                return getApplication().getString(R.string.chat_preview_checklist) + ": " + payload.title;
            }
            return getApplication().getString(R.string.chat_preview_checklist);
        }
        String messageType = msg.has("message_type") ? msg.get("message_type").getAsString() : "text";
        if ("voice".equals(messageType)) {
            return getApplication().getString(R.string.chat_preview_voice);
        }
        if ("image".equals(messageType)) {
            return getApplication().getString(R.string.chat_preview_photo);
        }
        if ("file".equals(messageType)) {
            return getApplication().getString(R.string.chat_preview_file);
        }
        if ("location".equals(messageType)) {
            return getApplication().getString(R.string.chat_preview_location);
        }
        if ("live_location".equals(messageType)) {
            return getApplication().getString(R.string.chat_preview_live_location);
        }
        return content;
    }

    private void touchChat(String chatId, String preview) {
        if (chatId == null || chatId.isEmpty()) return;
        if (db == null) return;
        String safePreview = preview != null ? preview : "";
        long now = System.currentTimeMillis();
        io.execute(() -> {
            db.chatDao().updatePreview(chatId, safePreview, now);
            chats.postValue(db.chatDao().getAll());
        });
    }

    private void handleIncomingCall(JsonObject obj) {
        String fromId = obj.has("from_user_id") ? obj.get("from_user_id").getAsString() : null;
        if (fromId == null || fromId.isEmpty()) return;
        String fromName = obj.has("from_username") ? obj.get("from_username").getAsString() : null;
        CallActivity.startIncoming(getApplication(), fromId, fromName);
    }
}
