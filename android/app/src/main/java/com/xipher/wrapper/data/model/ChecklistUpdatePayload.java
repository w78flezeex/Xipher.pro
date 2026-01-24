package com.xipher.wrapper.data.model;

import java.util.List;

public class ChecklistUpdatePayload {
    public String checklistId;
    public List<ChecklistItemUpdate> updates;
    public List<ChecklistItem> added;

    public ChecklistUpdatePayload() {}
}
