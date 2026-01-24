package com.xipher.wrapper.data.model;

public class ChatDto {
    public String id;
    public String name;
    public String username;
    @com.google.gson.annotations.SerializedName(value = "last_message", alternate = {"lastMessage"})
    public String last_message;
    public int unread;
    public boolean is_saved_messages;
    public boolean is_channel;
    public boolean is_group;
    public boolean is_private;
    public long updated_at;
    @com.google.gson.annotations.SerializedName("avatar_url")
    public String avatar_url;
}
