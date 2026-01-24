package com.xipher.wrapper.data.db.entity;

import androidx.annotation.NonNull;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "chats")
public class ChatEntity {
    @PrimaryKey
    @NonNull
    public String id;

    public String title;
    public String username;
    public String avatarUrl;
    public String lastMessage;
    public int unread;
    public boolean isChannel;
    public boolean isGroup;
    public boolean isSaved;
    public boolean isPrivate;
    public long updatedAt;
}
