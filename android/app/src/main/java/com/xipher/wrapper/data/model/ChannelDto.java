package com.xipher.wrapper.data.model;

public class ChannelDto {
    public String id;
    public String name;
    public String description;
    public String custom_link;
    @com.google.gson.annotations.SerializedName("avatar_url")
    public String avatar_url;
    public boolean is_private;
    public boolean show_author;
    public String created_at;
}
