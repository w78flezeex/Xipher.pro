package com.xipher.wrapper.data.db.entity;

import androidx.annotation.NonNull;
import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "users")
public class UserEntity {
    @PrimaryKey
    @NonNull
    public String id;

    public String username;
    public String avatarUrl;
    public boolean isFriend;
    public long lastActivity;
}
