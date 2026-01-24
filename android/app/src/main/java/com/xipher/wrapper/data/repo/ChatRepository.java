package com.xipher.wrapper.data.repo;

import android.content.Context;

import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.dao.ChatDao;
import com.xipher.wrapper.data.db.dao.MessageDao;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.data.model.ChatDto;
import com.xipher.wrapper.data.model.MessageDto;
import com.xipher.wrapper.util.ChecklistCodec;
import com.xipher.wrapper.util.SpoilerCodec;
import com.xipher.wrapper.security.SecurityContext;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.UUID;

public class ChatRepository {
    private final Context context;
    private final ChatDao chatDao;
    private final MessageDao messageDao;
    private final ApiClient api;

    public ChatRepository(Context context, String token) {
        this.context = context.getApplicationContext();
        AppDatabase db = null;
        try {
            db = AppDatabase.get(context);
        } catch (IllegalStateException e) {
            // Database not initialized
        }
        this.chatDao = db != null ? db.chatDao() : null;
        this.messageDao = db != null ? db.messageDao() : null;
        this.api = new ApiClient(token);
    }

    public List<ChatEntity> syncChats(String token) throws Exception {
        if (isPanicMode()) {
            return chatDao != null ? chatDao.getAll() : new ArrayList<>();
        }
        List<ChatEntity> entities = new ArrayList<>();
        List<ChatEntity> cached = chatDao != null ? chatDao.getAll() : new ArrayList<>();
        Map<String, ChatEntity> cachedById = new HashMap<>();
        for (ChatEntity c : cached) {
            if (c != null && c.id != null) {
                cachedById.put(c.id, c);
            }
        }
        List<ChatDto> remote = api.chats(token);
        for (ChatDto dto : remote) {
            ChatEntity cachedChat = cachedById.get(dto.id);
            ChatEntity e = new ChatEntity();
            e.id = dto.id;
            e.title = dto.name != null ? dto.name : dto.username;
            e.username = dto.username;
            e.avatarUrl = dto.avatar_url;
            e.lastMessage = resolveLastMessage(dto.last_message, cachedChat);
            e.unread = dto.unread;
            e.isChannel = dto.is_channel;
            e.isGroup = dto.is_group;
            e.isSaved = dto.is_saved_messages;
            e.isPrivate = dto.is_private;
            e.updatedAt = dto.updated_at;
            entities.add(e);
        }
        for (var group : api.groups(token)) {
            ChatEntity e = new ChatEntity();
            e.id = group.id;
            e.title = group.name != null ? group.name : context.getString(R.string.chat_type_group);
            e.username = null;
            e.avatarUrl = null;
            e.lastMessage = group.description != null ? group.description : "";
            e.unread = 0;
            e.isGroup = true;
            e.isChannel = false;
            e.isSaved = false;
            e.isPrivate = false;
            e.updatedAt = parseEpoch(group.created_at);
            ChatEntity cachedChat = cachedById.get(e.id);
            if (cachedChat != null) {
                if (cachedChat.updatedAt > 0) {
                    e.updatedAt = cachedChat.updatedAt;
                }
                String cachedPreview = resolveCachedLastMessage(cachedChat);
                if (cachedPreview != null && !cachedPreview.isEmpty()) {
                    e.lastMessage = cachedPreview;
                }
            }
            entities.add(e);
        }
        for (var channel : api.channels(token)) {
            ChatEntity e = new ChatEntity();
            e.id = channel.id;
            e.title = channel.name != null ? channel.name : context.getString(R.string.chat_type_channel);
            e.username = channel.custom_link;
            e.avatarUrl = channel.avatar_url;
            e.lastMessage = channel.description != null ? channel.description : "";
            e.unread = 0;
            e.isChannel = true;
            e.isGroup = false;
            e.isSaved = false;
            e.isPrivate = channel.is_private;
            e.updatedAt = parseEpoch(channel.created_at);
            ChatEntity cachedChat = cachedById.get(e.id);
            if (cachedChat != null) {
                if (cachedChat.updatedAt > 0) {
                    e.updatedAt = cachedChat.updatedAt;
                }
                String cachedPreview = resolveCachedLastMessage(cachedChat);
                if (cachedPreview != null && !cachedPreview.isEmpty()) {
                    e.lastMessage = cachedPreview;
                }
                if (e.avatarUrl == null || e.avatarUrl.isEmpty()) {
                    e.avatarUrl = cachedChat.avatarUrl;
                }
            }
            entities.add(e);
        }
        if (chatDao != null) chatDao.upsertAll(entities);
        return entities;
    }

    private String resolveLastMessage(String raw, ChatEntity cachedChat) {
        String content = raw != null ? raw : "";
        if (SpoilerCodec.isSpoilerContent(content)) {
            return context.getString(R.string.chat_preview_spoiler);
        }
        String preview = resolveChecklistPreview(content);
        if (preview != null) {
            if (ChecklistCodec.isChecklistUpdateContent(content)) {
                String cachedPreview = resolveCachedChecklistPreview(cachedChat);
                return cachedPreview != null ? cachedPreview : preview;
            }
            return preview;
        }
        if (content.isEmpty()) {
            String cachedPreview = resolveCachedLastMessage(cachedChat);
            return cachedPreview != null ? cachedPreview : "";
        }
        return content;
    }

    private String resolveCachedLastMessage(ChatEntity cachedChat) {
        if (cachedChat == null || cachedChat.lastMessage == null || cachedChat.lastMessage.isEmpty()) return null;
        String cached = cachedChat.lastMessage;
        if (SpoilerCodec.isSpoilerContent(cached)) {
            return context.getString(R.string.chat_preview_spoiler);
        }
        String preview = resolveChecklistPreview(cached);
        return preview != null ? preview : cached;
    }

    private String resolveCachedChecklistPreview(ChatEntity cachedChat) {
        if (cachedChat == null || cachedChat.lastMessage == null || cachedChat.lastMessage.isEmpty()) return null;
        return resolveChecklistPreview(cachedChat.lastMessage);
    }

    private String resolveChecklistPreview(String content) {
        if (content == null) return null;
        if (ChecklistCodec.isChecklistContent(content)) {
            ChecklistPayload payload = ChecklistCodec.parseChecklist(content);
            if (payload != null && payload.title != null && !payload.title.isEmpty()) {
                return context.getString(R.string.chat_preview_checklist) + ": " + payload.title;
            }
            return context.getString(R.string.chat_preview_checklist);
        }
        if (ChecklistCodec.isChecklistUpdateContent(content)) {
            return context.getString(R.string.chat_preview_checklist);
        }
        String label = context.getString(R.string.chat_preview_checklist);
        if (content.equals(label) || content.startsWith(label + ": ")) {
            return content;
        }
        return null;
    }

    public List<MessageEntity> syncMessages(String token, String chatId) throws Exception {
        if (isPanicMode()) {
            return messageDao != null ? messageDao.getByChat(chatId) : new ArrayList<>();
        }
        List<MessageDto> remote = ensureAscendingOrder(api.messages(token, chatId));
        List<MessageEntity> entities = new ArrayList<>();
        Map<String, String> statusById = new HashMap<>();
        List<MessageEntity> cachedList = messageDao != null ? messageDao.getByChat(chatId) : new ArrayList<>();
        for (MessageEntity cached : cachedList) {
            if (cached == null) continue;
            if (cached.id != null) {
                statusById.put(cached.id, cached.status);
            }
            if (cached.tempId != null) {
                statusById.put(cached.tempId, cached.status);
            }
        }
        for (MessageDto dto : remote) {
            MessageEntity m = new MessageEntity();
            m.id = dto.id != null ? dto.id : (dto.message_id != null ? dto.message_id : dto.temp_id);
            m.tempId = dto.temp_id;
            m.chatId = chatId;
            m.senderId = dto.sender_id;
            m.senderName = dto.sender_username;
            m.content = dto.content;
            m.messageType = dto.message_type;
            m.filePath = dto.file_path;
            m.fileName = dto.file_name;
            m.fileSize = dto.file_size;
            m.replyToMessageId = dto.reply_to_message_id;
            m.replyContent = dto.reply_content;
            m.replySenderName = dto.reply_sender_name;
            m.createdAt = dto.created_at != null ? dto.created_at : dto.time;
            m.createdAtMillis = parseEpoch(m.createdAt);
            m.ttlSeconds = dto.ttl_seconds != null ? dto.ttl_seconds : 0L;
            m.readTimestamp = dto.read_timestamp != null ? parseEpoch(String.valueOf(dto.read_timestamp)) : 0L;
            String cachedStatus = statusById.get(m.id);
            if (cachedStatus == null && m.tempId != null) {
                cachedStatus = statusById.get(m.tempId);
            }
            String serverStatus = resolveMessageStatus(dto);
            m.status = mergeStatus(cachedStatus, serverStatus);
            entities.add(m);
        }
        if (messageDao != null) messageDao.upsertAll(entities);
        return entities;
    }

    public List<MessageEntity> syncGroupMessages(String token, String groupId) throws Exception {
        if (isPanicMode()) {
            return messageDao != null ? messageDao.getByChat(groupId) : new ArrayList<>();
        }
        List<MessageDto> remote = ensureAscendingOrder(api.groupMessages(token, groupId));
        List<MessageEntity> entities = new ArrayList<>();
        Map<String, String> statusById = new HashMap<>();
        List<MessageEntity> groupCachedList = messageDao != null ? messageDao.getByChat(groupId) : new ArrayList<>();
        for (MessageEntity cached : groupCachedList) {
            if (cached == null) continue;
            if (cached.id != null) {
                statusById.put(cached.id, cached.status);
            }
            if (cached.tempId != null) {
                statusById.put(cached.tempId, cached.status);
            }
        }
        for (MessageDto dto : remote) {
            MessageEntity m = new MessageEntity();
            m.id = dto.id != null ? dto.id : (dto.message_id != null ? dto.message_id : dto.temp_id);
            m.tempId = dto.temp_id;
            m.chatId = groupId;
            m.senderId = dto.sender_id;
            m.senderName = dto.sender_username;
            m.content = dto.content;
            m.messageType = dto.message_type;
            m.filePath = dto.file_path;
            m.fileName = dto.file_name;
            m.fileSize = dto.file_size;
            m.replyToMessageId = dto.reply_to_message_id;
            m.replyContent = dto.reply_content;
            m.replySenderName = dto.reply_sender_name;
            m.createdAt = dto.created_at != null ? dto.created_at : dto.time;
            m.createdAtMillis = parseEpoch(m.createdAt);
            m.ttlSeconds = dto.ttl_seconds != null ? dto.ttl_seconds : 0L;
            m.readTimestamp = dto.read_timestamp != null ? parseEpoch(String.valueOf(dto.read_timestamp)) : 0L;
            String cachedStatus = statusById.get(m.id);
            if (cachedStatus == null && m.tempId != null) {
                cachedStatus = statusById.get(m.tempId);
            }
            String serverStatus = resolveMessageStatus(dto);
            m.status = mergeStatus(cachedStatus, serverStatus);
            entities.add(m);
        }
        if (messageDao != null) messageDao.upsertAll(entities);
        return entities;
    }

    public List<MessageEntity> syncChannelMessages(String token, String channelId) throws Exception {
        if (isPanicMode()) {
            return messageDao != null ? messageDao.getByChat(channelId) : new ArrayList<>();
        }
        List<MessageDto> remote = ensureAscendingOrder(api.channelMessages(token, channelId));
        List<MessageEntity> entities = new ArrayList<>();
        Map<String, String> statusById = new HashMap<>();
        List<MessageEntity> channelCachedList = messageDao != null ? messageDao.getByChat(channelId) : new ArrayList<>();
        for (MessageEntity cached : channelCachedList) {
            if (cached == null) continue;
            if (cached.id != null) {
                statusById.put(cached.id, cached.status);
            }
            if (cached.tempId != null) {
                statusById.put(cached.tempId, cached.status);
            }
        }
        for (MessageDto dto : remote) {
            MessageEntity m = new MessageEntity();
            m.id = dto.id != null ? dto.id : (dto.message_id != null ? dto.message_id : dto.temp_id);
            m.tempId = dto.temp_id;
            m.chatId = channelId;
            m.senderId = dto.sender_id;
            m.senderName = dto.sender_username;
            m.content = dto.content;
            m.messageType = dto.message_type;
            m.filePath = dto.file_path;
            m.fileName = dto.file_name;
            m.fileSize = dto.file_size;
            m.replyToMessageId = dto.reply_to_message_id;
            m.replyContent = dto.reply_content;
            m.replySenderName = dto.reply_sender_name;
            m.createdAt = dto.created_at != null ? dto.created_at : dto.time;
            m.createdAtMillis = parseEpoch(m.createdAt);
            m.ttlSeconds = dto.ttl_seconds != null ? dto.ttl_seconds : 0L;
            m.readTimestamp = dto.read_timestamp != null ? parseEpoch(String.valueOf(dto.read_timestamp)) : 0L;
            String cachedStatus = statusById.get(m.id);
            if (cachedStatus == null && m.tempId != null) {
                cachedStatus = statusById.get(m.tempId);
            }
            String serverStatus = resolveMessageStatus(dto);
            m.status = mergeStatus(cachedStatus, serverStatus);
            entities.add(m);
        }
        if (messageDao != null) messageDao.upsertAll(entities);
        return entities;
    }

    public MessageEntity sendMessage(String token, String chatId, String content, String messageType, String replyTo, String senderId) throws Exception {
        return sendMessage(token, chatId, content, messageType, replyTo, senderId, null, null, null, 0L);
    }

    public MessageEntity sendMessage(String token, String chatId, String content, String messageType, String replyTo,
                                     String senderId, String filePath, String fileName, Long fileSize) throws Exception {
        return sendMessage(token, chatId, content, messageType, replyTo, senderId, filePath, fileName, fileSize, 0L);
    }

    public MessageEntity sendMessage(String token, String chatId, String content, String messageType, String replyTo,
                                     String senderId, String filePath, String fileName, Long fileSize, long ttlSeconds) throws Exception {
        if (isPanicMode()) {
            MessageEntity m = buildLocalMessage(chatId, senderId, null, content, messageType, filePath, fileName, fileSize, ttlSeconds);
            if (messageDao != null) messageDao.upsert(m);
            return m;
        }
        JsonObject body = new JsonObject();
        body.addProperty("receiver_id", chatId);
        body.addProperty("content", content);
        body.addProperty("message_type", messageType);
        if (replyTo != null) body.addProperty("reply_to_message_id", replyTo);
        if (filePath != null) body.addProperty("file_path", filePath);
        if (fileName != null) body.addProperty("file_name", fileName);
        if (fileSize != null) body.addProperty("file_size", fileSize);
        if (ttlSeconds > 0L) body.addProperty("ttl_seconds", ttlSeconds);
        JsonObject res = api.sendMessage(token, body);
        ensureSuccess(res);
        MessageEntity m = new MessageEntity();
        m.id = extractMessageId(res);
        if (m.id == null || m.id.isEmpty()) {
            m.id = "temp-" + System.currentTimeMillis();
        }
        m.chatId = chatId;
        m.senderId = senderId;
        m.senderName = null;
        m.content = content;
        m.messageType = messageType;
        m.filePath = filePath;
        m.fileName = fileName;
        m.fileSize = fileSize;
        String createdAt = res.has("created_at") ? res.get("created_at").getAsString() : null;
        if (createdAt == null || createdAt.isEmpty()) {
            createdAt = nowIso();
        }
        m.createdAt = createdAt;
        m.createdAtMillis = parseEpoch(createdAt);
        if (m.createdAtMillis <= 0L) {
            m.createdAtMillis = System.currentTimeMillis();
        }
        m.ttlSeconds = Math.max(0L, ttlSeconds);
        m.readTimestamp = 0L;
        m.status = "sent";
        if (messageDao != null) messageDao.upsert(m);
        return m;
    }

    public MessageEntity sendGroupMessage(String token, String groupId, String content, String messageType, String replyTo,
                                          String senderId, String senderName, String filePath, String fileName, Long fileSize) throws Exception {
        return sendGroupMessage(token, groupId, content, messageType, replyTo, senderId, senderName, filePath, fileName, fileSize, 0L);
    }

    public MessageEntity sendGroupMessage(String token, String groupId, String content, String messageType, String replyTo,
                                          String senderId, String senderName, String filePath, String fileName, Long fileSize, long ttlSeconds) throws Exception {
        if (isPanicMode()) {
            MessageEntity m = buildLocalMessage(groupId, senderId, senderName, content, messageType, filePath, fileName, fileSize, ttlSeconds);
            if (messageDao != null) messageDao.upsert(m);
            return m;
        }
        JsonObject body = new JsonObject();
        body.addProperty("group_id", groupId);
        body.addProperty("content", content);
        body.addProperty("message_type", messageType);
        if (replyTo != null) body.addProperty("reply_to_message_id", replyTo);
        if (filePath != null) body.addProperty("file_path", filePath);
        if (fileName != null) body.addProperty("file_name", fileName);
        if (fileSize != null) body.addProperty("file_size", fileSize);
        if (ttlSeconds > 0L) body.addProperty("ttl_seconds", ttlSeconds);
        JsonObject res = api.sendGroupMessage(token, body);
        ensureSuccess(res);
        MessageEntity m = new MessageEntity();
        m.id = extractMessageId(res);
        if (m.id == null || m.id.isEmpty()) {
            m.id = "temp-" + System.currentTimeMillis();
        }
        m.chatId = groupId;
        m.senderId = senderId;
        m.senderName = senderName;
        m.content = content;
        m.messageType = messageType;
        m.filePath = filePath;
        m.fileName = fileName;
        m.fileSize = fileSize;
        String createdAt = res.has("created_at") ? res.get("created_at").getAsString() : null;
        if (createdAt == null || createdAt.isEmpty()) {
            createdAt = nowIso();
        }
        m.createdAt = createdAt;
        m.createdAtMillis = parseEpoch(createdAt);
        if (m.createdAtMillis <= 0L) {
            m.createdAtMillis = System.currentTimeMillis();
        }
        m.ttlSeconds = Math.max(0L, ttlSeconds);
        m.readTimestamp = 0L;
        m.status = "sent";
        if (messageDao != null) messageDao.upsert(m);
        return m;
    }

    public MessageEntity sendChannelMessage(String token, String channelId, String content, String messageType, String replyTo,
                                            String senderId, String senderName, String filePath, String fileName, Long fileSize) throws Exception {
        return sendChannelMessage(token, channelId, content, messageType, replyTo, senderId, senderName, filePath, fileName, fileSize, 0L);
    }

    public MessageEntity sendChannelMessage(String token, String channelId, String content, String messageType, String replyTo,
                                            String senderId, String senderName, String filePath, String fileName, Long fileSize, long ttlSeconds) throws Exception {
        if (isPanicMode()) {
            MessageEntity m = buildLocalMessage(channelId, senderId, senderName, content, messageType, filePath, fileName, fileSize, ttlSeconds);
            if (messageDao != null) messageDao.upsert(m);
            return m;
        }
        JsonObject body = new JsonObject();
        body.addProperty("channel_id", channelId);
        body.addProperty("content", content);
        body.addProperty("message_type", messageType);
        if (replyTo != null) body.addProperty("reply_to_message_id", replyTo);
        if (filePath != null) body.addProperty("file_path", filePath);
        if (fileName != null) body.addProperty("file_name", fileName);
        if (fileSize != null) body.addProperty("file_size", fileSize);
        if (ttlSeconds > 0L) body.addProperty("ttl_seconds", ttlSeconds);
        JsonObject res = api.sendChannelMessage(token, body);
        ensureSuccess(res);
        MessageEntity m = new MessageEntity();
        m.id = extractMessageId(res);
        if (m.id == null || m.id.isEmpty()) {
            m.id = "temp-" + System.currentTimeMillis();
        }
        m.chatId = channelId;
        m.senderId = senderId;
        m.senderName = senderName;
        m.content = content;
        m.messageType = messageType;
        m.filePath = filePath;
        m.fileName = fileName;
        m.fileSize = fileSize;
        String createdAt = res.has("created_at") ? res.get("created_at").getAsString() : null;
        if (createdAt == null || createdAt.isEmpty()) {
            createdAt = nowIso();
        }
        m.createdAt = createdAt;
        m.createdAtMillis = parseEpoch(createdAt);
        if (m.createdAtMillis <= 0L) {
            m.createdAtMillis = System.currentTimeMillis();
        }
        m.ttlSeconds = Math.max(0L, ttlSeconds);
        m.readTimestamp = 0L;
        m.status = "sent";
        if (messageDao != null) messageDao.upsert(m);
        return m;
    }

    public void deleteMessage(String token, String messageId) throws Exception {
        if (isPanicMode()) {
            if (messageDao != null) messageDao.deleteById(messageId);
            return;
        }
        JsonObject res = api.deleteMessage(token, messageId);
        if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) {
            throw new IllegalStateException("delete failed");
        }
        if (messageDao != null) messageDao.deleteById(messageId);
    }

    public void deleteGroupMessage(String token, String groupId, String messageId) throws Exception {
        if (isPanicMode()) {
            if (messageDao != null) messageDao.deleteById(messageId);
            return;
        }
        JsonObject res = api.deleteGroupMessage(token, groupId, messageId);
        if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) {
            throw new IllegalStateException("delete failed");
        }
        if (messageDao != null) messageDao.deleteById(messageId);
    }

    public void deleteChannelMessage(String token, String channelId, String messageId) throws Exception {
        if (isPanicMode()) {
            if (messageDao != null) messageDao.deleteById(messageId);
            return;
        }
        JsonObject res = api.deleteChannelMessage(token, channelId, messageId);
        if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) {
            throw new IllegalStateException("delete failed");
        }
        if (messageDao != null) messageDao.deleteById(messageId);
    }

    private static String nowIso() {
        java.text.SimpleDateFormat fmt = new java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", java.util.Locale.US);
        return fmt.format(new java.util.Date());
    }

    private MessageEntity buildLocalMessage(String chatId, String senderId, String senderName, String content,
                                            String messageType, String filePath, String fileName, Long fileSize,
                                            long ttlSeconds) {
        MessageEntity m = new MessageEntity();
        m.id = "decoy-" + UUID.randomUUID().toString();
        m.tempId = m.id;
        m.chatId = chatId;
        m.senderId = senderId;
        m.senderName = senderName;
        m.content = content != null ? content : "";
        m.messageType = messageType != null ? messageType : "text";
        m.filePath = filePath;
        m.fileName = fileName;
        m.fileSize = fileSize;
        m.createdAt = nowIso();
        m.createdAtMillis = System.currentTimeMillis();
        m.ttlSeconds = Math.max(0L, ttlSeconds);
        m.readTimestamp = 0L;
        m.status = "sent";
        return m;
    }

    private boolean isPanicMode() {
        return SecurityContext.get().isPanicMode();
    }

    private static void ensureSuccess(JsonObject res) {
        if (res != null && res.has("success") && !res.get("success").getAsBoolean()) {
            String msg = res.has("message") ? res.get("message").getAsString() : "Request failed";
            throw new IllegalStateException(msg);
        }
    }

    private static String extractMessageId(JsonObject res) {
        if (res == null) return null;
        if (res.has("message_id")) return res.get("message_id").getAsString();
        if (res.has("data") && res.get("data").isJsonObject()) {
            JsonObject data = res.getAsJsonObject("data");
            if (data.has("message_id")) return data.get("message_id").getAsString();
        }
        return null;
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

    private static String resolveMessageStatus(MessageDto dto) {
        if (dto == null) return "sent";
        if (dto.status != null && !dto.status.isEmpty()) return dto.status;
        if (Boolean.TRUE.equals(dto.is_read)) return "read";
        if (Boolean.TRUE.equals(dto.is_delivered)) return "delivered";
        if (Boolean.TRUE.equals(dto.sent)) return "sent";
        return "sent";
    }

    private static String mergeStatus(String cachedStatus, String serverStatus) {
        if (serverStatus == null || serverStatus.isEmpty()) {
            return cachedStatus != null ? cachedStatus : "sent";
        }
        if (cachedStatus == null || cachedStatus.isEmpty()) {
            return serverStatus;
        }
        return statusRank(serverStatus) > statusRank(cachedStatus) ? serverStatus : cachedStatus;
    }

    private static int statusRank(String status) {
        if (status == null) return 0;
        switch (status) {
            case "read":
                return 4;
            case "delivered":
                return 3;
            case "sent":
                return 2;
            case "pending":
                return 1;
            default:
                return 0;
        }
    }

    private List<MessageDto> ensureAscendingOrder(List<MessageDto> remote) {
        if (remote == null || remote.size() <= 1) return remote;
        boolean hasParsedTimestamp = false;
        for (MessageDto dto : remote) {
            if (dto == null) continue;
            String raw = dto.created_at != null ? dto.created_at : dto.time;
            if (parseEpoch(raw) > 0L) {
                hasParsedTimestamp = true;
                break;
            }
        }
        if (!hasParsedTimestamp) {
            List<MessageDto> copy = new ArrayList<>(remote);
            Collections.reverse(copy);
            return copy;
        }
        return remote;
    }

    public JsonObject uploadFile(String token, String fileName, String fileData) throws Exception {
        return api.uploadFile(token, fileName, fileData);
    }

    public okhttp3.Call uploadFileCall(String token, String fileName, String fileData, ApiClient.ProgressListener listener) {
        return api.uploadFileCall(token, fileName, fileData, listener);
    }

    public JsonObject uploadVoice(String token, String voiceData, String mimeType) throws Exception {
        return api.uploadVoice(token, voiceData, mimeType);
    }
}
