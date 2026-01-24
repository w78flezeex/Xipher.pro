package com.xipher.wrapper.data.db.entity;

import androidx.annotation.NonNull;
import androidx.room.Entity;
import androidx.room.ForeignKey;
import androidx.room.Index;
import androidx.room.PrimaryKey;

@Entity(
        tableName = "messages",
        foreignKeys = @ForeignKey(
                entity = ChatEntity.class,
                parentColumns = "id",
                childColumns = "chatId",
                onDelete = ForeignKey.CASCADE
        ),
        indices = {@Index("chatId")}
)
public class MessageEntity {
    @PrimaryKey
    @NonNull
    public String id;

    public String tempId;
    public String chatId;
    public String senderId;
    public String senderName;
    public String content;
    public String messageType;
    public String filePath;
    public String fileName;
    public Long fileSize;
    public String replyToMessageId;
    public String replyContent;
    public String replySenderName;
    public String createdAt;
    public long createdAtMillis;
    public long ttlSeconds;
    public long readTimestamp;
    public String status; // pending, sent, delivered, read, failed
}
