package com.xipher.wrapper.util;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.data.model.ChecklistUpdatePayload;

public final class ChecklistCodec {
    public static final String CHECKLIST_PREFIX = "[[XIPHER_CHECKLIST]]";
    public static final String CHECKLIST_UPDATE_PREFIX = "[[XIPHER_CHECKLIST_UPDATE]]";

    private static final Gson GSON = new GsonBuilder().create();

    private ChecklistCodec() {}

    public static boolean isChecklistContent(String content) {
        return content != null && content.startsWith(CHECKLIST_PREFIX);
    }

    public static boolean isChecklistUpdateContent(String content) {
        return content != null && content.startsWith(CHECKLIST_UPDATE_PREFIX);
    }

    public static ChecklistPayload parseChecklist(String content) {
        if (!isChecklistContent(content)) return null;
        String json = content.substring(CHECKLIST_PREFIX.length()).trim();
        if (json.isEmpty()) return null;
        try {
            return GSON.fromJson(json, ChecklistPayload.class);
        } catch (Exception e) {
            return null;
        }
    }

    public static ChecklistUpdatePayload parseUpdate(String content) {
        if (!isChecklistUpdateContent(content)) return null;
        String json = content.substring(CHECKLIST_UPDATE_PREFIX.length()).trim();
        if (json.isEmpty()) return null;
        try {
            return GSON.fromJson(json, ChecklistUpdatePayload.class);
        } catch (Exception e) {
            return null;
        }
    }

    public static String encodeChecklist(ChecklistPayload payload) {
        if (payload == null) return null;
        return CHECKLIST_PREFIX + GSON.toJson(payload);
    }

    public static String encodeUpdate(ChecklistUpdatePayload payload) {
        if (payload == null) return null;
        return CHECKLIST_UPDATE_PREFIX + GSON.toJson(payload);
    }
}
