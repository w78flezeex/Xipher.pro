package com.xipher.wrapper.data.db.dao;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import com.xipher.wrapper.data.db.entity.MessageEntity;

import java.util.List;

@Dao
public interface MessageDao {
    @Query("SELECT * FROM messages WHERE chatId = :chatId ORDER BY createdAtMillis ASC, createdAt ASC")
    List<MessageEntity> getByChat(String chatId);

    @Query("SELECT COUNT(*) FROM messages WHERE chatId = :chatId")
    int countByChat(String chatId);

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void upsertAll(List<MessageEntity> messages);

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void upsert(MessageEntity message);

    @Query("UPDATE messages SET status = :status WHERE id = :messageId")
    void updateStatus(String messageId, String status);

    @Query("UPDATE messages SET status = :status, readTimestamp = :readTimestamp WHERE id = :messageId")
    void updateStatusAndReadTimestamp(String messageId, String status, long readTimestamp);

    @Query("UPDATE messages SET readTimestamp = :readTimestamp WHERE id = :messageId")
    void updateReadTimestamp(String messageId, long readTimestamp);

    @Query("DELETE FROM messages WHERE id = :messageId")
    void deleteById(String messageId);

    @Query("DELETE FROM messages WHERE id IN (:messageIds)")
    void deleteByIds(List<String> messageIds);

    @Query("DELETE FROM messages WHERE chatId = :chatId")
    void clearChat(String chatId);

    @Query("DELETE FROM messages")
    void clearAll();
}
