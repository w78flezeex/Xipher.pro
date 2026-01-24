package com.xipher.wrapper.data.model;

public class ChecklistItem {
    public String id;
    public String text;
    public boolean done;

    public ChecklistItem() {}

    public ChecklistItem(String id, String text, boolean done) {
        this.id = id;
        this.text = text;
        this.done = done;
    }
}
