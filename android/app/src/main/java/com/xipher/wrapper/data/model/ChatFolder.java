package com.xipher.wrapper.data.model;

import com.google.gson.annotations.SerializedName;

import java.util.List;

public class ChatFolder {
    @SerializedName("id")
    public String id;

    @SerializedName("name")
    public String name;

    @SerializedName(value = "chat_keys", alternate = {"chatKeys"})
    public List<String> chatKeys;

    public ChatFolder() {}

    public ChatFolder(String id, String name, List<String> chatKeys) {
        this.id = id;
        this.name = name;
        this.chatKeys = chatKeys;
    }
}
