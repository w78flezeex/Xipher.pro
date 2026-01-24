package com.xipher.wrapper.data.api;

import android.util.Log;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.data.model.AuthResponse;
import com.xipher.wrapper.data.model.BasicResponse;
import com.xipher.wrapper.data.model.ChannelDto;
import com.xipher.wrapper.data.model.ChatDto;
import com.xipher.wrapper.data.model.ChatFolder;
import com.xipher.wrapper.data.model.ChatPinDto;
import com.xipher.wrapper.data.model.FriendDto;
import com.xipher.wrapper.data.model.GroupDto;
import com.xipher.wrapper.data.model.MessageDto;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import okhttp3.Call;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;
import okhttp3.logging.HttpLoggingInterceptor;
import okio.Buffer;
import okio.BufferedSink;
import okio.ForwardingSink;
import okio.Okio;
import okio.Sink;

public class ApiClient {
    private static final String TAG = "ApiClient";
    private static final MediaType JSON = MediaType.get("application/json; charset=utf-8");
    private static final String SESSION_COOKIE_NAME = "xipher_token";

    private final OkHttpClient client;
    private final Gson gson = new Gson();

    public ApiClient(String token) {
        HttpLoggingInterceptor logging = new HttpLoggingInterceptor();
        logging.setLevel(HttpLoggingInterceptor.Level.BASIC);
        OkHttpClient.Builder builder = new OkHttpClient.Builder().addInterceptor(logging);
        if (token != null && !token.isEmpty()) {
            builder.addInterceptor(chain -> {
                Request original = chain.request();
                Request.Builder rb = original.newBuilder();
                // Сервер ожидает токен в теле, но храним для WS; можно добавить заголовок при необходимости
                rb.header("X-Auth-Token", token);
                return chain.proceed(rb.build());
            });
        }
        client = builder.build();
    }

    private String url(String path) {
        return BuildConfig.API_BASE + path;
    }

    private Response postJson(String path, Object body) throws IOException {
        String json = gson.toJson(body);
        RequestBody rb = RequestBody.create(json, JSON);
        Request req = new Request.Builder()
                .url(url(path))
                .post(rb)
                .build();
        return client.newCall(req).execute();
    }

    private Call postJsonCall(String path, Object body, ProgressListener listener) {
        String json = gson.toJson(body);
        RequestBody base = RequestBody.create(json, JSON);
        RequestBody rb = listener != null ? new ProgressRequestBody(base, listener) : base;
        Request req = new Request.Builder()
                .url(url(path))
                .post(rb)
                .build();
        return client.newCall(req);
    }

    public AuthResponse login(String username, String password) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("username", username);
        o.addProperty("password", password);
        try (Response r = postJson("/api/login", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            String body = r.body() != null ? r.body().string() : "";
            AuthResponse res = gson.fromJson(body, AuthResponse.class);
            String cookieToken = extractCookieValue(r.headers("Set-Cookie"), SESSION_COOKIE_NAME);
            if (res != null) {
                if (res.data == null && cookieToken != null && !cookieToken.isEmpty()) {
                    res.data = new AuthResponse.AuthData();
                }
                if (res.data != null && (res.data.token == null || res.data.token.isEmpty())
                        && cookieToken != null && !cookieToken.isEmpty()) {
                    res.data.token = cookieToken;
                }
            }
            return res;
        }
    }

    public BasicResponse register(String username, String password) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("username", username);
        o.addProperty("password", password);
        try (Response r = postJson("/api/register", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public BasicResponse registerPushToken(String token, String deviceToken, String platform) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("device_token", deviceToken);
        o.addProperty("platform", platform != null ? platform : "android");
        try (Response r = postJson("/api/register-push-token", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public BasicResponse deletePushToken(String token, String deviceToken) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("device_token", deviceToken);
        try (Response r = postJson("/api/delete-push-token", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public BasicResponse checkUsername(String username) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("username", username);
        try (Response r = postJson("/api/check-username", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public boolean validate(String token) {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/validate-token", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res.has("success") && res.get("success").getAsBoolean();
        } catch (Exception e) {
            Log.e(TAG, "validate error", e);
            return false;
        }
    }

    public JsonObject validateTokenInfo(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/validate-token", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    private static String extractCookieValue(List<String> cookies, String name) {
        if (cookies == null || cookies.isEmpty()) return null;
        String needle = name + "=";
        for (String cookie : cookies) {
            if (cookie == null) continue;
            int start = cookie.indexOf(needle);
            if (start < 0) continue;
            start += needle.length();
            int end = cookie.indexOf(';', start);
            if (end < 0) end = cookie.length();
            String value = cookie.substring(start, end);
            if (!value.isEmpty()) return value;
        }
        return null;
    }

    public JsonObject createPremiumPayment(String token, String plan, String provider) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        if (plan != null && !plan.isEmpty()) {
            o.addProperty("plan", plan);
        }
        if (provider != null && !provider.isEmpty()) {
            o.addProperty("provider", provider);
        }
        try (Response r = postJson("/api/premium/create-payment", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject createPremiumGiftPayment(String token, String plan, String provider, String receiverId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        if (plan != null && !plan.isEmpty()) {
            o.addProperty("plan", plan);
        }
        if (provider != null && !provider.isEmpty()) {
            o.addProperty("provider", provider);
        }
        if (receiverId != null && !receiverId.isEmpty()) {
            o.addProperty("receiver_id", receiverId);
        }
        try (Response r = postJson("/api/premium/create-gift-payment", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject setBusinessHours(String token, String businessHoursJson) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        if (businessHoursJson != null) {
            o.addProperty("business_hours", businessHoursJson);
        }
        try (Response r = postJson("/api/set-business-hours", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject setPremium(String token, boolean active, String plan) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("active", active ? "true" : "false");
        if (plan != null && !plan.isEmpty()) {
            o.addProperty("plan", plan);
        }
        try (Response r = postJson("/api/set-premium", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public List<ChatDto> chats(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/chats", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("chats")) {
                ChatDto[] arr = gson.fromJson(obj.get("chats"), ChatDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public List<ChatPinDto> chatPins(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/get-chat-pins", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("pins")) {
                ChatPinDto[] arr = gson.fromJson(obj.get("pins"), ChatPinDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public List<ChatFolder> chatFolders(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/get-chat-folders", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("folders")) {
                ChatFolder[] arr = gson.fromJson(obj.get("folders"), ChatFolder[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public BasicResponse setChatFolders(String token, List<ChatFolder> folders) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        String payload = gson.toJson(folders != null ? folders : Collections.emptyList());
        o.addProperty("folders", payload);
        try (Response r = postJson("/api/set-chat-folders", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public BasicResponse pinChat(String token, String chatId, String chatType) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("chat_id", chatId);
        if (chatType != null && !chatType.isEmpty()) {
            o.addProperty("chat_type", chatType);
        }
        try (Response r = postJson("/api/pin-chat", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public BasicResponse unpinChat(String token, String chatId, String chatType) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("chat_id", chatId);
        if (chatType != null && !chatType.isEmpty()) {
            o.addProperty("chat_type", chatType);
        }
        try (Response r = postJson("/api/unpin-chat", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), BasicResponse.class);
        }
    }

    public List<FriendDto> friends(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/friends", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("friends")) {
                FriendDto[] arr = gson.fromJson(obj.get("friends"), FriendDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public List<GroupDto> groups(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/get-groups", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("groups")) {
                GroupDto[] arr = gson.fromJson(obj.get("groups"), GroupDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public JsonObject findUser(String token, String username) throws IOException {
        JsonObject o = new JsonObject();
        if (token != null && !token.isEmpty()) {
            o.addProperty("token", token);
        }
        o.addProperty("username", username);
        try (Response r = postJson("/api/find-user", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public List<ChannelDto> channels(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/get-channels", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("channels")) {
                ChannelDto[] arr = gson.fromJson(obj.get("channels"), ChannelDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public List<MessageDto> messages(String token, String chatId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("friend_id", chatId);
        try (Response r = postJson("/api/messages", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("messages")) {
                MessageDto[] arr = gson.fromJson(obj.get("messages"), MessageDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public List<MessageDto> groupMessages(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/get-group-messages", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("messages")) {
                MessageDto[] arr = gson.fromJson(obj.get("messages"), MessageDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public List<MessageDto> channelMessages(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/get-channel-messages", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            JsonObject obj = gson.fromJson(r.body().string(), JsonObject.class);
            if (obj.has("messages")) {
                MessageDto[] arr = gson.fromJson(obj.get("messages"), MessageDto[].class);
                return Arrays.asList(arr);
            }
            return Collections.emptyList();
        }
    }

    public JsonObject sendMessage(String token, JsonObject payload) throws IOException {
        payload.addProperty("token", token);
        try (Response r = postJson("/api/send-message", payload)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject sendGroupMessage(String token, JsonObject payload) throws IOException {
        payload.addProperty("token", token);
        try (Response r = postJson("/api/send-group-message", payload)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject sendChannelMessage(String token, JsonObject payload) throws IOException {
        payload.addProperty("token", token);
        try (Response r = postJson("/api/send-channel-message", payload)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject deleteMessage(String token, String messageId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("message_id", messageId);
        try (Response r = postJson("/api/delete-message", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject deleteGroupMessage(String token, String groupId, String messageId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("message_id", messageId);
        try (Response r = postJson("/api/delete-group-message", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject deleteChannelMessage(String token, String channelId, String messageId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("message_id", messageId);
        try (Response r = postJson("/api/delete-channel-message", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject deleteChat(String token, String chatId, String chatType) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("chat_id", chatId);
        if (chatType != null && !chatType.isEmpty()) {
            o.addProperty("chat_type", chatType);
        }
        try (Response r = postJson("/api/delete-chat", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject uploadFile(String token, String fileName, String fileData) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("file_name", fileName);
        o.addProperty("file_data", fileData);
        try (Response r = postJson("/api/upload-file", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public Call uploadFileCall(String token, String fileName, String fileData, ProgressListener listener) {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("file_name", fileName);
        o.addProperty("file_data", fileData);
        return postJsonCall("/api/upload-file", o, listener);
    }

    public interface ProgressListener {
        void onProgress(long bytesWritten, long contentLength);
    }

    private static class ProgressRequestBody extends RequestBody {
        private final RequestBody delegate;
        private final ProgressListener listener;

        private ProgressRequestBody(RequestBody delegate, ProgressListener listener) {
            this.delegate = delegate;
            this.listener = listener;
        }

        @Override
        public MediaType contentType() {
            return delegate.contentType();
        }

        @Override
        public long contentLength() throws IOException {
            return delegate.contentLength();
        }

        @Override
        public void writeTo(BufferedSink sink) throws IOException {
            Sink forwarding = new ForwardingSink(sink) {
                long written = 0L;
                long total = -1L;

                @Override
                public void write(Buffer source, long byteCount) throws IOException {
                    super.write(source, byteCount);
                    if (total < 0L) {
                        total = contentLength();
                    }
                    written += byteCount;
                    if (listener != null) {
                        listener.onProgress(written, total);
                    }
                }
            };
            BufferedSink buffered = Okio.buffer(forwarding);
            delegate.writeTo(buffered);
            buffered.flush();
        }
    }

    public JsonObject uploadVoice(String token, String voiceData, String mimeType) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("voice_data", voiceData);
        if (mimeType != null && !mimeType.isEmpty()) {
            o.addProperty("mime_type", mimeType);
        }
        try (Response r = postJson("/api/upload-voice", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject uploadAvatar(String token, String fileName, String avatarData) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("avatar_data", avatarData);
        if (fileName != null && !fileName.isEmpty()) {
            o.addProperty("filename", fileName);
        }
        try (Response r = postJson("/api/upload-avatar", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject createGroup(String token, String name) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("name", name);
        try (Response r = postJson("/api/create-group", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject addFriendToGroup(String token, String groupId, String friendId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("friend_id", friendId);
        try (Response r = postJson("/api/add-friend-to-group", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject createChannel(String token, String name) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("name", name);
        try (Response r = postJson("/api/create-channel", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject searchChannel(String token, String username) throws IOException {
        JsonObject o = new JsonObject();
        if (token != null && !token.isEmpty()) {
            o.addProperty("token", token);
        }
        o.addProperty("username", username);
        try (Response r = postJson("/api/search-channel", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getChannelInfo(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/get-channel-info", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject subscribeChannel(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/subscribe-channel", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject updateChannelName(String token, String channelId, String newName) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("new_name", newName);
        try (Response r = postJson("/api/update-channel-name", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject updateChannelDescription(String token, String channelId, String newDescription) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("new_description", newDescription);
        try (Response r = postJson("/api/update-channel-description", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject setChannelCustomLink(String token, String channelId, String customLink) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        if (customLink != null) {
            o.addProperty("custom_link", customLink);
        }
        try (Response r = postJson("/api/set-channel-custom-link", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject setChannelPrivacy(String token, String channelId, boolean isPrivate) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("is_private", isPrivate);
        try (Response r = postJson("/api/set-channel-privacy", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject setChannelShowAuthor(String token, String channelId, boolean showAuthor) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("show_author", showAuthor);
        try (Response r = postJson("/api/set-channel-show-author", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject updateMyProfile(String token, String firstName, String lastName, String bio) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        if (firstName != null) o.addProperty("first_name", firstName);
        if (lastName != null) o.addProperty("last_name", lastName);
        if (bio != null) o.addProperty("bio", bio);
        try (Response r = postJson("/api/update-my-profile", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject updateMyPrivacy(String token, String bioVisibility, String birthVisibility,
                                      String lastSeenVisibility, String avatarVisibility,
                                      Boolean sendReadReceipts) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        if (bioVisibility != null) o.addProperty("bio_visibility", bioVisibility);
        if (birthVisibility != null) o.addProperty("birth_visibility", birthVisibility);
        if (lastSeenVisibility != null) o.addProperty("last_seen_visibility", lastSeenVisibility);
        if (avatarVisibility != null) o.addProperty("avatar_visibility", avatarVisibility);
        if (sendReadReceipts != null) o.addProperty("send_read_receipts", sendReadReceipts);
        try (Response r = postJson("/api/update-my-privacy", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getUserProfile(String token, String userId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("user_id", userId);
        try (Response r = postJson("/api/get-user-profile", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject callNotification(String token, String receiverId, String callType) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        o.addProperty("call_type", callType != null ? callType : "audio");
        try (Response r = postJson("/api/call-notification", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject callResponse(String token, String receiverId, String response) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        o.addProperty("response", response);
        try (Response r = postJson("/api/call-response", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject callAnswer(String token, String receiverId, String answerB64) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        o.addProperty("answer", answerB64);
        o.addProperty("answer_encoding", "b64");
        try (Response r = postJson("/api/call-answer", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject callIce(String token, String receiverId, String candidateB64) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        o.addProperty("candidate", candidateB64);
        o.addProperty("candidate_encoding", "b64");
        try (Response r = postJson("/api/call-ice", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject checkCallResponse(String token, String receiverId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        try (Response r = postJson("/api/check-call-response", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getCallOffer(String token, String receiverId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        try (Response r = postJson("/api/get-call-offer", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getCallAnswer(String token, String receiverId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        try (Response r = postJson("/api/get-call-answer", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getCallIce(String token, String receiverId, long lastCheck) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("receiver_id", receiverId);
        if (lastCheck > 0) {
            o.addProperty("last_check", Long.toString(lastCheck));
        }
        try (Response r = postJson("/api/get-call-ice", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getTurnCredentials(String userId, int ttlMinutes) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("userId", userId);
        if (ttlMinutes > 0) {
            o.addProperty("ttlMinutes", ttlMinutes);
        }
        try (Response r = postJson("/api/turn-credentials", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    public JsonObject getTurnConfig(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/turn-config", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    // === CHANNEL MANAGEMENT API ===

    /**
     * Получить список подписчиков канала
     */
    public JsonObject getChannelMembers(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/get-channel-members", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Заблокировать/разблокировать подписчика канала
     */
    public boolean banChannelMember(String token, String channelId, String targetUserId, boolean ban) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("target_user_id", targetUserId);
        o.addProperty("banned", ban ? "true" : "false");
        try (Response r = postJson("/api/ban-channel-member", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Назначить/снять права администратора канала
     */
    public boolean setChannelAdminPermissions(String token, String channelId, String targetUserId, 
            int perms, String title, boolean revoke) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("target_user_id", targetUserId);
        o.addProperty("perms", perms);
        if (title != null && !title.isEmpty()) {
            o.addProperty("title", title);
        }
        if (revoke) {
            o.addProperty("revoke", true);
        }
        try (Response r = postJson("/api/set-channel-admin-permissions", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Создать пригласительную ссылку для канала
     */
    public JsonObject createChannelInvite(String token, String channelId, int expiresInSeconds) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        if (expiresInSeconds > 0) {
            o.addProperty("expires_in_seconds", expiresInSeconds);
        }
        try (Response r = postJson("/api/create-channel-invite", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Удалить канал
     */
    public boolean deleteChannel(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/delete-channel", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Покинуть канал (отписаться)
     */
    public boolean leaveChannel(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/unsubscribe-channel", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Поиск сообщений в канале
     */
    public JsonObject searchChannelMessages(String token, String channelId, String query) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        o.addProperty("query", query);
        try (Response r = postJson("/api/search-channel-messages", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    // === GROUP MANAGEMENT API ===

    /**
     * Получить информацию о группе
     */
    public JsonObject getGroupInfo(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/get-group-info", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Получить список участников группы
     */
    public JsonObject getGroupMembers(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/get-group-members", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Назначить/снять администратора с настройкой прав
     */
    public boolean setAdminPermissions(String token, String groupId, String targetUserId, 
            String permissionsJson) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("target_user_id", targetUserId);
        o.addProperty("role", "admin");
        if (permissionsJson != null) {
            o.addProperty("permissions", permissionsJson);
        }
        try (Response r = postJson("/api/set-admin-permissions", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Исключить участника из группы
     */
    public boolean kickMember(String token, String groupId, String targetUserId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("target_user_id", targetUserId);
        try (Response r = postJson("/api/kick-member", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Заблокировать/разблокировать участника
     */
    public boolean banMember(String token, String groupId, String targetUserId, boolean ban) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("target_user_id", targetUserId);
        o.addProperty("banned", ban);
        try (Response r = postJson("/api/ban-member", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Заглушить/разглушить участника
     */
    public boolean muteMember(String token, String groupId, String targetUserId, boolean mute) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("target_user_id", targetUserId);
        o.addProperty("muted", mute);
        try (Response r = postJson("/api/mute-member", o)) {
            if (!r.isSuccessful()) return false;
            JsonObject res = gson.fromJson(r.body().string(), JsonObject.class);
            return res != null && res.has("success") && res.get("success").getAsBoolean();
        }
    }

    /**
     * Поиск сообщений в группе
     */
    public JsonObject searchGroupMessages(String token, String groupId, String query) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("query", query);
        try (Response r = postJson("/api/search-group-messages", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Создать пригласительную ссылку
     */
    public JsonObject createGroupInvite(String token, String groupId, int expiresInSeconds) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        if (expiresInSeconds > 0) {
            o.addProperty("expires_in_seconds", expiresInSeconds);
        }
        try (Response r = postJson("/api/create-group-invite", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Обновить информацию о группе
     */
    public JsonObject updateGroupInfo(String token, String groupId, String name, String description) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        if (name != null) o.addProperty("name", name);
        if (description != null) o.addProperty("description", description);
        try (Response r = postJson("/api/update-group-info", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Обновить название группы
     */
    public JsonObject updateGroupName(String token, String groupId, String name) throws IOException {
        return updateGroupInfo(token, groupId, name, null);
    }

    /**
     * Обновить описание группы
     */
    public JsonObject updateGroupDescription(String token, String groupId, String description) throws IOException {
        return updateGroupInfo(token, groupId, null, description);
    }

    /**
     * Покинуть группу
     */
    public JsonObject leaveGroup(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/leave-group", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Создать пригласительную ссылку для группы (без срока истечения)
     */
    public JsonObject createGroupInvite(String token, String groupId) throws IOException {
        return createGroupInvite(token, groupId, 0);
    }

    /**
     * Получить ссылку на группу
     */
    public JsonObject getGroupLink(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/get-group-link", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Mute/unmute чат
     */
    public JsonObject muteChat(String token, String chatId, boolean mute) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("chat_id", chatId);
        o.addProperty("muted", mute);
        try (Response r = postJson("/api/mute-chat", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Удалить группу (возвращает JsonObject)
     */
    public JsonObject deleteGroup(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/delete-group", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Подать заявку на верификацию канала
     */
    public JsonObject requestVerification(String token, String channelUsername, String reason) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_username", channelUsername);
        o.addProperty("reason", reason);
        try (Response r = postJson("/api/request-verification", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Получить мои заявки на верификацию
     */
    public JsonObject getMyVerificationRequests(String token) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        try (Response r = postJson("/api/get-my-verification-requests", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Получить публичные каналы для каталога
     */
    public JsonObject getPublicChannels(String token, String category, String search) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        if (category != null && !category.isEmpty() && !category.equals("all")) {
            o.addProperty("category", category);
        }
        if (search != null && !search.isEmpty()) {
            o.addProperty("search", search);
        }
        try (Response r = postJson("/api/public-directory", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Присоединиться к каналу
     */
    public JsonObject joinChannel(String token, String channelId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("channel_id", channelId);
        try (Response r = postJson("/api/subscribe-channel", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    // =====================================================
    // Forum Topics API (Telegram-style)
    // =====================================================

    /**
     * Get topics for a forum group
     */
    public JsonObject getGroupTopics(String token, String groupId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        try (Response r = postJson("/api/get-group-topics", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Create a new topic in a forum group
     */
    public JsonObject createGroupTopic(String token, String groupId, String name, String iconEmoji, String iconColor, boolean isGeneral) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("name", name);
        if (iconEmoji != null) o.addProperty("icon_emoji", iconEmoji);
        if (iconColor != null) o.addProperty("icon_color", iconColor);
        o.addProperty("is_general", String.valueOf(isGeneral));
        try (Response r = postJson("/api/create-group-topic", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Update an existing topic
     */
    public JsonObject updateGroupTopic(String token, String topicId, String name, String iconEmoji, String iconColor) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        if (name != null) o.addProperty("name", name);
        if (iconEmoji != null) o.addProperty("icon_emoji", iconEmoji);
        if (iconColor != null) o.addProperty("icon_color", iconColor);
        try (Response r = postJson("/api/update-group-topic", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Delete a topic
     */
    public JsonObject deleteGroupTopic(String token, String topicId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        try (Response r = postJson("/api/delete-group-topic", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Get messages from a topic
     */
    public JsonObject getTopicMessages(String token, String topicId, int limit) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        o.addProperty("limit", String.valueOf(limit));
        try (Response r = postJson("/api/get-topic-messages", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Send a message to a topic
     */
    public JsonObject sendTopicMessage(String token, String topicId, String content, String messageType) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        o.addProperty("content", content);
        if (messageType != null) o.addProperty("message_type", messageType);
        try (Response r = postJson("/api/send-topic-message", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Pin/unpin a topic
     */
    public JsonObject pinGroupTopic(String token, String topicId, int order) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        o.addProperty("order", String.valueOf(order));
        try (Response r = postJson("/api/pin-group-topic", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Unpin a topic
     */
    public JsonObject unpinGroupTopic(String token, String topicId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        try (Response r = postJson("/api/unpin-group-topic", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Close/reopen a topic
     */
    public JsonObject toggleTopicClosed(String token, String topicId, boolean closed) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("topic_id", topicId);
        o.addProperty("is_closed", String.valueOf(closed));
        try (Response r = postJson("/api/update-group-topic", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Set forum mode for a group
     */
    public JsonObject setGroupForumMode(String token, String groupId, boolean enabled) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("group_id", groupId);
        o.addProperty("enabled", String.valueOf(enabled));
        try (Response r = postJson("/api/set-group-forum-mode", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Remove a friend
     */
    public JsonObject removeFriend(String token, String userId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("user_id", userId);
        try (Response r = postJson("/api/remove-friend", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Clear chat history
     */
    public JsonObject clearChatHistory(String token, String chatId) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("chat_id", chatId);
        try (Response r = postJson("/api/clear-chat", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }

    /**
     * Report a user
     */
    public JsonObject reportUser(String token, String userId, String reason) throws IOException {
        JsonObject o = new JsonObject();
        o.addProperty("token", token);
        o.addProperty("user_id", userId);
        o.addProperty("reason", reason);
        try (Response r = postJson("/api/report-user", o)) {
            if (!r.isSuccessful()) throw new IOException("HTTP " + r.code());
            return gson.fromJson(r.body().string(), JsonObject.class);
        }
    }
}
