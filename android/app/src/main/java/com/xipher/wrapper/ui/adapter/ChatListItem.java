package com.xipher.wrapper.ui.adapter;

import com.xipher.wrapper.data.db.entity.ChatEntity;

/**
 * Wrapper class for chat list items that can be either a chat or a divider
 */
public class ChatListItem {
    
    public static final int TYPE_CHAT = 0;
    public static final int TYPE_UNREAD_DIVIDER = 1;
    
    private final int type;
    private final ChatEntity chat;
    
    private ChatListItem(int type, ChatEntity chat) {
        this.type = type;
        this.chat = chat;
    }
    
    public static ChatListItem chat(ChatEntity chat) {
        return new ChatListItem(TYPE_CHAT, chat);
    }
    
    public static ChatListItem unreadDivider() {
        return new ChatListItem(TYPE_UNREAD_DIVIDER, null);
    }
    
    public int getType() {
        return type;
    }
    
    public ChatEntity getChat() {
        return chat;
    }
    
    public boolean isChat() {
        return type == TYPE_CHAT;
    }
    
    public boolean isDivider() {
        return type == TYPE_UNREAD_DIVIDER;
    }
    
    /**
     * Unique ID for DiffUtil
     */
    public String getId() {
        if (type == TYPE_UNREAD_DIVIDER) {
            return "__UNREAD_DIVIDER__";
        }
        return chat != null ? chat.id : "";
    }
    
    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        ChatListItem that = (ChatListItem) o;
        if (type != that.type) return false;
        if (type == TYPE_UNREAD_DIVIDER) return true;
        if (chat == null) return that.chat == null;
        return chat.id.equals(that.chat.id) &&
               chat.unread == that.chat.unread &&
               safeEq(chat.title, that.chat.title) &&
               safeEq(chat.lastMessage, that.chat.lastMessage) &&
               safeEq(chat.avatarUrl, that.chat.avatarUrl);
    }
    
    private boolean safeEq(String a, String b) {
        return a == null ? b == null : a.equals(b);
    }
    
    @Override
    public int hashCode() {
        if (type == TYPE_UNREAD_DIVIDER) return -1;
        return chat != null ? chat.id.hashCode() : 0;
    }
}
