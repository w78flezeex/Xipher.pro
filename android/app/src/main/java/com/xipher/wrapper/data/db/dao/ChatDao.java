package com.xipher.wrapper.data.db.dao;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.OnConflictStrategy;
import androidx.room.Query;

import com.xipher.wrapper.data.db.entity.ChatEntity;

import java.util.List;

@Dao
public interface ChatDao {
    @Query("SELECT * FROM chats ORDER BY updatedAt DESC")
    List<ChatEntity> getAll();

    @Query("SELECT * FROM chats WHERE id = :chatId LIMIT 1")
    ChatEntity getById(String chatId);

    @Query("UPDATE chats SET lastMessage = :lastMessage, updatedAt = :updatedAt WHERE id = :chatId")
    void updatePreview(String chatId, String lastMessage, long updatedAt);

    @Query("UPDATE chats SET updatedAt = :updatedAt WHERE id = :chatId")
    void updateUpdatedAt(String chatId, long updatedAt);

    @Insert(onConflict = OnConflictStrategy.REPLACE)
    void upsertAll(List<ChatEntity> chats);

    @Query("DELETE FROM chats WHERE id = :chatId")
    void deleteById(String chatId);

    @Query("DELETE FROM chats")
    void clear();
}
