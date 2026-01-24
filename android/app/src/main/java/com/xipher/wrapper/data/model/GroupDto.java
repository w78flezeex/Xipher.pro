package com.xipher.wrapper.data.model;

import java.util.List;

public class GroupDto {
    public String id;
    public String name;
    public String description;
    public String creator_id;
    public String created_at;
    public String avatar_url;
    
    // Forum mode support (Telegram-style topics)
    public boolean forum_mode;
    public List<TopicDto> topics;
    
    // Member info
    public int member_count;
    public String user_role; // "creator", "admin", "member"
    
    /**
     * Check if group has forum mode enabled
     */
    public boolean isForumMode() {
        return forum_mode;
    }
    
    /**
     * Get topics list or empty list
     */
    public List<TopicDto> getTopics() {
        return topics != null ? topics : java.util.Collections.emptyList();
    }
}
