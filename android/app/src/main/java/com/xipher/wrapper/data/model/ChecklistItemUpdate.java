package com.xipher.wrapper.data.model;

public class ChecklistItemUpdate {
    public String id;
    public boolean done;

    public ChecklistItemUpdate() {}

    public ChecklistItemUpdate(String id, boolean done) {
        this.id = id;
        this.done = done;
    }
}
