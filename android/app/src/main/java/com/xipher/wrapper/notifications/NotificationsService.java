package com.xipher.wrapper.notifications;

import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;

import androidx.annotation.Nullable;

import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.DatabaseManager;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.PinPrefs;
import com.xipher.wrapper.security.SecurityContext;
import com.xipher.wrapper.ui.CallActivity;
import com.xipher.wrapper.util.ChecklistCodec;
import com.xipher.wrapper.util.SpoilerCodec;
import com.xipher.wrapper.ws.SocketManager;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class NotificationsService extends Service implements SocketManager.Listener {
    public static final String ACTION_START = "com.xipher.wrapper.notifications.START";
    private static final String TAG = "NotificationsService";
    private static final long RECONNECT_MIN_MS = 5000L;
    private static final long RECONNECT_MAX_MS = 60000L;

    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());

    private SocketManager socket;
    private String currentToken;
    private AuthRepository authRepo;
    private long reconnectDelayMs = RECONNECT_MIN_MS;
    private Runnable reconnectRunnable;

    public static void startIfPossible(Context context) {
        if (context == null || !canRun(context)) return;
        Intent intent = new Intent(context, NotificationsService.class);
        intent.setAction(ACTION_START);
        try {
            context.getApplicationContext().startService(intent);
        } catch (Throwable t) {
            Log.w(TAG, "startService failed", t);
        }
    }

    public static void stop(Context context) {
        if (context == null) return;
        try {
            context.getApplicationContext().stopService(new Intent(context, NotificationsService.class));
        } catch (Throwable t) {
            Log.w(TAG, "stopService failed", t);
        }
    }

    private static boolean canRun(Context context) {
        if (context == null) return false;
        if (PanicModePrefs.isEnabled(context)) return false;
        if (SecurityContext.get().isPanicMode()) return false;
        if (!PushTokenManager.isPushEnabled(context)) return false;
        AuthRepository repo = AppServices.authRepository(context);
        String token = repo.getToken();
        return token != null && !token.isEmpty();
    }

    @Override
    public void onCreate() {
        super.onCreate();
        authRepo = AppServices.authRepository(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (!shouldRun()) {
            stopSelf();
            return START_NOT_STICKY;
        }
        ensureSocket();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        cancelReconnect();
        closeSocket();
        io.shutdownNow();
        requestRestart();
        super.onDestroy();
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        super.onTaskRemoved(rootIntent);
        requestRestart();
    }

    @Nullable
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private boolean shouldRun() {
        return canRun(this);
    }

    private void ensureSocket() {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            stopSelf();
            return;
        }
        if (socket != null && token.equals(currentToken)) return;
        closeSocket();
        currentToken = token;
        PushTokenManager.CachedToken cached = PushTokenManager.isPushEnabled(this)
                ? PushTokenManager.getCachedTokenInfo(this)
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

    private void requestRestart() {
        if (!shouldRun()) return;
        Intent intent = new Intent(this, AppStartReceiver.class);
        intent.setAction(AppStartReceiver.ACTION_APP_START);
        try {
            sendBroadcast(intent);
        } catch (Throwable t) {
            Log.w(TAG, "restart broadcast failed", t);
        }
    }

    @Override
    public void onAuthed() {
        reconnectDelayMs = RECONNECT_MIN_MS;
        cancelReconnect();
    }

    @Override
    public void onMessage(JsonObject obj) {
        if (obj == null) return;
        if (!shouldRun()) {
            stopSelf();
            return;
        }
        String type = getString(obj, "type");
        if ("call_offer".equals(type)) {
            handleIncomingCall(obj);
            return;
        }
        if ("channel_new_message".equals(type)) {
            handleChannelMessage(obj, "channel");
            return;
        }
        if ("group_message".equals(type)) {
            handleChannelMessage(obj, "group");
            return;
        }
        if ("new_message".equals(type)) {
            JsonObject msg = obj.has("message") && obj.get("message").isJsonObject()
                    ? obj.getAsJsonObject("message")
                    : obj;
            handleDirectMessage(msg);
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

    private void scheduleReconnect() {
        if (!shouldRun() || reconnectRunnable != null) return;
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

    private void handleIncomingCall(JsonObject obj) {
        String callerId = getString(obj, "from_user_id");
        if (callerId == null || callerId.isEmpty()) return;
        String callerName = getString(obj, "from_username", "from_user_name");
        String callId = getString(obj, "call_id");
        if (callId == null || callId.isEmpty()) {
            callId = "call-" + callerId;
        }
        if (!CallActivity.isCallInProgress()) {
            CallActivity.startIncoming(getApplicationContext(), callerId, callerName);
        }
        NotificationHelper.showCallNotification(this, callerName, callId, callerId);
    }

    private void handleChannelMessage(JsonObject obj, String chatType) {
        String chatId = getString(obj, "chat_id");
        if (chatId == null || chatId.isEmpty()) return;
        io.execute(() -> {
            if (ChatListPrefs.isMuted(getApplicationContext(), chatId, chatType)) return;
            if (!isDatabaseReady()) {
                String chatTitle = getString(obj, "chat_title", "chat_name", "title");
                String userName = getString(obj, "user_name", "username");
                String title = chatTitle != null && !chatTitle.isEmpty() ? chatTitle : getString(R.string.app_name);
                String body = getString(R.string.chat_preview_new_messages);
                NotificationHelper.showMessageNotification(getApplicationContext(), title, body, chatId, chatTitle, chatType, userName, null);
                return;
            }
            PushSyncHelper.syncAndNotify(getApplicationContext(), chatId, chatType, null, null, null, null);
        });
    }

    private void handleDirectMessage(JsonObject msg) {
        if (msg == null) return;
        io.execute(() -> {
            String senderId = getString(msg, "sender_id");
            String receiverId = getString(msg, "receiver_id");
            if ((senderId == null || senderId.isEmpty()) && (receiverId == null || receiverId.isEmpty())) return;
            String selfId = authRepo.getUserId();
            if (selfId != null && selfId.equals(senderId)) {
                return;
            }
            String chatId = senderId;
            if ((chatId == null || chatId.isEmpty()) && receiverId != null && !receiverId.isEmpty()) {
                chatId = receiverId;
            }
            if (chatId == null || chatId.isEmpty()) return;
            String chatType = "chat";
            if (ChatListPrefs.isMuted(getApplicationContext(), chatId, chatType)) return;

            String senderName = getString(msg, "sender_name", "sender_username");
            String body = buildPreview(msg);
            if (body == null || body.isEmpty()) {
                if (isDatabaseReady()) {
                    PushSyncHelper.syncAndNotify(getApplicationContext(), chatId, chatType, null, null, null, senderName);
                } else {
                    String title = senderName != null && !senderName.isEmpty()
                            ? senderName
                            : getString(R.string.app_name);
                    String fallbackBody = getString(R.string.chat_preview_new_messages);
                    NotificationHelper.showMessageNotification(
                            getApplicationContext(),
                            title,
                            fallbackBody,
                            chatId,
                            null,
                            chatType,
                            senderName,
                            senderName
                    );
                }
                return;
            }
            ChatEntity chat = safeGetChat(chatId);
            String chatTitle = chat != null ? chat.title : null;
            String userName = chat != null && chat.username != null && !chat.username.isEmpty()
                    ? chat.username
                    : senderName;
            String title = chatTitle != null && !chatTitle.isEmpty()
                    ? chatTitle
                    : (userName != null && !userName.isEmpty() ? userName : getString(R.string.app_name));

            NotificationHelper.showMessageNotification(
                    getApplicationContext(),
                    title,
                    body,
                    chatId,
                    chatTitle,
                    chatType,
                    userName,
                    senderName
            );
        });
    }

    private String buildPreview(JsonObject msg) {
        String content = getString(msg, "content");
        if (content == null) content = "";
        if (SpoilerCodec.isSpoilerContent(content)) {
            return getString(R.string.chat_preview_spoiler);
        }
        if (ChecklistCodec.isChecklistContent(content)) {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(content);
            if (payload != null && payload.title != null && !payload.title.isEmpty()) {
                return getString(R.string.chat_preview_checklist) + ": " + payload.title;
            }
            return getString(R.string.chat_preview_checklist);
        }
        if (ChecklistCodec.isChecklistUpdateContent(content)) {
            return getString(R.string.chat_preview_checklist);
        }
        String messageType = getString(msg, "message_type");
        if (messageType == null) messageType = "text";
        switch (messageType) {
            case "voice":
                return getString(R.string.chat_preview_voice);
            case "image":
                return getString(R.string.chat_preview_photo);
            case "file":
                return getString(R.string.chat_preview_file);
            case "location":
                return getString(R.string.chat_preview_location);
            case "live_location":
                return getString(R.string.chat_preview_live_location);
            default:
                return content;
        }
    }

    private ChatEntity safeGetChat(String chatId) {
        if (chatId == null || chatId.isEmpty()) return null;
        try {
            return AppDatabase.get(getApplicationContext()).chatDao().getById(chatId);
        } catch (Exception ignored) {
            return null;
        }
    }

    private boolean isDatabaseReady() {
        try {
            AppDatabase.get(getApplicationContext());
            return true;
        } catch (Exception ignored) {
        }
        if (PanicModePrefs.isEnabled(getApplicationContext())) return false;
        if (PinPrefs.isMainPinSet(getApplicationContext())) return false;
        String key = DbKeyStore.getMainKey(getApplicationContext());
        if (key == null || key.isEmpty()) return false;
        try {
            DatabaseManager.getInstance().init(getApplicationContext(), key);
            return true;
        } catch (Exception ignored) {
            return false;
        }
    }

    private String getString(JsonObject obj, String... keys) {
        if (obj == null || keys == null) return null;
        for (String key : keys) {
            if (key == null || key.isEmpty()) continue;
            if (obj.has(key) && obj.get(key).isJsonPrimitive()) {
                try {
                    String val = obj.get(key).getAsString();
                    if (val != null) return val;
                } catch (Exception ignored) {
                }
            }
        }
        return null;
    }
}
