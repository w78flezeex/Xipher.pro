package com.xipher.wrapper.data.model;

public class FriendDto {
    public String id;
    public String username;
    @com.google.gson.annotations.SerializedName("avatar_url")
    public String avatar_url;
    public boolean is_bot;
    public boolean online;
    public String last_activity;
    public String created_at;
}
