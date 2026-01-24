package com.xipher.wrapper.ws;

import android.util.Log;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.security.SecurityContext;

import java.util.concurrent.TimeUnit;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.WebSocket;
import okhttp3.WebSocketListener;

public class SocketManager {
    private static final String TAG = "SocketManager";

    public interface Listener {
        void onAuthed();
        void onMessage(JsonObject obj);
        void onClosed();
        void onFailure(Throwable t);
    }

    private final OkHttpClient client;
    private final Gson gson = new Gson();
    private WebSocket ws;
    private volatile boolean authed = false;
    private Listener listener;
    private final String token;
    private final String pushToken;
    private final String pushPlatform;

    public SocketManager(String token) {
        this(token, null, null);
    }

    public SocketManager(String token, String pushToken, String pushPlatform) {
        this.token = token;
        this.pushToken = pushToken;
        this.pushPlatform = pushPlatform;
        client = new OkHttpClient.Builder()
                .pingInterval(20, TimeUnit.SECONDS)
                .connectTimeout(10, TimeUnit.SECONDS)
                .build();
    }

    public void setListener(Listener l) {
        this.listener = l;
    }

    public void connect() {
        if (SecurityContext.get().isPanicMode()) {
            Log.w(TAG, "panic mode active; websocket blocked");
            if (listener != null) listener.onClosed();
            return;
        }
        String scheme = BuildConfig.API_BASE.startsWith("https") ? "wss" : "ws";
        String url = scheme + "://" + BuildConfig.API_BASE.replace("https://", "").replace("http://", "") + BuildConfig.WS_PATH;
        Request req = new Request.Builder().url(url).build();
        authed = false;
        ws = client.newWebSocket(req, new WebSocketListener() {
            @Override
            public void onOpen(WebSocket webSocket, Response response) {
                authed = false;
                JsonObject auth = new JsonObject();
                auth.addProperty("type", "auth");
                auth.addProperty("token", token);
                if (pushToken != null && !pushToken.isEmpty()) {
                    auth.addProperty("push_token", pushToken);
                    if (pushPlatform != null && !pushPlatform.isEmpty()) {
                        auth.addProperty("platform", pushPlatform);
                    }
                }
                webSocket.send(gson.toJson(auth));
            }

            @Override
            public void onMessage(WebSocket webSocket, String text) {
                try {
                    JsonElement el = gson.fromJson(text, JsonElement.class);
                    if (!el.isJsonObject()) return;
                    JsonObject obj = el.getAsJsonObject();
                    String type = obj.has("type") ? obj.get("type").getAsString() : "";
                    if ("auth_success".equals(type)) {
                        authed = true;
                        if (listener != null) listener.onAuthed();
                        return;
                    }
                    if ("auth_error".equals(type)) {
                        authed = false;
                        return;
                    }
                    if (listener != null) listener.onMessage(obj);
                } catch (Exception e) {
                    Log.e(TAG, "parse error", e);
                }
            }

            @Override
            public void onClosed(WebSocket webSocket, int code, String reason) {
                authed = false;
                if (listener != null) listener.onClosed();
            }

            @Override
            public void onFailure(WebSocket webSocket, Throwable t, Response response) {
                authed = false;
                if (listener != null) listener.onFailure(t);
            }
        });
    }

    public boolean send(JsonObject obj) {
        if (ws != null) {
            return ws.send(gson.toJson(obj));
        }
        return false;
    }

    public void close() {
        if (ws != null) {
            ws.close(1000, "client");
            ws = null;
        }
    }

    public boolean isAuthed() {
        return authed;
    }
}
