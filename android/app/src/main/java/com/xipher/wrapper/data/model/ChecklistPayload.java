package com.xipher.wrapper.data.model;

import java.util.List;

public class ChecklistPayload {
    public String id;
    public String title;
    public boolean othersCanAdd;
    public boolean othersCanMark;
    public List<ChecklistItem> items;

    public ChecklistPayload() {}
}
