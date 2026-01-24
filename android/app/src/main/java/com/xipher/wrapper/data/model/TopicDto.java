package com.xipher.wrapper.data.model;

import java.util.List;

/**
 * Data class representing a forum topic (Telegram-style)
 */
public class TopicDto {
    public String id;
    public String group_id;
    public String name;
    public String icon_emoji;
    public String icon_color;
    public String creator_id;
    public String created_at;
    public String last_message_at;
    public int message_count;
    public int unread_count;
    public int pinned_order;
    public boolean is_general;
    public boolean is_closed;
    public boolean is_hidden;
    
    // Last message preview
    public String last_message_content;
    public String last_message_sender_name;
    
    /**
     * Default topic colors (Telegram-style)
     */
    public static final String[] TOPIC_COLORS = {
        "#6fb1fc", "#ffa44e", "#ff7a82", "#b691ff",
        "#ffbc5c", "#7ed7a8", "#ff87b0", "#8fd3ff"
    };
    
    /**
     * Default topic emojis
     */
    public static final String[] TOPIC_EMOJIS = {
        "💬", "📢", "❓", "💡", "🎯", "📌", "🔥", "⚡",
        "📚", "🎨", "🎮", "💻", "📷", "🎵", "🎬", "🏆",
        "✨", "💎", "🌟", "🚀", "🎁", "💪", "👋", "🔔",
        "📝", "💼", "🛠️", "🎓", "🌍", "💰", "📊", "🎪"
    };
    
    /**
     * Get display name with emoji
     */
    public String getDisplayName() {
        String emoji = icon_emoji != null ? icon_emoji : "💬";
        return emoji + " " + (name != null ? name : "Topic");
    }
    
    /**
     * Get icon color or default
     */
    public String getIconColor() {
        return icon_color != null ? icon_color : TOPIC_COLORS[0];
    }
    
    /**
     * Get icon emoji or default
     */
    public String getIconEmoji() {
        return icon_emoji != null ? icon_emoji : "💬";
    }
}
